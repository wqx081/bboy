#pragma once

#include <functional>
#include <memory>
#include <string>

#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "bboy/gbase/strings/substitute.h"
#include "bboy/base/status.h"

// Forward declarations for the OpenSSL typedefs.
typedef struct X509_req_st X509_REQ;
typedef struct bio_st BIO;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;
typedef struct x509_st X509;

#define OPENSSL_CHECK_OK(call) \
  CHECK_GT((call), 0)

#define OPENSSL_RET_NOT_OK(call, msg) \
  if ((call) <= 0) { \
    return Status::RuntimeError((msg), GetOpenSSLErrors()); \
  }

#define OPENSSL_RET_IF_NULL(call, msg) \
  if ((call) == nullptr) { \
    return Status::RuntimeError((msg), GetOpenSSLErrors()); \
  }

namespace bb {
namespace security {

// Disable initialization of OpenSSL. Must be called before
// any call to InitializeOpenSSL().
Status DisableOpenSSLInitialization() WARN_UNUSED_RESULT;

// Initializes static state required by the OpenSSL library.
// This is a no-op if DisableOpenSSLInitialization() has been called.
//
// Safe to call multiple times.
void InitializeOpenSSL();

// Fetches errors from the OpenSSL error error queue, and stringifies them.
//
// The error queue will be empty after this method returns.
//
// See man(3) ERR_get_err for more discussion.
std::string GetOpenSSLErrors();

// Returns a string representation of the provided error code, which must be
// from a prior call to the SSL_get_error function.
//
// If necessary, the OpenSSL error queue may be inspected and emptied as part of
// this call, and/or 'errno' may be inspected. As a result, this method should
// only be used directly after the error occurs, and from the same thread.
//
// See man(3) SSL_get_error for more discussion.
std::string GetSSLErrorDescription(int error_code);


// A generic wrapper for OpenSSL structures.
template <typename T>
using c_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

// For each SSL type, the Traits class provides the important OpenSSL
// API functions.
template<typename SSL_TYPE>
struct SslTypeTraits {};

template<> struct SslTypeTraits<X509> {
  static constexpr auto free = &X509_free;
  static constexpr auto read_pem = &PEM_read_bio_X509;
  static constexpr auto read_der = &d2i_X509_bio;
  static constexpr auto write_pem = &PEM_write_bio_X509;
  static constexpr auto write_der = &i2d_X509_bio;
};
template<> struct SslTypeTraits<X509_EXTENSION> {
  static constexpr auto free = &X509_EXTENSION_free;
};
template<> struct SslTypeTraits<X509_REQ> {
  static constexpr auto free = &X509_REQ_free;
  static constexpr auto read_pem = &PEM_read_bio_X509_REQ;
  static constexpr auto read_der = &d2i_X509_REQ_bio;
  static constexpr auto write_pem = &PEM_write_bio_X509_REQ;
  static constexpr auto write_der = &i2d_X509_REQ_bio;
};
template<> struct SslTypeTraits<EVP_PKEY> {
  static constexpr auto free = &EVP_PKEY_free;
};
template<> struct SslTypeTraits<SSL_CTX> {
  static constexpr auto free = &SSL_CTX_free;
};

template<typename SSL_TYPE, typename Traits = SslTypeTraits<SSL_TYPE>>
c_unique_ptr<SSL_TYPE> ssl_make_unique(SSL_TYPE* d) {
  return {d, Traits::free};
}

// Acceptable formats for keys, X509 certificates and X509 CSRs.
enum class DataFormat {
  DER = 0,    // DER/ASN1 format (binary): for representing object on the wire
  PEM = 1,    // PEM format (ASCII): for storing on filesystem, printing, etc.
};

// Data format representation as a string.
const std::string& DataFormatToString(DataFormat fmt);

// Template wrapper for dynamically allocated entities with custom deleter.
// Mostly, using it for xxx_st types from the OpenSSL crypto library.
template<typename Type>
class RawDataWrapper {
 public:
  typedef Type RawDataType;

  RawDataType* GetRawData() const {
    return data_.get();
  }

  void AdoptRawData(RawDataType* d) {
    data_ = ssl_make_unique(d);
  }

 protected:
  c_unique_ptr<RawDataType> data_;
};

} // namespace security
} // namespace bb
