#include "Nexora/Editor/SignedExtensionHost.h"
#include <algorithm>
#include <limits>
#if defined(__linux__)
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace nexora::editor {
namespace {
constexpr std::string_view magic = "NXEXTM1\n";
bool Identifier(std::string_view value, std::size_t maximum = 128) {
  const auto alpha = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
  };
  return !value.empty() && value.size() <= maximum && alpha(value.front()) &&
         std::ranges::all_of(value,
                             [&](char c) { return alpha(c) || c == '.' || c == '_' || c == '-'; });
}
bool Dependencies(std::span<const std::string> values) {
  return values.size() <= SignedExtensionHost::kMaximumDependencies &&
         std::ranges::is_sorted(values) &&
         std::adjacent_find(values.begin(), values.end()) == values.end() &&
         std::ranges::all_of(values, [](const auto &v) { return Identifier(v); });
}
bool Valid(const ExtensionManifest &m) {
  return Identifier(m.id) && Identifier(m.version, 64) && Identifier(m.publisher) &&
         Identifier(m.target, 64) && m.engine_abi &&
         !(m.permissions & ~SignedExtensionHost::kKnownPermissions) &&
         Dependencies(m.dependencies) &&
         !std::ranges::all_of(m.artifact_digest, [](auto byte) { return byte == std::byte{}; });
}
void U32(std::vector<std::byte> &bytes, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i)
    bytes.push_back(static_cast<std::byte>(value >> (i * 8)));
}
void Text(std::vector<std::byte> &bytes, std::string_view text) {
  U32(bytes, static_cast<std::uint32_t>(text.size()));
  for (const auto c : text)
    bytes.push_back(static_cast<std::byte>(c));
}
struct Reader final {
  std::span<const std::byte> bytes;
  std::size_t offset{};
  std::optional<std::uint32_t> Number() {
    if (offset > bytes.size() || bytes.size() - offset < 4)
      return {};
    std::uint32_t value{};
    for (unsigned i = 0; i < 4; ++i)
      value |= std::to_integer<std::uint32_t>(bytes[offset++]) << (i * 8);
    return value;
  }
  bool TextValue(std::string &text, std::size_t maximum = 128) {
    const auto length = Number();
    if (!length || *length > maximum || *length > bytes.size() - offset)
      return false;
    text.clear();
    text.reserve(*length);
    for (std::uint32_t i = 0; i < *length; ++i)
      text.push_back(static_cast<char>(bytes[offset++]));
    return true;
  }
};
bool Same(std::span<const std::byte> a, std::span<const std::byte> b) {
  return a.size() == b.size() && std::ranges::equal(a, b);
}
struct FileGuard final {
  int fd{-1};
  ~FileGuard() {
#if defined(__linux__)
    if (fd >= 0)
      static_cast<void>(close(fd));
#endif
  }
};
} // namespace
SignedExtensionHost::SignedExtensionHost(ExtensionTrust &trust, std::uint32_t engine_abi)
    : trust_(trust), engine_abi_(engine_abi), native_(engine_abi) {
  images_.reserve(runtime::PluginHost::kMaximumPlugins);
}
SignedExtensionHost::~SignedExtensionHost() {
  for (const auto &image : images_)
    static_cast<void>(native_.RequestUnload(image.id));
  native_.PollShutdown();
  ReleaseUnloaded();
  // Pending/legacy mappings intentionally retain their unique proc/fd image identity until restart.
  // NativeHost also keeps those unsafe-to-unmap images mapped; never recycle their descriptor path.
}
bool SignedExtensionHost::SetPolicy(ExtensionAdmissionPolicy policy) {
  if (policy.engine_abi != engine_abi_ || !policy.engine_abi || !Identifier(policy.target, 64) ||
      (policy.allowed_permissions & ~kKnownPermissions) ||
      !Dependencies(policy.available_dependencies) ||
      policy_revision_ == std::numeric_limits<std::uint64_t>::max())
    return false;
  if (policy == policy_)
    return true;
  policy_ = std::move(policy);
  ++policy_revision_;
  for (const auto &image : images_)
    if (image.fd >= 0)
      static_cast<void>(native_.RequestUnload(image.id));
  native_.PollShutdown();
  ReleaseUnloaded();
  return true;
}
bool SignedExtensionHost::PolicyAllows(const ExtensionManifest &manifest) const {
  if (policy_.engine_abi != manifest.engine_abi || policy_.target != manifest.target ||
      (manifest.permissions & ~policy_.allowed_permissions))
    return false;
  for (const auto &dependency : manifest.dependencies)
    if (!std::ranges::binary_search(policy_.available_dependencies, dependency))
      return false;
  return true;
}
std::optional<std::vector<std::byte>>
SignedExtensionHost::EncodeManifest(const ExtensionManifest &manifest) {
  if (!Valid(manifest))
    return {};
  std::vector<std::byte> bytes;
  bytes.reserve(1024);
  for (const auto c : magic)
    bytes.push_back(static_cast<std::byte>(c));
  U32(bytes, manifest.engine_abi);
  U32(bytes, manifest.permissions);
  Text(bytes, manifest.id);
  Text(bytes, manifest.version);
  Text(bytes, manifest.publisher);
  Text(bytes, manifest.target);
  bytes.insert(bytes.end(), manifest.artifact_digest.begin(), manifest.artifact_digest.end());
  U32(bytes, static_cast<std::uint32_t>(manifest.dependencies.size()));
  for (const auto &dependency : manifest.dependencies)
    Text(bytes, dependency);
  if (bytes.size() > kMaximumManifestBytes)
    return {};
  return bytes;
}
std::optional<ExtensionManifest>
SignedExtensionHost::DecodeManifest(std::span<const std::byte> bytes) {
  if (bytes.size() < magic.size() || bytes.size() > kMaximumManifestBytes)
    return {};
  for (std::size_t i = 0; i < magic.size(); ++i)
    if (bytes[i] != static_cast<std::byte>(magic[i]))
      return {};
  Reader reader{bytes, magic.size()};
  ExtensionManifest manifest;
  const auto abi = reader.Number(), permissions = reader.Number();
  if (!abi || !permissions || !reader.TextValue(manifest.id) ||
      !reader.TextValue(manifest.version, 64) || !reader.TextValue(manifest.publisher) ||
      !reader.TextValue(manifest.target, 64) ||
      bytes.size() - reader.offset < manifest.artifact_digest.size())
    return {};
  manifest.engine_abi = *abi;
  manifest.permissions = *permissions;
  std::ranges::copy(bytes.subspan(reader.offset, manifest.artifact_digest.size()),
                    manifest.artifact_digest.begin());
  reader.offset += manifest.artifact_digest.size();
  const auto count = reader.Number();
  if (!count || *count > kMaximumDependencies)
    return {};
  for (std::uint32_t i = 0; i < *count; ++i) {
    std::string dependency;
    if (!reader.TextValue(dependency))
      return {};
    manifest.dependencies.push_back(std::move(dependency));
  }
  if (reader.offset != bytes.size())
    return {};
  const auto canonical = EncodeManifest(manifest);
  return canonical && Same(*canonical, bytes) ? std::optional(std::move(manifest)) : std::nullopt;
}
std::optional<PreparedExtension>
SignedExtensionHost::Prepare(SignedExtensionPackage package, ExtensionAdmissionError *error) const {
  const auto fail = [&](ExtensionAdmissionError reason) -> std::optional<PreparedExtension> {
    if (error)
      *error = reason;
    return {};
  };
  if (package.artifact.empty() || package.artifact.size() > cryptography::kMaximumMessageBytes)
    return fail(ExtensionAdmissionError::BudgetExceeded);
  const auto manifest = DecodeManifest(package.manifest);
  if (!manifest)
    return fail(ExtensionAdmissionError::InvalidManifest);
  if (!PolicyAllows(*manifest))
    return fail(ExtensionAdmissionError::PolicyRejected);
  const auto verified = trust_.Verify(manifest->publisher, package.manifest, package.signature);
  if (verified.status == ExtensionTrustStatus::BackendUnavailable ||
      verified.status == ExtensionTrustStatus::BackendFailure)
    return fail(ExtensionAdmissionError::BackendUnavailable);
  if (verified.status != ExtensionTrustStatus::Verified)
    return fail(ExtensionAdmissionError::UntrustedOrInvalidSignature);
  const auto digest = cryptography::Sha256(package.artifact);
  if (!digest || *digest != manifest->artifact_digest)
    return fail(ExtensionAdmissionError::ArtifactMismatch);
  PreparedExtension prepared;
  prepared.manifest_ = *manifest;
  prepared.package_ = std::move(package);
  prepared.trust_revision = verified.trust_revision;
  prepared.policy_revision = policy_revision_;
  if (error)
    *error = ExtensionAdmissionError::None;
  return prepared;
}
SignedExtensionLoadResult SignedExtensionHost::Load(const PreparedExtension &prepared,
                                                    runtime::ServiceRegistry *services) {
  const auto fail = [](ExtensionAdmissionError error) {
    return SignedExtensionLoadResult{error, {}};
  };
  if (prepared.trust_revision != trust_.Revision() ||
      prepared.policy_revision != policy_revision_ || !PolicyAllows(prepared.manifest_))
    return fail(ExtensionAdmissionError::StaleAdmission);
  // Prepared values have no public byte mutation. Re-verify against the current trusted registry;
  // retained/cross-host observations never substitute for current signed-byte authorization.
  const auto verified = trust_.Verify(prepared.manifest_.publisher, prepared.package_.manifest,
                                      prepared.package_.signature);
  const auto digest = cryptography::Sha256(prepared.package_.artifact);
  if (verified.status != ExtensionTrustStatus::Verified ||
      verified.trust_revision != prepared.trust_revision || !digest ||
      *digest != prepared.manifest_.artifact_digest)
    return fail(ExtensionAdmissionError::StaleAdmission);
  if (images_.size() == runtime::PluginHost::kMaximumPlugins)
    return fail(ExtensionAdmissionError::BudgetExceeded);
  if (std::ranges::any_of(images_, [&](const auto &image) {
        return image.fd >= 0 && image.identity == prepared.manifest_.id;
      }))
    return fail(ExtensionAdmissionError::DuplicateIdentity);
#if defined(__linux__)
  if (prepared.manifest_.target != "linux-x86_64" && prepared.manifest_.target != "linux-aarch64")
    return fail(ExtensionAdmissionError::BackendUnavailable);
#if defined(__x86_64__)
  if (prepared.manifest_.target != "linux-x86_64")
    return fail(ExtensionAdmissionError::BackendUnavailable);
#elif defined(__aarch64__)
  if (prepared.manifest_.target != "linux-aarch64")
    return fail(ExtensionAdmissionError::BackendUnavailable);
#else
  return fail(ExtensionAdmissionError::BackendUnavailable);
#endif
  FileGuard image{memfd_create("nexora-signed-extension", MFD_CLOEXEC | MFD_ALLOW_SEALING)};
  if (image.fd < 0)
    return fail(ExtensionAdmissionError::BackendUnavailable);
  std::size_t written{};
  const auto &artifact = prepared.package_.artifact;
  while (written < artifact.size()) {
    const auto count = write(image.fd, artifact.data() + written, artifact.size() - written);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0)
      return fail(ExtensionAdmissionError::BackendUnavailable);
    written += static_cast<std::size_t>(count);
  }
  constexpr int seals = F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;
  struct stat info{};
  if (fstat(image.fd, &info) != 0 || info.st_size < 0 ||
      static_cast<std::uint64_t>(info.st_size) != artifact.size() ||
      fcntl(image.fd, F_ADD_SEALS, seals) != 0)
    return fail(ExtensionAdmissionError::BackendUnavailable);
  const auto actual_seals = fcntl(image.fd, F_GET_SEALS);
  if (actual_seals < 0 || (actual_seals & seals) != seals)
    return fail(ExtensionAdmissionError::BackendUnavailable);
  const auto path = "/proc/self/fd/" + std::to_string(image.fd);
  // All byte/policy checks precede this call; the OS loader can run native constructors here.
  const auto loaded = native_.Load(path, services);
  if (loaded.id) {
    images_.push_back({loaded.id, prepared.manifest_.id, image.fd, prepared.trust_revision,
                       prepared.policy_revision});
    image.fd = -1;
  }
  if (!loaded.loaded)
    return {ExtensionAdmissionError::NativeLoadFailed, loaded};
  return {ExtensionAdmissionError::None, loaded};
