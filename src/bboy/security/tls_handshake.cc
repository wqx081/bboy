#include "bboy/security/tls_handshake.h"

#include <memory>
#include <string>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "bboy/security/cert.h"
#include "bboy/security/tls_socket.h"
#include "bboy/base/net/sockaddr.h"
#include "bboy/base/status.h"

#if OPENSSL_VERSION_NUMBER < 0x10002000L
#include "bboy/security/x509_check_host.h"
#endif // OPENSSL_VERSION_NUMBER

using std::string;
using std::unique_ptr;

namespace bb {
namespace security {

void TlsHandshake::SetSSLVerify() {
  CHECK(ssl_);
  CHECK(!has_started_);
  int ssl_mode = 0;
  switch (verification_mode_) {
    case TlsVerificationMode::VERIFY_NONE:
      ssl_mode = SSL_VERIFY_NONE;
      break;
    case TlsVerificationMode::VERIFY_REMOTE_CERT_AND_HOST:
      // Server mode: the server sends a client certificate request to the client. The
      // certificate returned (if any) is checked. If the verification process fails, the TLS/SSL
      // handshake is immediately terminated with an alert message containing the reason for the
      // verification failure. The behaviour can be controlled by the additional
      // SSL_VERIFY_FAIL_IF_NO_PEER_CERT and SSL_VERIFY_CLIENT_ONCE flags.

      // Client mode: the server certificate is verified. If the verification process fails, the
      // TLS/SSL handshake is immediately terminated with an alert message containing the reason
      // for the verification failure. If no server certificate is sent, because an anonymous
      // cipher is used, SSL_VERIFY_PEER is ignored.
      ssl_mode |= SSL_VERIFY_PEER;

      // Server mode: if the client did not return a certificate, the TLS/SSL handshake is
      // immediately terminated with a "handshake failure" alert. This flag must be used
      // together with SSL_VERIFY_PEER.
      ssl_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
      // Server mode: only request a client certificate on the initial TLS/SSL handshake. Do
      // not ask for a client certificate again in case of a renegotiation. This flag must be
      // used together with SSL_VERIFY_PEER.
      ssl_mode |= SSL_VERIFY_CLIENT_ONCE;
      break;
  }

  SSL_set_verify(ssl_.get(), ssl_mode, /* callback = */nullptr);
}

Status TlsHandshake::Continue(const string& recv, string* send) {
  if (!has_started_) {
    SetSSLVerify();
    has_started_ = true;
  }
  CHECK(ssl_);
  ERR_clear_error();

  BIO* rbio = SSL_get_rbio(ssl_.get());
  int n = BIO_write(rbio, recv.data(), recv.size());
  DCHECK_EQ(n, recv.size());
  DCHECK_EQ(BIO_ctrl_pending(rbio), recv.size());

  int rc = SSL_do_handshake(ssl_.get());
  if (rc != 1) {
    int ssl_err = SSL_get_error(ssl_.get(), rc);
    // WANT_READ and WANT_WRITE indicate that the handshake is not yet complete.
    if (ssl_err != SSL_ERROR_WANT_READ && ssl_err != SSL_ERROR_WANT_WRITE) {
      return Status::RuntimeError("TLS Handshake error", GetSSLErrorDescription(ssl_err));
    }
  }

  BIO* wbio = SSL_get_wbio(ssl_.get());
  int pending = BIO_ctrl_pending(wbio);

  send->resize(pending);
  BIO_read(wbio, &(*send)[0], send->size());
  DCHECK_EQ(BIO_ctrl_pending(wbio), 0);

  if (rc == 1) {
    // The handshake is done, but in the case of the server, we still need to
    // send the final response to the client.
    DCHECK_GE(send->size(), 0);
    return Status::OK();
  }
  DCHECK_GT(send->size(), 0);
  return Status::Incomplete("TLS Handshake incomplete");
}

Status TlsHandshake::Verify(const Socket& socket) const {
  DCHECK(SSL_is_init_finished(ssl_.get()));
  CHECK(ssl_);

  if (verification_mode_ == TlsVerificationMode::VERIFY_NONE) {
    return Status::OK();
  }
  DCHECK(verification_mode_ == TlsVerificationMode::VERIFY_REMOTE_CERT_AND_HOST);

  ERR_clear_error();
  int rc = SSL_get_verify_result(ssl_.get());
  if (rc != X509_V_OK) {
    return Status::NotAuthorized("SSL_get_verify_result()", X509_verify_cert_error_string(rc));
  }

  // Get the peer certificate.
  X509* cert = remote_cert_.GetRawData();
  if (!cert) {
    if (SSL_get_verify_mode(ssl_.get()) & SSL_VERIFY_FAIL_IF_NO_PEER_CERT) {
      return Status::NotAuthorized("Handshake failed: unable to retreive peer certificate");
    }
    // No cert, but we weren't requiring one.
//    TRACE("Got no cert from peer, but not required");
    return Status::OK();
  }

  // TODO(PKI): KUDU-1886: Do hostname verification.
  /*
  TRACE("Verifying peer cert");

  // Get the peer's hostname
  Sockaddr peer_addr;
  if (!socket.GetPeerAddress(&peer_addr).ok()) {
    return Status::NotAuthorized(
        "TLS certificate hostname verification failed: unable to get peer address");
  }
  string peer_hostname;
  RETURN_NOT_OK_PREPEND(peer_addr.LookupHostname(&peer_hostname),
      "TLS certificate hostname verification failed: unable to lookup peer hostname");

  // Check if the hostname matches with either the Common Name or any of the Subject Alternative
  // Names of the certificate.
  int match = X509_check_host(cert,
                              peer_hostname.c_str(),
                              peer_hostname.length(),
                              0,
                              nullptr);
  if (match == 0) {
    return Status::NotAuthorized("TLS certificate hostname verification failed");
  }
  if (match < 0) {
    return Status::RuntimeError("TLS certificate hostname verification error", GetOpenSSLErrors());
  }
  DCHECK_EQ(match, 1);
  */
  return Status::OK();
}

Status TlsHandshake::GetCerts() {
  X509* cert = SSL_get_certificate(ssl_.get());
  if (cert) {
    // For whatever reason, SSL_get_certificate (unlike SSL_get_peer_certificate)
    // does not increment the X509's reference count.
    local_cert_.AdoptAndAddRefRawData(cert);
  }

  cert = SSL_get_peer_certificate(ssl_.get());
  if (cert) {
    remote_cert_.AdoptRawData(cert);
  }
  return Status::OK();
}

Status TlsHandshake::Finish(unique_ptr<Socket>* socket) {
  RETURN_NOT_OK(GetCerts());
  RETURN_NOT_OK(Verify(**socket));

  int fd = (*socket)->Release();

  // Give the socket to the SSL instance. This will automatically free the
  // read and write memory BIO instances.
  int ret = SSL_set_fd(ssl_.get(), fd);
  if (ret != 1) {
    return Status::RuntimeError("TLS handshake error", GetOpenSSLErrors());
  }

  // Transfer the SSL instance to the socket.
  socket->reset(new TlsSocket(fd, std::move(ssl_)));

  return Status::OK();
}

Status TlsHandshake::FinishNoWrap(const Socket& socket) {
  RETURN_NOT_OK(GetCerts());
  return Verify(socket);
}

Status TlsHandshake::GetLocalCert(Cert* cert) const {
  if (!local_cert_.GetRawData()) {
    return Status::RuntimeError("no local certificate");
  }
  cert->AdoptAndAddRefRawData(local_cert_.GetRawData());
  return Status::OK();
}

Status TlsHandshake::GetRemoteCert(Cert* cert) const {
  if (!remote_cert_.GetRawData()) {
    return Status::RuntimeError("no remote certificate");
  }
  cert->AdoptAndAddRefRawData(remote_cert_.GetRawData());
  return Status::OK();
}

string TlsHandshake::GetCipherSuite() const {
  CHECK(has_started_);
  return SSL_get_cipher_name(ssl_.get());
}

string TlsHandshake::GetProtocol() const {
  CHECK(has_started_);
  return SSL_get_version(ssl_.get());
}

} // namespace security
} // namespace bb
