#pragma once

#include "bboy/security/openssl_util.h"
#include "bboy/base/net/socket.h"
#include "bboy/base/status.h"

struct ssl_st;
typedef ssl_st SSL;

namespace bb {
namespace security {

class TlsSocket : public Socket {
 public:

  ~TlsSocket() override;

  virtual Status Write(const uint8_t *buf, size_t amt, size_t *nwritten) override WARN_UNUSED_RESULT;
  virtual Status Writev(const struct ::iovec *iov, int iov_len, size_t* nwritten) override WARN_UNUSED_RESULT;
  virtual Status Read(uint8_t *buf, size_t amt, size_t *nread) override WARN_UNUSED_RESULT;

  Status Close() override WARN_UNUSED_RESULT;

 private:

  friend class TlsHandshake;

  TlsSocket(int fd, c_unique_ptr<SSL> ssl);

  // Owned SSL handle.
  c_unique_ptr<SSL> ssl_;
};

} // namespace security
} // namespace kudu
