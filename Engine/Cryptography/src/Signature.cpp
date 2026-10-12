#include "Nexora/Cryptography/Signature.h"
#if defined(NEXORA_CRYPTOGRAPHY_OPENSSL)
#include <memory>
#include <openssl/evp.h>
#endif

namespace nexora::cryptography {
Provider ActiveProvider() noexcept {
#if defined(NEXORA_CRYPTOGRAPHY_OPENSSL)
  return Provider::OpenSsl;
#else
  return Provider::Unavailable;
#endif
}
Verification VerifyEd25519(std::span<const std::byte> public_key,
                           std::span<const std::byte> signature,
                           std::span<const std::byte> message) noexcept {
  if (public_key.size() != 32 || signature.size() != 64 || message.size() > kMaximumMessageBytes)
    return Verification::InvalidInput;
#if defined(NEXORA_CRYPTOGRAPHY_OPENSSL)
  const auto key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(
      EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
                                  reinterpret_cast<const unsigned char *>(public_key.data()),
                                  public_key.size()),
      EVP_PKEY_free);
  const auto context =
      std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  if (!key || !context ||
      EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1)
    return Verification::BackendFailure;
  const unsigned char empty{};
  const auto result = EVP_DigestVerify(
      context.get(), reinterpret_cast<const unsigned char *>(signature.data()), signature.size(),
      message.empty() ? &empty : reinterpret_cast<const unsigned char *>(message.data()),
      message.size());
  return result == 1   ? Verification::Verified
         : result == 0 ? Verification::InvalidSignature
                       : Verification::BackendFailure;
#else
  return Verification::BackendUnavailable;
#endif
}
std::optional<Sha256Digest> Sha256(std::span<const std::byte> message) noexcept {
  if (message.size() > kMaximumMessageBytes)
    return std::nullopt;
#if defined(NEXORA_CRYPTOGRAPHY_OPENSSL)
  Sha256Digest digest{};
  unsigned int size{};
  if (EVP_Digest(message.data(), message.size(), reinterpret_cast<unsigned char *>(digest.data()),
                 &size, EVP_sha256(), nullptr) != 1 ||
      size != digest.size())
    return std::nullopt;
  return digest;
#else
  return std::nullopt;
#endif
}
} // namespace nexora::cryptography
