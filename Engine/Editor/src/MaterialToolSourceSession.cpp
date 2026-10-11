#include "Nexora/Editor/MaterialToolSourceSession.h"
#include "AtomicFile.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <limits>

namespace nexora::editor {
namespace {
bool Fail(std::string *error, const char *message) {
  if (error)
    *error = message;
  return false;
}
bool Directory(const std::filesystem::path &path) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  return !ec && std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status);
}
bool MaterialPath(const std::filesystem::path &root, const std::filesystem::path &relative) {
  const auto text = detail::PathUtf8(relative);
  if (text.empty() || text.size() > 1024 || text.find('\0') != text.npos ||
      !foundation::IsValidUtf8(text) || relative.is_absolute() || relative.has_root_name() ||
      relative.empty() || *relative.begin() != "Content" || !Directory(root))
    return false;
  if (std::ranges::any_of(relative, [](const auto &part) { return part == "." || part == ".."; }))
    return false;
  std::error_code root_error;
  if (std::filesystem::canonical(root, root_error) != root.lexically_normal() || root_error)
    return false;
  auto parent = root;
  for (const auto &part : relative.parent_path()) {
    parent /= part;
    if (!Directory(parent))
      return false;
  }
  return true;
}
std::optional<std::string> Read(const std::filesystem::path &root,
                                const std::filesystem::path &relative) {
  if (!MaterialPath(root, relative))
    return {};
  const auto path = root / relative;
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec || !std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status) ||
      std::filesystem::hard_link_count(path, ec) != 1 || ec ||
      std::filesystem::file_size(path, ec) > kMaximumMaterialSourceBytes || ec)
    return {};
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return {};
  std::string bytes;
  std::array<char, 8192> chunk{};
  while (input) {
    input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    const auto count = static_cast<std::size_t>(input.gcount());
    if (count > kMaximumMaterialSourceBytes - bytes.size())
      return {};
    bytes.append(chunk.data(), count);
  }
  return input.eof() && !input.bad() ? std::optional(std::move(bytes)) : std::nullopt;
}
} // namespace
std::uint64_t MaterialToolSourceSession::AllocateInstance() noexcept {
  static std::atomic<std::uint64_t> next{1};
  auto value = next.load(std::memory_order_relaxed);
  while (value) {
    const auto following = value == std::numeric_limits<std::uint64_t>::max() ? 0 : value + 1;
    if (next.compare_exchange_weak(value, following, std::memory_order_relaxed))
      return value;
  }
  return 0;
}
MaterialToolSourceSession::MaterialToolSourceSession(const ProjectWorkspace &workspace,
                                                     ProjectContentSession &content)
    : workspace_(workspace), content_(content) {}
