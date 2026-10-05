#include "Nexora/Editor/SceneFiles.h"
#include "AtomicFile.h"
#include "Nexora/Editor/ProjectContent.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <system_error>

namespace nexora::editor {
namespace {
SceneFileResult Rejected(std::string message) {
  return {SceneFileStatus::Rejected, std::move(message)};
}
bool ReservedPathAllowed(const std::filesystem::path &relative) {
  const auto lower = [](const std::filesystem::path &part) {
    const auto encoded = part.generic_u8string();
    std::string text(encoded.begin(), encoded.end());
    std::ranges::transform(text, text.begin(), [](unsigned char c) {
      return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    });
    return text;
  };
  auto part = relative.begin();
  if (part == relative.end() || lower(*part) != ".nexora")
    return true;
  return ++part != relative.end() && lower(*part) == "scenes";
}
bool Inside(const std::filesystem::path &root, const std::filesystem::path &path) {
  auto candidate = path.begin();
  for (auto part = root.begin(); part != root.end(); ++part, ++candidate)
    if (candidate == path.end() || *candidate != *part)
      return false;
  return candidate != path.end();
}
} // namespace
SceneFileSession::SceneFileSession(const ProjectWorkspace &workspace, SceneDocument &document)
    : workspace_(workspace), document_(document), root_(workspace.Root()),
      project_(workspace.Project().id), generation_(document.Generation()) {}
SceneFileToken SceneFileSession::Token() const noexcept { return {project_, generation_}; }
std::optional<std::filesystem::path> SceneFileSession::CurrentPath() const { return current_; }
bool SceneFileSession::Live(SceneFileToken token) const noexcept {
  return !root_.empty() && workspace_.Root() == root_ && workspace_.Project().id == project_ &&
         token == Token() && document_.Generation() == generation_;
}
std::optional<std::filesystem::path>
SceneFileSession::Resolve(const std::filesystem::path &relative) const {
  const auto utf8 = relative.generic_u8string();
  const std::string text(utf8.begin(), utf8.end());
  if (text.empty() || text.size() >= 1024 || !foundation::IsValidUtf8(text) ||
      relative.has_root_path() || relative.extension() != ".scene" ||
      text.find_first_of("\\:<>\"|?*") != std::string::npos ||
      std::ranges::any_of(text, [](unsigned char c) { return c < 0x20 || c == 0x7f; }))
    return std::nullopt;
  for (const auto &part : relative) {
    const auto name = part.generic_u8string();
    if (part.empty() || part == "." || part == ".." || name.back() == u8'.' || name.back() == u8' ')
      return std::nullopt;
  }
  if (!ReservedPathAllowed(relative))
    return std::nullopt;
  std::error_code error;
  const auto absolute = std::filesystem::weakly_canonical(root_ / relative, error);
  if (error || absolute.extension() != ".scene" || !Inside(root_, absolute) ||
      !ReservedPathAllowed(absolute.lexically_relative(root_)))
    return std::nullopt;
  return absolute;
}
bool SceneFileSession::BindCurrent(std::filesystem::path relative, bool save_blocked) {
  const auto path = Resolve(relative);
  if (!Live(Token()) || !path)
    return false;
  current_ = path->lexically_relative(root_);
  save_blocked_ = save_blocked;
  content_asset_.reset();
  content_blocked_ = false;
  content_relocated_ = false;
  return true;
}
SceneFileResult SceneFileSession::New(SceneFileToken token, bool discard_unsaved) {
  if (!Live(token) || !workspace_.Writable())
    return Rejected("New scene is unavailable for this document or read-only project.");
  if (document_.Dirty() && !discard_unsaved)
    return {SceneFileStatus::NeedsUnsavedChoice, "Save or discard the current scene first."};
  if (!document_.NewScene())
    return Rejected("The current scene could not be replaced.");
  generation_ = document_.Generation();
  current_.reset();
  save_blocked_ = false;
  content_asset_.reset();
  content_blocked_ = false;
  content_relocated_ = false;
  return {SceneFileStatus::Applied, "New unsaved scene."};
}
SceneFileResult SceneFileSession::Open(SceneFileToken token, const std::filesystem::path &relative,
                                       bool discard_unsaved) {
  if (!Live(token))
    return Rejected("The scene or project has changed. Choose the scene again.");
  const auto path = Resolve(relative);
  if (!path)
    return Rejected("Choose a relative .scene file inside this project.");
  if (document_.Dirty() && !discard_unsaved)
    return {SceneFileStatus::NeedsUnsavedChoice, "Save or discard the current scene first."};
  if (!document_.Reload(*path))
    return Rejected("Scene could not be opened. The current scene is unchanged.");
  generation_ = document_.Generation();
  current_ = path->lexically_relative(root_);
  save_blocked_ = false;
  content_asset_.reset();
  content_blocked_ = false;
  content_relocated_ = false;
  return {SceneFileStatus::Applied, "Scene opened."};
}
SceneFileResult SceneFileSession::Save(SceneFileToken token) {
  if (!Live(token) || !workspace_.Writable())
    return Rejected("Save is unavailable for this document or read-only project.");
  if (SaveBlocked())
    return Rejected("The current scene file is unavailable. Use New, Open or Save As.");
  if (!current_)
    return {SceneFileStatus::NeedsPath, "Choose a scene filename with Save As."};
  return SaveAs(token, *current_);
}
SceneFileResult SceneFileSession::SaveAs(SceneFileToken token,
                                         const std::filesystem::path &relative,
                                         bool replace_existing) {
  if (!Live(token) || !workspace_.Writable())
    return Rejected("Save As is unavailable for this document or read-only project.");
  const auto path = Resolve(relative);
  if (!path)
    return Rejected("Choose a relative .scene filename inside this project.");
  std::error_code error;
  const bool exists = std::filesystem::exists(*path, error);
  if (error || (exists && !std::filesystem::is_regular_file(*path, error)) || error)
    return Rejected("The scene destination is not an accessible file.");
  if (exists && !replace_existing &&
      (SaveBlocked() || !current_ || *current_ != path->lexically_relative(root_)))
    return {SceneFileStatus::NeedsOverwrite, "The destination exists. Confirm replacement first."};
  const bool reset_content =
      SaveBlocked() || !current_ || *current_ != path->lexically_relative(root_);
  if (!document_.Save(*path))
    return Rejected("Scene could not be saved. Check the project directory.");
  current_ = path->lexically_relative(root_);
  save_blocked_ = false;
  if (reset_content) {
    content_asset_.reset();
    content_relocated_ = false;
  }
  content_blocked_ = false;
  return {SceneFileStatus::Applied, "Scene saved."};
}
SceneFileResult SceneFileSession::SynchronizeContent(SceneFileToken token,
                                                     const ProjectContentSession &content) {
  if (!Live(token) || content.Root() != root_ || !content.Browser().ProjectGeneration())
    return Rejected("The scene or Content project has changed.");
  if (save_blocked_)
    return Rejected("The scene could not be loaded; its destination remains protected.");
  if (!current_ || *current_->begin() != "Content")
    return {SceneFileStatus::Applied, {}};
  const auto &browser = content.Browser();
  const ContentItem *item = nullptr;
  if (content_asset_) {
    if (content_generation_ != browser.ProjectGeneration()) {
      content_blocked_ = true;
      return Rejected("The scene's Content session changed. Use Open or Save As.");
    }
    item = browser.Find(*content_asset_);
  } else {
    const auto found = std::ranges::find(browser.Items(), *current_, &ContentItem::path);
    if (found == browser.Items().end())
      return {SceneFileStatus::Applied, {}};
    item = &*found;
  }
  const auto path = item && item->type == ".scene" ? Resolve(item->path) : std::nullopt;
  std::error_code error;
  if (!path || path->lexically_relative(root_) != item->path ||
      !std::filesystem::is_regular_file(*path, error) || error) {
    content_blocked_ = true;
    return Rejected(
        "The tracked scene asset is missing or unsafe. Undo its Content change or use Save As.");
  }
  content_asset_ = item->id;
  content_generation_ = browser.ProjectGeneration();
  content_relocated_ |= *current_ != item->path;
  current_ = item->path;
  content_blocked_ = false;
  return {SceneFileStatus::Applied, {}};
}
std::optional<std::filesystem::path> SceneFileSession::StartupMetadataPath() const {
  std::error_code error;
  const auto directory = root_ / ".nexora";
  // Project-owned metadata must not follow a substituted directory or file alias.
  if (std::filesystem::canonical(directory, error) != directory || error)
    return std::nullopt;
  const auto path = directory / "scene-session.ini";
  const auto status = std::filesystem::symlink_status(path, error);
  if (error == std::errc::no_such_file_or_directory ||
      (!error && status.type() == std::filesystem::file_type::not_found))
    return path;
  if (error || !std::filesystem::is_regular_file(status))
    return std::nullopt;
  return path;
}
SceneFileResult
SceneFileSession::ReadStartup(std::optional<std::filesystem::path> &relative) const {
  relative.reset();
  const auto path = StartupMetadataPath();
  if (!path)
    return Rejected("Scene startup settings are unavailable or unsafe; preserved for inspection.");
  std::error_code error;
  if (!std::filesystem::exists(*path, error) && !error)
    return {SceneFileStatus::NeedsPath, {}};
  std::ifstream input(*path, std::ios::binary);
  std::array<char, 1100> buffer{};
  input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  const auto count = static_cast<std::size_t>(input.gcount());
  const std::string_view text(buffer.data(), count);
  const auto prefix = "schema=1\nproject=" + project_.ToString() + "\nscene=";
  if (!input.eof() || input.bad() || count == buffer.size() || !text.starts_with(prefix) ||
      !text.ends_with('\n'))
    return Rejected("Scene startup settings are invalid; preserved for inspection.");
  const auto name = text.substr(prefix.size(), text.size() - prefix.size() - 1);
  if (name.empty() || name.size() >= 1024 || !foundation::IsValidUtf8(name))
    return Rejected("Scene startup filename is invalid; settings preserved for inspection.");
  const std::filesystem::path requested(std::u8string(name.begin(), name.end()));
  const auto resolved = Resolve(requested);
  if (!resolved)
    return Rejected("Scene startup path is outside the managed scope; settings preserved.");
  relative = resolved->lexically_relative(root_);
  return {SceneFileStatus::Applied, {}};
}
SceneFileResult SceneFileSession::RestoreStartup(SceneFileToken token) {
  if (!Live(token))
    return Rejected("The scene or project changed before startup restoration.");
  std::optional<std::filesystem::path> relative;
  auto result = ReadStartup(relative);
  startup_checked_ = true;
  startup_blocked_ = result.status == SceneFileStatus::Rejected;
  if (!relative)
    return result;
  result = Open(token, *relative);
  if (result.status == SceneFileStatus::Rejected) {
    startup_blocked_ = true;
    result.message = "Startup scene could not be loaded; settings preserved. " + result.message;
  }
  return result;
}
SceneFileResult SceneFileSession::RememberCurrent(SceneFileToken token) {
  if (!Live(token) || !workspace_.Writable() || workspace_.HasRecoveryJournal() ||
      !startup_checked_ || startup_blocked_ || SaveBlocked() || !current_ ||
      (document_.Dirty() && !content_relocated_))
    return Rejected("Scene startup selection could not be remembered; prior settings preserved.");
  std::optional<std::filesystem::path> previous;
  const auto checked = ReadStartup(previous);
  if (checked.status == SceneFileStatus::Rejected) {
    startup_blocked_ = true;
    return checked;
  }
  const auto scene_path = Resolve(*current_);
  const auto metadata = StartupMetadataPath();
  std::error_code error;
  if (!scene_path || !metadata || !std::filesystem::is_regular_file(*scene_path, error) || error)
    return Rejected("Current scene is unavailable; prior startup selection preserved.");
  const auto relative = scene_path->lexically_relative(root_);
  const auto text =
      "schema=1\nproject=" + project_.ToString() + "\nscene=" + detail::PathUtf8(relative) + "\n";
  std::string message;
  if (!detail::AtomicWrite(*metadata, text, &message))
    return Rejected("Scene selection was not remembered: " + message);
  content_relocated_ = false;
  return {SceneFileStatus::Applied, "Scene startup selection remembered."};
}
} // namespace nexora::editor
