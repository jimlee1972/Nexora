#include "Nexora/Editor/ExtensionTrust.h"
#include <algorithm>
#include <limits>

namespace nexora::editor {
namespace {
bool PublisherId(std::string_view id) noexcept {
  const auto alphanumeric = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
  };
  if (id.empty() || id.size() > ExtensionTrust::kMaximumPublisherBytes || !alphanumeric(id.front()))
    return false;
  return std::ranges::all_of(
      id, [&](char c) { return alphanumeric(c) || c == '.' || c == '-' || c == '_'; });
}
bool KeyShape(std::span<const std::byte> key) noexcept {
  if (key.size() != 32)
    return false;
  if (std::ranges::all_of(key, [](auto byte) { return byte == std::byte{}; }))
    return false;
  return key.front() != std::byte{1} ||
         !std::ranges::all_of(key.subspan(1), [](auto byte) { return byte == std::byte{}; });
}
} // namespace
bool ExtensionTrust::SetPublisher(std::string_view publisher,
                                  std::span<const std::byte> public_key) {
  if (!PublisherId(publisher) || !KeyShape(public_key))
    return false;
  const auto found = std::ranges::find(publishers_, publisher, &TrustedPublisherKey::publisher);
  if (found != publishers_.end() && std::ranges::equal(found->public_key, public_key))
    return true;
  if (revision_ == std::numeric_limits<std::uint64_t>::max() ||
      (found == publishers_.end() && publishers_.size() == kMaximumPublishers))
    return false;
  if (found == publishers_.end()) {
    TrustedPublisherKey key;
    key.publisher = publisher;
    std::ranges::copy(public_key, key.public_key.begin());
    publishers_.push_back(std::move(key));
  } else {
    std::ranges::copy(public_key, found->public_key.begin());
  }
  ++revision_;
  return true;
}
bool ExtensionTrust::RemovePublisher(std::string_view publisher) noexcept {
  const auto found = std::ranges::find(publishers_, publisher, &TrustedPublisherKey::publisher);
  if (found == publishers_.end() || revision_ == std::numeric_limits<std::uint64_t>::max())
    return false;
  publishers_.erase(found);
  ++revision_;
  return true;
}
ExtensionTrustResult ExtensionTrust::Verify(std::string_view publisher,
                                            std::span<const std::byte> artifact,
                                            std::span<const std::byte> signature) const noexcept {
  ExtensionTrustResult result{ExtensionTrustStatus::InvalidInput, revision_, {}};
  if (!PublisherId(publisher) || artifact.size() > cryptography::kMaximumMessageBytes ||
      signature.size() != 64)
    return result;
  const auto found = std::ranges::find(publishers_, publisher, &TrustedPublisherKey::publisher);
  if (found == publishers_.end()) {
    result.status = ExtensionTrustStatus::UntrustedPublisher;
    return result;
  }
  const auto verified = cryptography::VerifyEd25519(found->public_key, signature, artifact);
  switch (verified) {
  case cryptography::Verification::Verified:
    result.artifact_digest = cryptography::Sha256(artifact);
    result.status = result.artifact_digest ? ExtensionTrustStatus::Verified
                                           : ExtensionTrustStatus::BackendFailure;
    break;
  case cryptography::Verification::InvalidInput:
    result.status = ExtensionTrustStatus::InvalidInput;
    break;
  case cryptography::Verification::InvalidSignature:
    result.status = ExtensionTrustStatus::InvalidSignature;
    break;
  case cryptography::Verification::BackendUnavailable:
    result.status = ExtensionTrustStatus::BackendUnavailable;
    break;
  case cryptography::Verification::BackendFailure:
    result.status = ExtensionTrustStatus::BackendFailure;
    break;
  }
  return result;
}
} // namespace nexora::editor