bool MaterialToolSourceSession::Owner(std::string *error) const {
  return owner_ == std::this_thread::get_id() ||
         Fail(error, "Material source session requires its construction thread");
}
bool MaterialToolSourceSession::Workspace(std::string *error) const {
  if (!Owner(error))
    return false;
  return (!workspace_.Root().empty() && content_.Root() == workspace_.Root() &&
          content_.Browser().ProjectGeneration() && !workspace_.HasRecoveryJournal() &&
          !workspace_.HasExternalChange() && !content_.ReimportBusy()) ||
         Fail(error, "Material source workspace is unavailable or unresolved");
}
bool MaterialToolSourceSession::Open(runtime::AssetUuid asset, bool discard_dirty,
                                     std::string *error) {
  if (!Workspace(error))
    return false;
  const auto *item = content_.Browser().Find(asset);
  if (!item || item->type != ".nmaterial")
    return Fail(error, "Material asset is unavailable");
  auto root = workspace_.Root(), path = item->path;
  const auto source = Read(root, path);
  if (!source)
    return Fail(error, "Material source is unavailable, aliased or oversized");
  const auto content_generation = content_.Browser().ProjectGeneration();
  const MaterialToolScope scope{workspace_.Project().id, asset, instance_};
  if (!document_.Open(scope, *source, discard_dirty, error))
    return false;
  root_ = std::move(root);
  path_ = std::move(path);
  content_generation_ = content_generation;
  return true;
}
std::optional<MaterialToolSnapshot> MaterialToolSourceSession::Snapshot() const {
  if (!Owner(nullptr))
    return {};
  return document_.Snapshot();
}
bool MaterialToolSourceSession::Matches(MaterialToolScope scope, std::uint64_t serial,
                                        bool authoring_allowed, std::string *error) const {
  if (!Owner(error) || !document_.Matches(scope, serial, error) || !Workspace(error))
    return false;
  const auto *item = content_.Browser().Find(scope.asset);
  return (root_ == workspace_.Root() && scope.project == workspace_.Project().id &&
          scope.generation == instance_ &&
          content_generation_ == content_.Browser().ProjectGeneration() && item &&
          item->path == path_ && item->type == ".nmaterial" && authoring_allowed &&
          workspace_.Writable() && content_.Writable()) ||
         Fail(error, "Material source scope is stale or authoring is unavailable");
}
std::optional<std::string> MaterialToolSourceSession::CurrentSource(MaterialToolScope scope,
                                                                    std::string *error) const {
  const auto source = Read(root_, path_);
  if (!source || *source != document_.saved_source_ || scope != document_.scope_) {
    Fail(error, "Material source no longer matches the exact saved baseline");
    return {};
  }
  return source;
}
bool MaterialToolSourceSession::Apply(MaterialToolScope scope, std::uint64_t serial,
                                      std::span<const std::byte> bytes, bool allowed,
                                      std::string *error) {
  return Matches(scope, serial, allowed, error) && CurrentSource(scope, error) &&
         document_.Apply(scope, serial, bytes, true, error);
}
bool MaterialToolSourceSession::Undo(MaterialToolScope scope, std::uint64_t serial, bool allowed,
                                     std::string *error) {
  return Matches(scope, serial, allowed, error) && CurrentSource(scope, error) &&
         document_.Undo(scope, serial, true, error);
}
bool MaterialToolSourceSession::Redo(MaterialToolScope scope, std::uint64_t serial, bool allowed,
                                     std::string *error) {
  return Matches(scope, serial, allowed, error) && CurrentSource(scope, error) &&
         document_.Redo(scope, serial, true, error);
}
bool MaterialToolSourceSession::Save(MaterialToolScope scope, std::uint64_t serial, bool allowed,
                                     std::string *error) {
  if (!Matches(scope, serial, allowed, error) || !CurrentSource(scope, error))
    return false;
  const auto snapshot = document_.Snapshot();
  if (!snapshot)
    return Fail(error, "Material document is unavailable");
  // Every saved-baseline allocation precedes publication. Confirmation only moves owning strings.
  std::string next_saved(snapshot->source), next_canonical(snapshot->source);
  if (!Matches(scope, serial, allowed, error) || !CurrentSource(scope, error) ||
      !detail::AtomicWrite(root_ / path_, snapshot->source, error))
    return false;
  const auto confirmed = Read(root_, path_);
  if (!confirmed || *confirmed != snapshot->source || !Matches(scope, serial, allowed, nullptr))
    return Fail(error,
                "Material source was published but current scope/source could not be confirmed");
  std::string refresh_error;
  if (!content_.Reimport(scope.asset, &refresh_error)) {
    if (error)
      *error = "Material source was published but Content refresh failed; owner remains "
               "unacknowledged: " +
               refresh_error;
    return false;
  }
  const auto refreshed = Read(root_, path_);
  if (!refreshed || *refreshed != snapshot->source || !Matches(scope, serial, allowed, nullptr))
    return Fail(error, "Material source was published but changed before acknowledgement");
  document_.saved_source_ = std::move(next_saved);
  document_.saved_canonical_ = std::move(next_canonical);
  ++document_.serial_;
  if (error)
    error->clear();
  return true;
}
bool MaterialToolSourceSession::Close(MaterialToolScope scope, std::uint64_t serial,
                                      bool discard_dirty, std::string *error) {
  if (!Owner(error) || !document_.Close(scope, serial, discard_dirty, error))
    return false;
  root_.clear();
  path_.clear();
  content_generation_ = 0;
  return true;
}
} // namespace nexora::editor
