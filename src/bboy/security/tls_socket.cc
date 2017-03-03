#include "bboy/security/tls_socket.h"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "bboy/gbase/basictypes.h"
#include "bboy/security/cert.h"
#include "bboy/security/openssl_util.h"
#include "bboy/base/errno.h"

namespace bb {
namespace security {

TlsSocket::TlsSocket(int fd, c_unique_ptr<SSL> ssl)
    : Socket(fd),
      ssl_(std::move(ssl)) {
}

TlsSocket::~TlsSocket() {
  ignore_result(Close());
}

Status TlsSocket::Write(const uint8_t *buf, size_t amt, size_t *nwritten) {
  CHECK(ssl_);

  if (PREDICT_FALSE(amt == 0)) {
    // Writing an empty buffer is a no-op. This happens occasionally, eg in the
    // case where the response has an empty sidecar. We have to special case
    // it, because SSL_write can return '0' to indicate certain types of errors.
    *nwritten = 0;
    return Status::OK();
  }

  ERR_clear_error();
  errno = 0;
  int32_t bytes_written = SSL_write(ssl_.get(), buf, amt);
  if (bytes_written <= 0) {
    auto error_code = SSL_get_error(ssl_.get(), bytes_written);
    if (error_code == SSL_ERROR_WANT_WRITE) {
      // Socket not ready to write yet.
      *nwritten = 0;
      return Status::OK();
    }
    return Status::NetworkError("failed to write to TLS socket",
                                GetSSLErrorDescription(error_code));
  }
  *nwritten = bytes_written;
  return Status::OK();
}

Status TlsSocket::Writev(const struct ::iovec *iov, int iov_len, size_t *nwritten) {
  CHECK(ssl_);
  ERR_clear_error();
  int32_t total_written = 0;
  // Allows packets to be aggresively be accumulated before sending.
  RETURN_NOT_OK(SetTcpCork(1));
  Status write_status = Status::OK();
  for (int i = 0; i < iov_len; ++i) {
    int32_t frame_size = iov[i].iov_len;
    // Don't return before unsetting TCP_CORK.
    write_status = Write(static_cast<uint8_t*>(iov[i].iov_base), frame_size, nwritten);
    total_written += *nwritten;
    if (*nwritten < frame_size) break;
  }
  RETURN_NOT_OK(SetTcpCork(0));
  *nwritten = total_written;
  return write_status;
}

Status TlsSocket::Read(uint8_t *buf, size_t amt, size_t *nread) {
  const char* kErrString = "failed to read from TLS socket";

  CHECK(ssl_);
  ERR_clear_error();
  errno = 0;
  int32_t bytes_read = SSL_read(ssl_.get(), buf, amt);
  int save_errno = errno;
  if (bytes_read <= 0) {
    if (bytes_read == 0 && SSL_get_shutdown(ssl_.get()) == SSL_RECEIVED_SHUTDOWN) {
      return Status::NetworkError(kErrString, ErrnoToString(ESHUTDOWN), ESHUTDOWN);
    }
    auto error_code = SSL_get_error(ssl_.get(), bytes_read);
    if (error_code == SSL_ERROR_WANT_READ) {
      // Nothing available to read yet.
      *nread = 0;
      return Status::OK();
    }
    if (error_code == SSL_ERROR_SYSCALL && ERR_peek_error() == 0) {
      // From the OpenSSL docs:
      //   Some I/O error occurred.  The OpenSSL error queue may contain more
      //   information on the error.  If the error queue is empty (i.e.
      //   ERR_get_error() returns 0), ret can be used to find out more about
      //   the error: If ret == 0, an EOF was observed that violates the pro-
      //   tocol.  If ret == -1, the underlying BIO reported an I/O error (for
      //   socket I/O on Unix systems, consult errno for details).
      if (bytes_read == 0) {
        // "EOF was observed that violates the protocol" (eg the other end disconnected)
        return Status::NetworkError(kErrString, ErrnoToString(ECONNRESET), ECONNRESET);
      }
      if (bytes_read == -1 && save_errno != 0) {
        return Status::NetworkError(kErrString, ErrnoToString(save_errno), save_errno);
      }
      return Status::NetworkError(kErrString, "unknown ERROR_SYSCALL");
    }
    return Status::NetworkError(kErrString, GetSSLErrorDescription(error_code));
  }
  *nread = bytes_read;
  return Status::OK();
}

Status TlsSocket::Close() {
  ERR_clear_error();
  errno = 0;

  if (!ssl_) {
    // Socket is already closed.
    return Status::OK();
  }

  // Start the TLS shutdown processes. We don't care about waiting for the
  // response, since the underlying socket will not be reused.
  int32_t ret = SSL_shutdown(ssl_.get());
  Status ssl_shutdown;
  if (ret >= 0) {
    ssl_shutdown = Status::OK();
  } else {
    auto error_code = SSL_get_error(ssl_.get(), ret);
    ssl_shutdown = Status::NetworkError("TlsSocket::Close", GetSSLErrorDescription(error_code));
  }

  ssl_.reset();
  ERR_remove_state(0);

  // Close the underlying socket.
  RETURN_NOT_OK(Socket::Close());
  return ssl_shutdown;
}

} // namespace security
} // namespace bb
