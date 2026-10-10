#pragma once
#include "Nexora/Editor/ExtensionTrust.h"
#include "Nexora/Runtime/EditorSdk.h"

namespace nexora::editor {
struct ExtensionManifest final {
  std::string id, version, publisher, target;
  std::uint32_t engine_abi{}, permissions{};
  std::vector<std::string> dependencies;
  cryptography::Sha256Digest artifact_digest{};
  friend bool operator==(const ExtensionManifest &, const ExtensionManifest &) = default;
};
struct ExtensionAdmissionPolicy final {
  std::uint32_t engine_abi{}, allowed_permissions{};
  std::string target;
  std::vector<std::string> available_dependencies;
  friend bool operator==(const ExtensionAdmissionPolicy &,
                         const ExtensionAdmissionPolicy &) = default;
};
enum class ExtensionAdmissionError : std::uint8_t {
  None,
  InvalidManifest,
  PolicyRejected,
  UntrustedOrInvalidSignature,
  ArtifactMismatch,
  BackendUnavailable,
  StaleAdmission,
  BudgetExceeded,
  DuplicateIdentity,
  NativeLoadFailed
};
struct SignedExtensionPackage final {
  std::vector<std::byte> manifest;
  std::array<std::byte, 64> signature{};
  std::vector<std::byte> artifact;
};
class PreparedExtension final {
public:
  [[nodiscard]] const ExtensionManifest &Manifest() const noexcept { return manifest_; }

private:
  friend class SignedExtensionHost;
  ExtensionManifest manifest_;
  SignedExtensionPackage package_;
  std::uint64_t trust_revision{}, policy_revision{};
};
struct SignedExtensionLoadResult final {
  ExtensionAdmissionError error{ExtensionAdmissionError::InvalidManifest};
  runtime::PluginLoadResult native;
};
// Authoring-owner serialization; trust and optional services outlive this host and its calls.
// Existing PluginHost remains a trusted low-level loader; Editor package admissions use this host.
class NEXORA_EDITOR_API SignedExtensionHost final {
public:
  static constexpr std::size_t kMaximumManifestBytes = 16 * 1024, kMaximumDependencies = 64;
  static constexpr std::uint32_t kKnownPermissions = 0x3f;
  SignedExtensionHost(ExtensionTrust &trust, std::uint32_t engine_abi);
  ~SignedExtensionHost();
  SignedExtensionHost(const SignedExtensionHost &) = delete;
  SignedExtensionHost &operator=(const SignedExtensionHost &) = delete;
  bool SetPolicy(ExtensionAdmissionPolicy policy);
  [[nodiscard]] static std::optional<std::vector<std::byte>>
  EncodeManifest(const ExtensionManifest &);
  [[nodiscard]] static std::optional<ExtensionManifest> DecodeManifest(std::span<const std::byte>);
  // No IO or code execution; captures actual verified manifest/artifact and current
  // policy/revision.
  [[nodiscard]] std::optional<PreparedExtension>
  Prepare(SignedExtensionPackage package, ExtensionAdmissionError *error = nullptr) const;
  // Revalidates trust/policy and exact owned bytes before any native initializer can execute.
  // Linux sealed images only; unavailable backends reject rather than staging mutable files.
  SignedExtensionLoadResult Load(const PreparedExtension &,
                                 runtime::ServiceRegistry *services = nullptr);
  runtime::PluginState RequestUnload(std::uint64_t id) noexcept;
  void PollShutdown() noexcept;
  [[nodiscard]] std::vector<runtime::PluginSnapshot> Snapshot() const;

private:
  struct Image final {
    std::uint64_t id{};
    std::string identity;
    int fd{-1};
    std::uint64_t trust_revision{}, policy_revision{};
  };
  bool PolicyAllows(const ExtensionManifest &) const;
  void ReleaseUnloaded() noexcept;
  ExtensionTrust &trust_;
  std::uint32_t engine_abi_{};
  std::uint64_t policy_revision_{1};
  ExtensionAdmissionPolicy policy_;
  runtime::PluginHost native_;
  std::vector<Image> images_;
};
} // namespace nexora::editor
