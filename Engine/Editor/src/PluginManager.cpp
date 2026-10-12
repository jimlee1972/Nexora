#include "Nexora/Editor/PluginManager.h"
#include "AtomicFile.h"
#include "Nexora/Foundation/BuildInfo.h"
#include <algorithm>
#include <atomic>
#include <fstream>
#include <limits>

namespace nexora::editor {
namespace {
std::uint64_t NextInstance() noexcept {
  static std::atomic<std::uint64_t> next{1};
  auto value = next.load(std::memory_order_relaxed);
  while (value != std::numeric_limits<std::uint64_t>::max()) {
    if (next.compare_exchange_weak(value, value + 1, std::memory_order_relaxed))
      return value;
  }
  return 0; // Exhaustion never reuses an earlier manager identity.
}
constexpr std::string_view magic = "NXEXTPK1";
constexpr std::size_t header_bytes = 84;
bool Fail(std::string *error, const char *message) {
  if (error)
    *error = message;
  return false;
}
bool BuiltinDependency(std::string_view dependency) {
  return dependency == "core" || dependency == "reflection" || dependency == "Editor" ||
         dependency == "Foundation";
}
bool Missing(const std::filesystem::file_status &status, const std::error_code &error) {
  return error == std::errc::no_such_file_or_directory ||
         (!error && status.type() == std::filesystem::file_type::not_found);
}
bool Directory(const std::filesystem::path &path, bool create, std::string *error) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (Missing(status, ec) && create) {
    ec.clear();
    return std::filesystem::create_directory(path, ec) && !ec
               ? true
               : Fail(error, "Extension installation directory could not be created.");
  }
  return !ec && std::filesystem::is_directory(status)
             ? true
             : Fail(error, "Extension installation directory is missing or aliased.");
}
std::string Target() {
#if defined(__ANDROID__)
  return "android-native";
#elif defined(__linux__) && defined(__aarch64__)
  return "linux-aarch64";
#elif defined(__linux__)
  return "linux-x86_64";
#elif defined(_WIN32)
  return "windows-x86_64";
#elif defined(__APPLE__)
  return "macos-native";
#else
  return "unsupported-native";
#endif
}
std::filesystem::path Destination(const ExtensionManifest &m) {
  return std::filesystem::path(".nexora/extensions") /
         (std::to_string(m.id.size()) + "-" + m.id + "-" + m.version + ".nxpkg");
}
void Number(std::vector<std::byte> &bytes, std::uint64_t value, unsigned width) {
  for (unsigned i = 0; i < width; ++i)
    bytes.push_back(static_cast<std::byte>(value >> (i * 8)));
}
std::uint64_t Number(std::span<const std::byte> bytes) {
  std::uint64_t value{};
  for (std::size_t i = 0; i < bytes.size(); ++i)
    value |= std::to_integer<std::uint64_t>(bytes[i]) << (i * 8);
  return value;
}
ManagedExtensionState State(runtime::PluginState state) {
  switch (state) {
  case runtime::PluginState::Loaded:
    return ManagedExtensionState::Loaded;
  case runtime::PluginState::ShutdownPending:
    return ManagedExtensionState::ShutdownPending;
  case runtime::PluginState::RestartRequired:
    return ManagedExtensionState::RestartRequired;
  default:
    return ManagedExtensionState::Disabled;
  }
}
} // namespace
PluginManager::PluginManager()
    : instance_(NextInstance()), host_(trust_, foundation::kEngineAbiVersion) {
  entries_.reserve(kMaximumPackages);
  static_cast<void>(Configure());
}
PluginManager::~PluginManager() { Detach(); }
bool PluginManager::Matches(const ProjectWorkspace &workspace) const {
  return !root_.empty() && project_ == workspace.Project().id && root_ == workspace.Root() &&
         access_ == workspace.Access();
}
bool PluginManager::Writable(const ProjectWorkspace &workspace) const {
  return Matches(workspace) && workspace.Writable() && !workspace.HasRecoveryJournal() &&
         !workspace.HasExternalChange();
}
bool PluginManager::Configure() {
  ExtensionAdmissionPolicy policy{foundation::kEngineAbiVersion,
                                  allowed_permissions_,
                                  Target(),
                                  {"Editor", "Foundation", "core", "reflection"}};
  for (const auto &entry : entries_)
    policy.available_dependencies.push_back(entry.manifest.id);
  std::ranges::sort(policy.available_dependencies);
  const auto last =
      std::unique(policy.available_dependencies.begin(), policy.available_dependencies.end());
  policy.available_dependencies.erase(last, policy.available_dependencies.end());
  return host_.SetPolicy(std::move(policy));
}
bool PluginManager::Changed() {
  if (configuration_ == std::numeric_limits<std::uint64_t>::max())
    return false;
  ++configuration_;
  CancelReview();
  host_.PollShutdown();
  Poll();
  return true;
}
bool PluginManager::BindProject(const ProjectWorkspace &workspace, std::string *error) {
  if (workspace.Root().empty() || workspace.Project().id == foundation::Uuid{} ||
      scope_ == std::numeric_limits<std::uint64_t>::max())
    return Fail(error, "An open project and available extension scope are required.");
  if (Matches(workspace)) {
    if (workspace.HasRecoveryJournal() || workspace.HasExternalChange()) {
      host_.RequestUnloadAll();
      CancelReview();
      Poll();
    }
    return true;
  }
  Detach();
  project_ = workspace.Project().id;
  root_ = workspace.Root();
  access_ = workspace.Access();
  return Configure() && Refresh(workspace, error);
}
void PluginManager::Detach() noexcept {
  host_.RequestUnloadAll();
  CancelReview();
  entries_.clear();
  root_.clear();
  project_ = {};
  access_ = ProjectAccess::ReadOnly;
  allowed_permissions_ = 0;
  rejected_files_ = 0;
  if (scope_ != std::numeric_limits<std::uint64_t>::max())
    ++scope_;
}
bool PluginManager::SetPublisher(std::string publisher, std::span<const std::byte> key) {
  if (configuration_ == std::numeric_limits<std::uint64_t>::max())
    return false;
  const auto before = trust_.Revision();
  if (!trust_.SetPublisher(publisher, key))
    return false;
  return before == trust_.Revision() || Changed();
}
bool PluginManager::RemovePublisher(std::string_view publisher) {
  if (configuration_ == std::numeric_limits<std::uint64_t>::max() ||
      !trust_.RemovePublisher(publisher))
    return false;
  return Changed();
}
bool PluginManager::SetAllowedPermissions(std::uint32_t permissions) {
  if (permissions & ~SignedExtensionHost::kKnownPermissions ||
      configuration_ == std::numeric_limits<std::uint64_t>::max())
    return false;
  if (permissions == allowed_permissions_)
    return true;
  const auto previous = allowed_permissions_;
  allowed_permissions_ = permissions;
  if (!Configure()) {
    allowed_permissions_ = previous;
    return false;
  }
  return Changed();
}
std::vector<TrustedPublisherKey> PluginManager::Publishers() const { return trust_.Snapshot(); }
std::optional<std::vector<std::byte>>
PluginManager::EncodePackage(const SignedExtensionPackage &p) {
  if (!SignedExtensionHost::DecodeManifest(p.manifest) || p.artifact.empty() ||
      p.artifact.size() > cryptography::kMaximumMessageBytes)
    return {};
  std::vector<std::byte> bytes;
  bytes.reserve(header_bytes + p.manifest.size() + p.artifact.size());
  for (const auto c : magic)
    bytes.push_back(static_cast<std::byte>(c));
  Number(bytes, p.manifest.size(), 4);
  Number(bytes, p.artifact.size(), 8);
  bytes.insert(bytes.end(), p.signature.begin(), p.signature.end());
  bytes.insert(bytes.end(), p.manifest.begin(), p.manifest.end());
  bytes.insert(bytes.end(), p.artifact.begin(), p.artifact.end());
  return bytes;
}
std::optional<SignedExtensionPackage>
PluginManager::DecodePackage(std::span<const std::byte> bytes) {
  if (bytes.size() < header_bytes || bytes.size() > kMaximumPackageBytes)
    return {};
  for (std::size_t i = 0; i < magic.size(); ++i)
    if (bytes[i] != static_cast<std::byte>(magic[i]))
      return {};
  const auto manifest = Number(bytes.subspan(8, 4)), artifact = Number(bytes.subspan(12, 8));
  if (!manifest || manifest > SignedExtensionHost::kMaximumManifestBytes || !artifact ||
      artifact > cryptography::kMaximumMessageBytes || manifest > bytes.size() - header_bytes ||
      artifact != bytes.size() - header_bytes - manifest)
    return {};
  SignedExtensionPackage p;
  std::ranges::copy(bytes.subspan(20, 64), p.signature.begin());
  p.manifest.assign(bytes.begin() + header_bytes,
                    bytes.begin() + header_bytes + static_cast<std::size_t>(manifest));
  if (!SignedExtensionHost::DecodeManifest(p.manifest))
    return {};
  p.artifact.assign(bytes.begin() + header_bytes + static_cast<std::size_t>(manifest), bytes.end());
  return p;
}
std::optional<std::vector<std::byte>>
PluginManager::ReadPackageFile(const std::filesystem::path &path) {
  const auto utf8 = path.generic_u8string();
  if (!path.is_absolute() || utf8.empty() || utf8.size() > runtime::PluginHost::kMaximumPathBytes ||
      std::ranges::find(utf8, char8_t{}) != utf8.end() ||
      !foundation::IsValidUtf8({reinterpret_cast<const char *>(utf8.data()), utf8.size()}))
    return {};
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec || !std::filesystem::is_regular_file(status) ||
      std::filesystem::hard_link_count(path, ec) != 1 || ec)
    return {};
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return {};
  std::vector<std::byte> bytes;
  std::array<char, 16384> chunk{};
  while (input) {
    input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    const auto count = static_cast<std::size_t>(input.gcount());
    if (count > kMaximumPackageBytes - bytes.size())
      return {};
    if (bytes.size() + count > bytes.capacity())
      bytes.reserve(
          std::min(kMaximumPackageBytes, std::max(bytes.size() + count, bytes.capacity() * 2)));
    const auto *begin = reinterpret_cast<const std::byte *>(chunk.data());
    bytes.insert(bytes.end(), begin, begin + count);
  }
  return input.bad() ? std::nullopt : std::optional(std::move(bytes));
}
std::optional<PreparedExtension> PluginManager::Verify(std::span<const std::byte> bytes,
                                                       ExtensionAdmissionError *error) const {
  auto p = DecodePackage(bytes);
  if (!p) {
    if (error)
      *error = ExtensionAdmissionError::InvalidManifest;
    return {};
  }
  return host_.Prepare(std::move(*p), error);
}
bool PluginManager::Review(const ProjectWorkspace &workspace, const std::filesystem::path &path,
                           std::string *error) {
  CancelReview();
  if (!Matches(workspace) || workspace.HasRecoveryJournal() || workspace.HasExternalChange())
    return Fail(error, "Extension review requires the current project with resolved recovery.");
  auto bytes = ReadPackageFile(path);
  ExtensionAdmissionError admission{};
  const auto prepared = bytes ? Verify(*bytes, &admission) : std::nullopt;
  if (!prepared)
    return Fail(error, "Package is unreadable, untrusted or denied by current extension policy.");
  review_ = ExtensionPackageReview{prepared->Manifest(), bytes->size(), scope_, configuration_};
  reviewed_bytes_ = std::move(*bytes);
  return true;
}
std::optional<ExtensionPackageReview> PluginManager::ReviewSnapshot() const { return review_; }
void PluginManager::CancelReview() noexcept {
  review_.reset();
  std::vector<std::byte>{}.swap(reviewed_bytes_);
}
bool PluginManager::Install(const ProjectWorkspace &workspace, std::uint64_t scope,
                            std::uint64_t configuration, std::string *error) {
  if (!Writable(workspace) || !review_ || scope != scope_ || configuration != configuration_ ||
      review_->scope != scope || review_->configuration != configuration)
    return Fail(error, "Installation requires a current verified review and writable project.");
  ExtensionAdmissionError admission{};
  const auto prepared = Verify(reviewed_bytes_, &admission);
  if (!prepared || prepared->Manifest() != review_->manifest)
    return Fail(error, "Reviewed package is no longer authorized; no files were installed.");
  const auto relative = Destination(review_->manifest);
  const auto found = std::ranges::find(entries_, relative, &ManagedExtension::relative_path);
  if (found == entries_.end() && entries_.size() == kMaximumPackages)
    return Fail(error, "Installed extension capacity is exhausted.");
  ManagedExtension entry{review_->manifest, relative, reviewed_bytes_.size()};
  if (!Directory(root_ / ".nexora", false, error) ||
      !Directory(root_ / ".nexora/extensions", true, error))
    return false;
  const auto path = root_ / relative;
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (!Missing(status, ec)) {
    const auto existing = ReadPackageFile(path);
    if (!existing || *existing != reviewed_bytes_)
      return Fail(
          error,
          "Immutable installed identity/version is already occupied; preserve existing bytes.");
  } else if (!detail::AtomicWriteWith(
                 path,
                 [&](std::ostream &output) {
                   output.write(reinterpret_cast<const char *>(reviewed_bytes_.data()),
                                static_cast<std::streamsize>(reviewed_bytes_.size()));
                 },
                 error))
    return false;
  const auto installed = ReadPackageFile(path);
  if (!installed || *installed != reviewed_bytes_)
    return Fail(error, "Installed bytes could not be confirmed; preserve the file for inspection.");
  if (found == entries_.end())
    entries_.push_back(std::move(entry));
  else if (found->manifest != entry.manifest)
    return Fail(error, "Existing installed observation differs; refresh before installation.");
  CancelReview();
  static_cast<void>(Configure());
  Poll();
  return true;
}
bool PluginManager::Refresh(const ProjectWorkspace &workspace, std::string *error) {
  if (!Matches(workspace) || workspace.HasRecoveryJournal() || workspace.HasExternalChange())
    return Fail(error, "Extension inspection requires the current resolved project.");
  if (!Directory(root_ / ".nexora", false, error))
    return false;
  const auto directory = root_ / ".nexora/extensions";
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(directory, ec);
  if (Missing(status, ec)) {
    host_.RequestUnloadAll();
    entries_.clear();
    rejected_files_ = 0;
    return Configure();
  }
  if (ec || !std::filesystem::is_directory(status))
    return Fail(error, "Extension directory is aliased or unavailable.");
  std::vector<ManagedExtension> staged;
  staged.reserve(kMaximumPackages);
  std::size_t encountered{}, rejected{};
  std::filesystem::directory_iterator iterator(directory, ec), end;
  if (ec)
    return Fail(error, "Extension directory could not be inspected.");
  while (iterator != end) {
    if (++encountered > 64)
      return Fail(error, "Extension directory exceeds bounded inspection capacity.");
    const auto path = iterator->path();
    if (path.extension() == ".nxpkg") {
      const auto bytes = ReadPackageFile(path);
      const auto package = bytes ? DecodePackage(*bytes) : std::nullopt;
      const auto manifest =
          package ? SignedExtensionHost::DecodeManifest(package->manifest) : std::nullopt;
      if (!manifest || path.filename() != Destination(*manifest).filename())
        ++rejected;
      else {
        if (staged.size() == kMaximumPackages)
          return Fail(error, "Installed extensions exceed bounded package capacity.");
        ManagedExtension entry{*manifest, Destination(*manifest), bytes->size()};
        const auto previous =
            std::ranges::find(entries_, entry.relative_path, &ManagedExtension::relative_path);
        if (previous != entries_.end() && previous->manifest == entry.manifest)
          entry = *previous;
        staged.push_back(std::move(entry));
      }
    }
    iterator.increment(ec);
    if (ec)
      return Fail(error, "Extension directory changed during inspection.");
  }
  for (const auto &previous : entries_)
    if (previous.native_id && std::ranges::none_of(staged, [&](const auto &entry) {
          return entry.native_id == previous.native_id;
        }))
      static_cast<void>(host_.RequestUnload(previous.native_id));
  entries_.swap(staged);
  rejected_files_ = rejected;
  static_cast<void>(Configure());
  for (auto &entry : entries_) {
    const auto bytes = ReadPackageFile(root_ / entry.relative_path);
    const auto prepared = bytes ? Verify(*bytes, &entry.error) : std::nullopt;
    if (!prepared || prepared->Manifest() != entry.manifest ||
        bytes->size() != entry.package_bytes) {
      if (entry.native_id)
        static_cast<void>(host_.RequestUnload(entry.native_id));
      entry.state = ManagedExtensionState::Rejected;
      if (!bytes)
        entry.error = ExtensionAdmissionError::InvalidManifest;
      else if (prepared)
        entry.error = ExtensionAdmissionError::ArtifactMismatch;
    } else if (!entry.native_id) {
      entry.error = ExtensionAdmissionError::None;
      entry.state = ManagedExtensionState::Disabled;
    }
  }
  Poll();
  return true;
}
bool PluginManager::Enable(const ProjectWorkspace &workspace, std::string_view id,
                           std::string_view version, std::string *error) {
  if (!Writable(workspace))
    return Fail(error, "Enable requires the current writable, resolved project.");
  const auto found = std::ranges::find_if(entries_, [&](const auto &entry) {
    return entry.manifest.id == id && entry.manifest.version == version;
  });
  if (found == entries_.end())
    return Fail(error, "Installed extension identity/version was not found.");
  Poll();
  if (found->state == ManagedExtensionState::Loaded)
    return true;
  const auto native = host_.Snapshot();
  if (std::ranges::any_of(native, [](const auto &entry) {
        return entry.state == runtime::PluginState::RestartRequired ||
               entry.state == runtime::PluginState::ShutdownPending;
      }))
    return Fail(error, "Previous native work must drain or restart before enable.");
  for (const auto &dependency : found->manifest.dependencies)
    if (!BuiltinDependency(dependency) && std::ranges::none_of(entries_, [&](const auto &entry) {
          return entry.manifest.id == dependency && entry.state == ManagedExtensionState::Loaded;
        }))
      return Fail(error, "Required installed dependency is not enabled.");
  if (!Directory(root_ / ".nexora", false, error) ||
      !Directory(root_ / ".nexora/extensions", false, error))
    return false;
  const auto bytes = ReadPackageFile(root_ / found->relative_path);
  ExtensionAdmissionError admission{};
  const auto prepared = bytes ? Verify(*bytes, &admission) : std::nullopt;
  if (!prepared || prepared->Manifest() != found->manifest ||
      bytes->size() != found->package_bytes) {
    found->state = ManagedExtensionState::Rejected;
    found->error = bytes ? admission : ExtensionAdmissionError::InvalidManifest;
    if (prepared)
      found->error = ExtensionAdmissionError::ArtifactMismatch;
    return Fail(error, "Installed bytes no longer match the verified identity/version/policy.");
  }
  const auto result = host_.Load(*prepared, &services_);
  found->native_id = result.native.id;
  found->error = result.error;
  found->native_error = result.native.error;
  found->reported_abi = result.native.reported_abi;
  found->cooperative = result.native.cooperative;
  found->state =
      result.native.loaded ? ManagedExtensionState::Loaded : ManagedExtensionState::Rejected;
  Poll();
  return result.native.loaded
             ? true
             : Fail(error, "Native enable failed; inspect the actual rejection/restart state.");
}
bool PluginManager::Disable(std::string_view id, std::string_view version) {
  const auto found = std::ranges::find_if(entries_, [&](const auto &entry) {
    return entry.manifest.id == id && entry.manifest.version == version;
  });
  if (found == entries_.end())
    return false;
  if (found->native_id)
    found->state = State(host_.RequestUnload(found->native_id));
  Poll();
  return true;
}
bool PluginManager::Remove(const ProjectWorkspace &workspace, std::string_view id,
                           std::string_view version, std::string *error) {
  if (!Writable(workspace))
    return Fail(error, "Remove requires the current writable, resolved project.");
  Poll();
  const auto found = std::ranges::find_if(entries_, [&](const auto &entry) {
    return entry.manifest.id == id && entry.manifest.version == version;
  });
  if (found == entries_.end())
    return Fail(error, "Installed extension was not found.");
  if (found->native_id) {
    const auto native = host_.Snapshot();
    const auto status = std::ranges::find(native, found->native_id, &runtime::PluginSnapshot::id);
    if (status == native.end() || status->state != runtime::PluginState::Unloaded)
      return Fail(error, "Disable and drain native code before removing an installed package.");
  }
  if (!Directory(root_ / ".nexora", false, error) ||
      !Directory(root_ / ".nexora/extensions", false, error))
    return false;
  const auto bytes = ReadPackageFile(root_ / found->relative_path);
  const auto package = bytes ? DecodePackage(*bytes) : std::nullopt;
  const auto manifest =
      package ? SignedExtensionHost::DecodeManifest(package->manifest) : std::nullopt;
  if (!manifest || *manifest != found->manifest || bytes->size() != found->package_bytes)
    return Fail(error, "Installed file changed; preserve it and refresh before removing.");
  // Revalidate actual artifact/signature before deleting the known installed package.
  const auto verified = trust_.Verify(manifest->publisher, package->manifest, package->signature);
  const auto digest = cryptography::Sha256(package->artifact);
  if (verified.status != ExtensionTrustStatus::Verified || !digest ||
      *digest != manifest->artifact_digest)
    return Fail(error, "Untrusted or changed package is preserved for inspection.");
  std::error_code ec;
  if (!std::filesystem::remove(root_ / found->relative_path, ec) || ec)
    return Fail(error, "Installed package could not be removed.");
  entries_.erase(found);
  static_cast<void>(Configure());
  Poll();
  return true;
}
void PluginManager::Poll() noexcept {
  host_.PollShutdown();
  try {
    const auto native = host_.Snapshot();
    for (auto &entry : entries_)
      if (entry.native_id) {
        const auto found = std::ranges::find(native, entry.native_id, &runtime::PluginSnapshot::id);
        if (found != native.end()) {
          entry.native_error = found->load_error;
          entry.lifecycle_error = found->lifecycle_error;
          entry.lifecycle_result = found->lifecycle_result;
          entry.reported_abi = found->reported_abi;
          entry.registered_services = found->registered_services;
          entry.cooperative = found->cooperative;
          if (found->state == runtime::PluginState::Unloaded &&
              entry.error != ExtensionAdmissionError::None)
            entry.state = ManagedExtensionState::Rejected;
          else
            entry.state = State(found->state);
        }
      }
  } catch (...) {
    // Inspection allocation failure never proves native shutdown or permits descriptor reuse.
  }
}
std::vector<ManagedExtension> PluginManager::Snapshot() const { return entries_; }
void *PluginManager::FindService(std::string_view name) const { return services_.Find(name); }
std::optional<ManagedToolSelection> PluginManager::SelectTool(std::string_view id,
                                                              std::string_view version) const {
  if (tool_invoker_.ContextState() != NativeToolState::Success)
    return {};
  if (!instance_ || root_.empty() || id.empty() || id.size() > 128 || version.empty() ||
      version.size() > 64)
    return {};
  const auto found = std::ranges::find_if(entries_, [&](const auto &entry) {
    return entry.manifest.id == id && entry.manifest.version == version && entry.native_id &&
           entry.state == ManagedExtensionState::Loaded;
  });
  if (found == entries_.end())
    return {};
  return ManagedToolSelection{instance_,          scope_,
                              configuration_,     found->native_id,
                              found->manifest.id, found->manifest.version};
}
NativeToolOutcome PluginManager::InvokeTool(const ProjectWorkspace &workspace,
                                            const ManagedToolSelection &selection,
                                            std::string_view service, NativeToolOperation operation,
                                            std::span<const std::byte> input) {
  const auto context = tool_invoker_.ContextState();
  if (context != NativeToolState::Success)
    return {context, {}, {}, "Managed tool invocation requires its nonreentrant owner context"};
  const auto fail = [](const char *message) {
    return NativeToolOutcome{NativeToolState::Unavailable, {}, {}, message};
  };
  if (!instance_ || selection.manager != instance_ || selection.scope != scope_ ||
      selection.configuration != configuration_ || !selection.native_id || selection.id.empty() ||
      selection.id.size() > 128 || selection.version.empty() || selection.version.size() > 64)
    return fail("Managed tool selection is stale or foreign");
  if (!Matches(workspace) || workspace.HasRecoveryJournal() || workspace.HasExternalChange())
    return fail("Managed tool project scope is unavailable or unresolved");
  if (operation == NativeToolOperation::Edit && !workspace.Writable())
    return {NativeToolState::Rejected, {}, {}, "Managed tool edits require a writable project"};
  const auto found = std::ranges::find_if(entries_, [&](const auto &entry) {
    return entry.manifest.id == selection.id && entry.manifest.version == selection.version &&
           entry.native_id == selection.native_id && entry.state == ManagedExtensionState::Loaded;
  });
  if (found == entries_.end())
    return fail("Selected managed tool admission is no longer enabled");
  return host_.InvokeTool(tool_invoker_, services_, found->native_id, service, operation, input);
}
bool PluginManager::RestartRequired() const {
  const auto native = host_.Snapshot();
  return std::ranges::any_of(native, [](const auto &entry) {
    return entry.state == runtime::PluginState::RestartRequired;
  });
}
} // namespace nexora::editor