#else
  static_cast<void>(services);
  return fail(ExtensionAdmissionError::BackendUnavailable);
#endif
}
void SignedExtensionHost::ReleaseUnloaded() noexcept {
  // Snapshot allocates; native teardown must retain descriptors on inspection allocation failure.
  try {
    const auto snapshot = native_.Snapshot();
    for (auto &image : images_) {
      const auto found = std::ranges::find(snapshot, image.id, &runtime::PluginSnapshot::id);
      if (image.fd >= 0 && found != snapshot.end() &&
          found->state == runtime::PluginState::Unloaded) {
#if defined(__linux__)
        static_cast<void>(close(image.fd));
#endif
        image.fd = -1;
      }
    }
  } catch (...) {
  }
}
runtime::PluginState SignedExtensionHost::RequestUnload(std::uint64_t id) noexcept {
  const auto state = native_.RequestUnload(id);
  ReleaseUnloaded();
  return state;
}
void SignedExtensionHost::PollShutdown() noexcept {
  for (const auto &image : images_)
    if (image.fd >= 0 &&
        (image.trust_revision != trust_.Revision() || image.policy_revision != policy_revision_))
      static_cast<void>(native_.RequestUnload(image.id));
  native_.PollShutdown();
  ReleaseUnloaded();
}
std::vector<runtime::PluginSnapshot> SignedExtensionHost::Snapshot() const {
  return native_.Snapshot();
}
} // namespace nexora::editor
