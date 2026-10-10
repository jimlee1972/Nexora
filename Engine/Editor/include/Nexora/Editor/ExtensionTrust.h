#pragma once
#include "Nexora/Cryptography/Signature.h"
#include "Nexora/Editor/Api.h"
#include <string>
#include <string_view>
#include <vector>

namespace nexora::editor {
struct TrustedPublisherKey final {
  std::string publisher;
  std::array<std::byte, 32> public_key{};
};
enum class ExtensionTrustStatus : std::uint8_t {
  Verified,
  InvalidInput,
  UntrustedPublisher,
  InvalidSignature,
  BackendUnavailable,
  BackendFailure
};
struct ExtensionTrustResult final {
  ExtensionTrustStatus status{ExtensionTrustStatus::InvalidInput};
  std::uint64_t trust_revision{};
  std::optional<cryptography::Sha256Digest> artifact_digest;
};
// Serialized caller-owned trust configuration; no files, private keys, network or code loading.
// Verify borrows the exact signed bytes only for the call and returns owning metadata. It never
// accepts a caller's signature-valid boolean. Key changes advance revision; later hosts must
// revalidate revision, manifest/permissions/ABI and staged artifact bytes before native loading.
class NEXORA_EDITOR_API ExtensionTrust final {
public:
  static constexpr std::size_t kMaximumPublishers = 64;
  static constexpr std::size_t kMaximumPublisherBytes = 128;
  // IDs: case-sensitive ASCII alphanumeric first, then alphanumeric/dot/dash/underscore.
  // Empty/all-zero/identity key admissions reject, preserving the existing registry.
  bool SetPublisher(std::string_view publisher, std::span<const std::byte> public_key);
  bool RemovePublisher(std::string_view publisher) noexcept;
  [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }
  [[nodiscard]] std::vector<TrustedPublisherKey> Snapshot() const { return publishers_; }
  [[nodiscard]] ExtensionTrustResult Verify(std::string_view publisher,
                                            std::span<const std::byte> artifact,
                                            std::span<const std::byte> signature) const noexcept;

private:
  std::uint64_t revision_{1};
  std::vector<TrustedPublisherKey> publishers_;
};
} // namespace nexora::editor
