#include "Nexora/Cryptography/Signature.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>
#if defined(NEXORA_TEST_OPENSSL)
#include <memory>
#include <openssl/evp.h>
#endif
namespace {
using namespace nexora::cryptography;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::vector<std::byte> Hex(std::string_view text) {
  const auto digit = [](char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
  };
  Require(text.size() % 2 == 0, "Invalid fixture hex");
  std::vector<std::byte> bytes;
  for (std::size_t i = 0; i < text.size(); i += 2) {
    Require(digit(text[i]) >= 0 && digit(text[i + 1]) >= 0, "Invalid fixture hex digit");
    bytes.push_back(static_cast<std::byte>((digit(text[i]) << 4) | digit(text[i + 1])));
  }
  return bytes;
}
void Run() {
  // Public, independently published RFC8032 section 7.1 vectors.
  const auto key = Hex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
  auto signature = Hex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
                       "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
  Require(VerifyEd25519({}, signature, {}) == Verification::InvalidInput &&
              VerifyEd25519(key, std::span(signature).first(63), {}) == Verification::InvalidInput,
          "Malformed key/signature shape reached crypto");
  std::vector<std::byte> large(kMaximumMessageBytes + 1);
  Require(VerifyEd25519(key, signature, large) == Verification::InvalidInput && !Sha256(large),
          "Over-budget input reached crypto");
  if (ActiveProvider() == Provider::Unavailable) {
    Require(VerifyEd25519(key, signature, {}) == Verification::BackendUnavailable && !Sha256({}),
            "Unavailable provider accepted a signature/digest");
    return;
  }
  Require(ActiveProvider() == Provider::OpenSsl, "Unexpected provider");
  Require(VerifyEd25519(key, signature, {}) == Verification::Verified,
          "RFC8032 empty-message signature rejected");
  const auto empty_hash = Sha256({});
  const auto expected_empty =
      Hex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  Require(empty_hash && std::equal(empty_hash->begin(), empty_hash->end(), expected_empty.begin()),
          "SHA256 empty known vector failed");
  const std::array message{std::byte{'a'}, std::byte{'b'}, std::byte{'c'}};
  const auto abc = Sha256(message);
  const auto expected_abc = Hex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  Require(abc && std::equal(abc->begin(), abc->end(), expected_abc.begin()),
          "SHA256 abc known vector failed");
  auto altered_key = key;
  altered_key[4] ^= std::byte{1};
  Require(VerifyEd25519(altered_key, signature, {}) != Verification::Verified,
          "Modified public key accepted");
  signature[7] ^= std::byte{1};
  Require(VerifyEd25519(key, signature, {}) != Verification::Verified,
          "Modified signature accepted");
  signature[7] ^= std::byte{1};
  Require(VerifyEd25519(key, signature, message) != Verification::Verified,
          "Modified message accepted");
  const auto key2 = Hex("3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c");
  const auto sig2 = Hex("92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
                        "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00");
  const auto one = Hex("72");
  Require(VerifyEd25519(key2, sig2, one) == Verification::Verified,
          "RFC8032 one-byte vector rejected");
#if defined(NEXORA_TEST_OPENSSL)
  // Published RFC seed, not a user credential; sign the exact boundary with the real provider.
  const auto seed = Hex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
  auto private_key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(
      EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr,
                                   reinterpret_cast<const unsigned char *>(seed.data()),
                                   seed.size()),
      EVP_PKEY_free);
  auto context =
      std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  large.resize(kMaximumMessageBytes);
  std::array<std::byte, 64> boundary_signature{};
  std::size_t length = boundary_signature.size();
  Require(
      private_key && context &&
          EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, private_key.get()) == 1 &&
          EVP_DigestSign(
              context.get(), reinterpret_cast<unsigned char *>(boundary_signature.data()), &length,
              reinterpret_cast<const unsigned char *>(large.data()), large.size()) == 1 &&
          length == 64,
      "Exact-boundary real signature fixture failed");
  Require(VerifyEd25519(key, boundary_signature, large) == Verification::Verified &&
              Sha256(large).has_value(),
          "Exact 64MiB boundary rejected");
#endif
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "RFC8032/SHA256/tampering/bounds/provider contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
