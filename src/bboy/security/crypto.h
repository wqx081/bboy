#pragma once

#include <string>

#include "bboy/security/openssl_util.h"

// Forward declarations for the OpenSSL typedefs.
typedef struct rsa_st RSA;
typedef struct bio_st BIO;

namespace bb {

class Status;

namespace security {

// Supported message digests for data signing and signature verification.
enum class DigestType {
  SHA256,
  SHA512,
};

// A class with generic public key interface, but actually it represents
// an RSA key.
class PublicKey : public RawDataWrapper<EVP_PKEY> {
 public:
  ~PublicKey() {}

  Status FromString(const std::string& data, DataFormat format) WARN_UNUSED_RESULT;
  Status ToString(std::string* data, DataFormat format) const WARN_UNUSED_RESULT;
  Status FromFile(const std::string& fpath, DataFormat format) WARN_UNUSED_RESULT;

  Status FromBIO(BIO* bio, DataFormat format) WARN_UNUSED_RESULT;

  // Using the key, verify data signature using the specified message
  // digest algorithm for signature verification.
  // The input signature should be in in raw format (i.e. no base64 encoding).
  Status VerifySignature(DigestType digest,
                         const std::string& data,
                         const std::string& signature) const WARN_UNUSED_RESULT;

  // Sets 'equals' to true if the other public key equals this.
  Status Equals(const PublicKey& other, bool* equals) const WARN_UNUSED_RESULT;
};

// A class with generic private key interface, but actually it represents
// an RSA private key. It's important to have PrivateKey and PublicKey
// be different types to avoid accidental leakage of private keys.
class PrivateKey : public RawDataWrapper<EVP_PKEY> {
 public:
  ~PrivateKey() {}

  Status FromString(const std::string& data, DataFormat format) WARN_UNUSED_RESULT;
  Status ToString(std::string* data, DataFormat format) const WARN_UNUSED_RESULT;
  Status FromFile(const std::string& fpath, DataFormat format) WARN_UNUSED_RESULT;

  // Output the public part of the keypair into the specified placeholder.
  Status GetPublicKey(PublicKey* public_key) const WARN_UNUSED_RESULT;

  // Using the key, generate data signature using the specified
  // message digest algorithm. The result signature is in raw format
  // (i.e. no base64 encoding).
  Status MakeSignature(DigestType digest,
                       const std::string& data,
                       std::string* signature) const WARN_UNUSED_RESULT;
};

// Utility method to generate private keys.
Status GeneratePrivateKey(int num_bits, PrivateKey* ret) WARN_UNUSED_RESULT;

} // namespace security
} // namespace bb
