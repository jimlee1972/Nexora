#include "Nexora/Editor/SceneFiles.h"

#include <algorithm>
#include <system_error>

namespace nexora::editor {
namespace {
SceneFileResult Rejected(std::string message) {
  return {SceneFileStatus::Rejected, std::move(message)};
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
  for (const auto &part : relative)
    if (part.empty() || part == "." || part == "..")
      return std::nullopt;
  if (*relative.begin() == ".nexora") {
    auto part = relative.begin();
    if (++part == relative.end() || *part != "scenes")
      return std::nullopt;
  }
  std::error_code error;
  const auto absolute = std::filesystem::weakly_canonical(root_ / relative, error);
  if (error || !Inside(root_, absolute))
    return std::nullopt;
  return root_ / relative;
}
bool SceneFileSession::BindCurrent(std::filesystem::path relative, bool save_blocked) {
  if (!Live(Token()) || !Resolve(relative))
    return false;
  current_ = std::move(relative);
  save_blocked_ = save_blocked;
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
  current_ = relative;
  save_blocked_ = false;
  return {SceneFileStatus::Applied, "Scene opened."};
}
SceneFileResult SceneFileSession::Save(SceneFileToken token) {
  if (!Live(token) || !workspace_.Writable())
    return Rejected("Save is unavailable for this document or read-only project.");
  if (save_blocked_)
    return Rejected("The current scene file could not be loaded. Use New, Open or Save As.");
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
  if (exists && !replace_existing && (save_blocked_ || !current_ || *current_ != relative))
    return {SceneFileStatus::NeedsOverwrite, "The destination exists. Confirm replacement first."};
  if (!document_.Save(*path))
    return Rejected("Scene could not be saved. Check the project directory.");
  current_ = relative;
  save_blocked_ = false;
  return {SceneFileStatus::Applied, "Scene saved."};
}
} // namespace nexora::editor
