#pragma once
#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/NativeTool.h"
#include "Nexora/Editor/SignedExtensionHost.h"

namespace nexora::editor {
enum class ManagedExtensionState : std::uint8_t {
  Disabled,
  Loaded,
  ShutdownPending,
  RestartRequired,
  Rejected
};
struct ManagedExtension final {
  ExtensionManifest manifest;
  std::filesystem::path relative_path;
  std::size_t package_bytes{};
  std::uint64_t native_id{};
  ManagedExtensionState state{ManagedExtensionState::Disabled};
  ExtensionAdmissionError error{ExtensionAdmissionError::None};
  runtime::PluginLoadError native_error{runtime::PluginLoadError::None};
  runtime::PluginLifecycleError lifecycle_error{runtime::PluginLifecycleError::None};
  std::int32_t lifecycle_result{};
  std::uint32_t reported_abi{};
  std::size_t registered_services{};
  bool cooperative{};
};
struct ExtensionPackageReview final {
  ExtensionManifest manifest;
  std::size_t package_bytes{};
  std::uint64_t scope{}, configuration{};
};
// Owning observation, not a permission grant. Exact manager instance/scope/configuration and
// admission are rechecked for every call; callers never retain a native service pointer.
struct ManagedToolSelection final {
  std::uint64_t manager{}, scope{}, configuration{}, native_id{};
  std::string id, version;
};
// Serialized owner. Callers drain every borrowed service call before revoke, detach or unload.
// No project/package input enrolls trusted keys, and no package auto-enables on project activation.
class NEXORA_EDITOR_API PluginManager final {
public:
  static constexpr std::size_t kMaximumPackages = 16;
  static constexpr std::size_t kMaximumPackageBytes =
      cryptography::kMaximumMessageBytes + SignedExtensionHost::kMaximumManifestBytes + 96;
  PluginManager();
  ~PluginManager();
  PluginManager(const PluginManager &) = delete;
  PluginManager &operator=(const PluginManager &) = delete;
  bool BindProject(const ProjectWorkspace &, std::string *error = nullptr);
  void Detach() noexcept;
  bool SetPublisher(std::string publisher, std::span<const std::byte> key);
  bool RemovePublisher(std::string_view publisher);
  bool SetAllowedPermissions(std::uint32_t permissions);
  [[nodiscard]] std::vector<TrustedPublisherKey> Publishers() const;
  [[nodiscard]] std::uint32_t AllowedPermissions() const noexcept { return allowed_permissions_; }
  [[nodiscard]] std::uint64_t Scope() const noexcept { return scope_; }
  [[nodiscard]] foundation::Uuid Project() const noexcept { return project_; }
  [[nodiscard]] const std::filesystem::path &Root() const noexcept { return root_; }
  [[nodiscard]] static std::optional<std::vector<std::byte>>
  EncodePackage(const SignedExtensionPackage &);
  [[nodiscard]] static std::optional<SignedExtensionPackage>
      DecodePackage(std::span<const std::byte>);
  [[nodiscard]] static std::optional<std::vector<std::byte>>
  ReadPackageFile(const std::filesystem::path &);
  bool Review(const ProjectWorkspace &, const std::filesystem::path &,
              std::string *error = nullptr);
  [[nodiscard]] std::optional<ExtensionPackageReview> ReviewSnapshot() const;
  void CancelReview() noexcept;
  bool Install(const ProjectWorkspace &, std::uint64_t scope, std::uint64_t configuration,
               std::string *error = nullptr);
  // Explicit discovery performs no code loading. Corrupt/untrusted files cannot become enabled.
  bool Refresh(const ProjectWorkspace &, std::string *error = nullptr);
  bool Enable(const ProjectWorkspace &, std::string_view id, std::string_view version,
              std::string *error = nullptr);
  bool Disable(std::string_view id, std::string_view version);
  bool Remove(const ProjectWorkspace &, std::string_view id, std::string_view version,
              std::string *error = nullptr);
  void Poll() noexcept;
  [[nodiscard]] std::vector<ManagedExtension> Snapshot() const;
  // Borrowed while enabled, until the next owner revoke/disable/detach/poll operation.
  [[nodiscard]] void *FindService(std::string_view name) const;
  [[nodiscard]] std::optional<ManagedToolSelection> SelectTool(std::string_view id,
                                                               std::string_view version) const;
  // Synchronous owning-byte call. No lifecycle changes or document/IO publication. Callers parse
  // outputs and independently recheck document scope/Play/writer authority before authoring.
  [[nodiscard]] NativeToolOutcome InvokeTool(const ProjectWorkspace &, const ManagedToolSelection &,
                                             std::string_view service, NativeToolOperation,
                                             std::span<const std::byte> input);
  [[nodiscard]] bool RestartRequired() const;
  [[nodiscard]] std::size_t RejectedFiles() const noexcept { return rejected_files_; }

private:
  bool Matches(const ProjectWorkspace &) const;
  bool Writable(const ProjectWorkspace &) const;
  bool Configure();
  bool Changed();
  std::optional<PreparedExtension> Verify(std::span<const std::byte>,
                                          ExtensionAdmissionError *) const;
  NativeToolInvoker tool_invoker_;
  const std::uint64_t instance_;
  ExtensionTrust trust_;
  runtime::ServiceRegistry services_;
  SignedExtensionHost host_;
  foundation::Uuid project_{};
  std::filesystem::path root_;
  std::uint64_t scope_{1}, configuration_{1};
  std::uint32_t allowed_permissions_{};
  ProjectAccess access_{ProjectAccess::ReadOnly};
  std::size_t rejected_files_{};
  std::vector<ManagedExtension> entries_;
  std::vector<std::byte> reviewed_bytes_;
  std::optional<ExtensionPackageReview> review_;
};
} // namespace nexora::editor
