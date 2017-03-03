#pragma once

#include <map>
#include <vector>
#include <memory>

#include "bboy/gbase/macros.h"
#include "bboy/base/status.h"
#include "bboy/base/sync/rw_mutex.h"

namespace bb {
namespace security {

class SignedTokenPB;
class TokenPB;
class TokenSigningPublicKey;
class TokenSigningPublicKeyPB;
enum class VerificationResult;

// Class responsible for verifying tokens provided to a server.
//
// This class manages a set of public keys, each identified by a sequence
// number. It exposes the latest known sequence number, which can be sent
// to a 'TokenSigner' running on another node. That node can then
// export public keys, which are transferred back to this node and imported
// into the 'TokenVerifier'.
//
// Each signed token also includes the key sequence number that signed it,
// so this class can look up the correct key and verify the token's
// validity and expiration.
//
// Note that this class does not perform any "business logic" around the
// content of a token. It only verifies that the token has a valid signature
// and is not yet expired. Any business rules around authorization or
// authentication are left up to callers.
//
// This class is thread-safe.
class TokenVerifier {
 public:
  TokenVerifier();
  ~TokenVerifier();

  // Return the highest key sequence number known by this instance.
  //
  // If no keys are known, return -1.
  int64_t GetMaxKnownKeySequenceNumber() const;

  // Import a set of public keys provided by a TokenSigner instance
  // (which might be running on a remote node). If any public keys already
  // exist with matching key sequence numbers, they are replaced by
  // the new keys.
  Status ImportKeys(const std::vector<TokenSigningPublicKeyPB>& keys) WARN_UNUSED_RESULT;

  // Export token signing public keys. Specifying the 'after_sequence_number'
  // allows to get public keys with sequence numbers greater than
  // 'after_sequence_number'. If the 'after_sequence_number' parameter is
  // omitted, all known public keys are exported.
  std::vector<TokenSigningPublicKeyPB> ExportKeys(
      int64_t after_sequence_number = -1) const;

  // Verify the signature on the given signed token, and deserialize the
  // contents into 'token'.
  VerificationResult VerifyTokenSignature(const SignedTokenPB& signed_token,
                                          TokenPB* token) const;

  // TODO(PKI): should expire out old key versions at some point. eg only
  // keep old key versions until their expiration is an hour or two in the past?
  // Not sure where we'll call this from, and likely the "slow leak" isn't
  // critical for first implementation.
  // void ExpireOldKeys();

 private:
  typedef std::map<int64_t, std::unique_ptr<TokenSigningPublicKey>> KeysMap;

  // Lock protecting keys_by_seq_
  mutable RWMutex lock_;
  KeysMap keys_by_seq_;

  DISALLOW_COPY_AND_ASSIGN(TokenVerifier);
};

// Result of a token verification.
enum class VerificationResult {
  // The signature is valid and the token is not expired.
  VALID,
  // The token itself is invalid (e.g. missing its signature or data,
  // can't be deserialized, etc).
  INVALID_TOKEN,
  // The signature is invalid (i.e. cryptographically incorrect).
  INVALID_SIGNATURE,
  // The signature is valid, but the token has already expired.
  EXPIRED_TOKEN,
  // The signature is valid, but the signing key is no longer valid.
  EXPIRED_SIGNING_KEY,
  // The signing key used to sign this token is not available.
  UNKNOWN_SIGNING_KEY,
  // The token uses an incompatible feature which isn't supported by this
  // version of the server. We reject the token to give a "default deny"
  // policy.
  INCOMPATIBLE_FEATURE
};

const char* VerificationResultToString(VerificationResult r);

} // namespace security
} // namespace bb
