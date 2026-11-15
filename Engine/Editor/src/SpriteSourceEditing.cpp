#include "Nexora/Editor/SpriteSourceEditing.h"
#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/TwoD/Scene.h"
#include "ReimportSource.h"

#include <fstream>
#include <random>
#include <sstream>
#include <type_traits>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdio>
#endif

namespace nexora::editor {
namespace {
using Error = SpriteSourceEditingError;
using State = SpriteSourceEditingState;
bool Directory(const std::filesystem::path &path) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  return !ec && std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status);
}
bool SourcePath(const std::filesystem::path &root, const std::filesystem::path &path) {
  if (path.empty() || path.has_root_path() || *path.begin() != "Content")
    return false;
  auto current = root;
  for (const auto &part : path.parent_path()) {
    if (part.empty() || part == "." || part == "..")
      return false;
    current /= part;
    if (!Directory(current))
      return false;
    std::error_code ec;
    if (std::filesystem::canonical(current, ec) != current || ec)
      return false;
  }
  const auto filename = path.filename();
  const auto encoded = path.generic_u8string();
  auto extension = filename.extension().string();
  for (auto &c : extension)
    if (c >= 'A' && c <= 'Z')
      c = char(c + ('a' - 'A'));
  return !filename.empty() && filename != "." && filename != ".." && encoded.size() <= 1024 &&
         extension == ".nx2sprite";
}
bool ExactNames(const std::filesystem::path &path, const std::filesystem::path &sidecar) {
  const auto text = [](const std::filesystem::path &value) {
    const auto bytes = value.filename().generic_u8string();
    return std::string(bytes.begin(), bytes.end());
  };
  const auto fold = [](std::string value) {
    for (auto &c : value)
      if (c >= 'A' && c <= 'Z')
        c = char(c + ('a' - 'A'));
    return value;
  };
  const auto source = text(path), identity = text(sidecar);
  const auto source_folded = fold(source), identity_folded = fold(identity);
  bool found_source{}, found_identity{};
  std::size_t count{};
  for (const auto &entry : std::filesystem::directory_iterator(path.parent_path())) {
    if (++count > 65536)
      return false;
    const auto name = text(entry.path()), folded = fold(name);
    if (folded == source_folded) {
      if (name != source)
        return false;
      found_source = true;
    }
    if (folded == identity_folded) {
      if (name != identity)
        return false;
      found_identity = true;
    }
  }
  return found_source && found_identity;
}
std::optional<std::string> Read(const std::filesystem::path &path, std::uintmax_t maximum) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec || !std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status))
    return {};
  const auto links = std::filesystem::hard_link_count(path, ec);
  if (ec || links != 1)
    return {};
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || size > maximum)
    return {};
  std::string bytes(std::size_t(size), '\0');
  std::ifstream input(path, std::ios::binary);
  if (!input || !input.read(bytes.data(), std::streamsize(size)) ||
      input.peek() != std::char_traits<char>::eof() || input.bad())
    return {};
  return bytes;
}
bool Identity(std::string_view bytes, runtime::AssetUuid expected) {
  std::istringstream input{std::string(bytes)};
  std::string schema, uuid, type, extra;
  if (!std::getline(input, schema) || !std::getline(input, uuid) || !std::getline(input, type) ||
      std::getline(input, extra))
    return false;
  for (auto *line : {&schema, &uuid, &type})
    if (!line->empty() && line->back() == '\r')
      line->pop_back();
  if (!type.starts_with("type="))
    return false;
  auto type_value = type.substr(5);
  for (auto &c : type_value)
    if (c >= 'A' && c <= 'Z')
      c = char(c + ('a' - 'A'));
  const auto parsed = uuid.starts_with("uuid=")
                          ? runtime::AssetUuid::Parse(std::string_view(uuid).substr(5))
                          : std::nullopt;
  return schema == "schema=1" && parsed && *parsed == expected && type_value == ".nx2sprite";
}
struct Temporary final {
  std::filesystem::path directory, file;
  bool owned{};
  ~Temporary() {
    if (owned) {
      std::error_code ec;
      std::filesystem::remove(file, ec);
      std::filesystem::remove(directory, ec);
    }
  }
};
} // namespace

std::string_view SpriteSourceEditingMessage(Error error) noexcept {
  switch (error) {
  case Error::None:
    return "Sprite source saved and imported.";
  case Error::Denied:
    return "Source editing requires a writable, idle project and stopped Play.";
  case Error::Stale:
    return "Project, Content or source bytes changed. Reopen source editing.";
  case Error::Path:
    return "The imported source must be an ordinary .nx2sprite in Content.";
  case Error::Identity:
    return "The original source and matching identity sidecar are required.";
  case Error::Atlas:
    return "The imported Atlas or selected region is unavailable.";
  case Error::Geometry:
    return "Source dimensions, trim, Pivot, borders or PPU are invalid.";
  case Error::Unchanged:
    return "No source changes to save.";
  case Error::Content:
    return "Content cannot publish the edited source and its dependencies.";
  case Error::Storage:
    return "The edit could not be staged. The original source is preserved.";
  case Error::Publication:
    return "Source replacement is uncertain. Inspect or reimport before retrying.";
  }
  return "Sprite source editing failed.";
}

SpriteSourceEditingResult SaveSpriteSourceFile(const ProjectWorkspace &workspace,
                                               ProjectContentSession &content,
                                               const SpriteSourceEditingRequest &request,
                                               bool authorized) noexcept {
  const auto fail = [](Error error) { return SpriteSourceEditingResult{State::Rejected, error}; };
  try {
    const auto permitted = [&] {
      return authorized && workspace.Writable() && content.Writable() && !content.ReimportBusy() &&
             !workspace.HasRecoveryJournal() && !workspace.HasExternalChange();
    };
    if (!permitted())
      return fail(Error::Denied);
    const auto current = [&] {
      return request.project == workspace.Project().id && request.root == workspace.Root() &&
             content.Root() == request.root && request.version.project_generation &&
             request.version == SpriteSourceContentVersion{content.Browser().ProjectGeneration(),
                                                           content.Browser().Revision()};
    };
    if (!current())
      return fail(Error::Stale);
    if (!SourcePath(request.root, request.path))
      return fail(Error::Path);
    const auto *item = content.Browser().Find(request.baseline.id);
    const auto original = item && item->twod ? item->twod->SpriteSource() : nullptr;
    if (!original || item->type != ".nx2sprite" || item->thumbnail != ThumbnailState::Ready ||
        item->id != original->id || item->path != request.path || *original != request.baseline ||
        item->artifact_hash != request.artifact)
      return fail(Error::Stale);
    const auto &source = request.source;
    if (source.id != original->id || source.atlas != original->atlas)
      return fail(Error::Identity);
    const auto *atlas_item = content.Browser().Find(source.atlas);
    const auto atlas = atlas_item && atlas_item->twod ? atlas_item->twod->Atlas() : nullptr;
    if (!atlas || atlas_item->id != atlas->id || atlas_item->type != ".nx2atlas" ||
        atlas_item->thumbnail != ThumbnailState::Ready)
      return fail(Error::Atlas);
    twod::SpriteInstance witness;
    witness.id = 1;
    witness.matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    if (!twod::ResolveSpriteAsset(source, *atlas, std::move(witness)))
      return fail(Error::Geometry);
    const auto before = twod::EncodeSpriteAsset(*original);
    const auto encoded = twod::EncodeSpriteAsset(source);
    if (before.size() != 96 || encoded.size() != 96)
      return fail(Error::Geometry);
    if (before == encoded)
      return fail(Error::Unchanged);
    const auto destination = request.root / request.path;
    const auto sidecar = AssetWorkspace::IdentitySidecar(destination);
    if (!ExactNames(destination, sidecar))
      return fail(Error::Path);
    const std::string baseline(reinterpret_cast<const char *>(before.data()), before.size());
    const auto identity = Read(sidecar, 4096);
    if (!identity || !Identity(*identity, source.id))
      return fail(Error::Identity);
    const auto disk = Read(destination, 96);
    if (!disk || *disk != baseline)
      return fail(Error::Stale);
    const auto imported = ImportTwoDAsset(".nx2sprite", encoded, source.id);
    if (!imported.payload)
      return fail(Error::Geometry);
    const std::string_view bytes(reinterpret_cast<const char *>(encoded.data()), encoded.size());
    const auto source_hash =
        detail::SourceHashHex(detail::HashSourceChunk(bytes, 1469598103934665603ULL));
    const auto artifact = detail::SourceHashHex(detail::HashSourceChunk(
        bytes, detail::HashSourceChunk(source.id.ToString(), 1469598103934665603ULL)));
    auto browser = content.Browser();
    auto dependencies = content.Dependencies();
    ReimportTransaction transaction(browser.ProjectGeneration(), source.id, item->artifact_hash);
    if (!browser.PublishArtifact(source.id, artifact, ThumbnailState::Ready, nullptr, {}, {},
                                 imported.payload) ||
        !transaction.Stage({browser.ProjectGeneration(),
                            source.id,
                            source_hash,
                            "default-v1",
                            artifact,
                            imported.payload->Dependencies(),
                            {},
                            false}) ||
        !transaction.Commit(browser.ProjectGeneration(), dependencies))
      return fail(Error::Content);
    Temporary temporary;
    std::random_device random;
    for (int attempt = 0; attempt < 8 && !temporary.owned; ++attempt) {
      temporary.directory =
          destination.parent_path() /
          (".nexora-sprite-edit-" + source.id.ToString() + "-" + std::to_string(random()));
      temporary.file = temporary.directory / "source.meta";
      std::error_code ec;
      temporary.owned = std::filesystem::create_directory(temporary.directory, ec);
      if (ec)
        return fail(Error::Storage);
    }
    if (!temporary.owned)
      return fail(Error::Storage);
    {
      std::ofstream output(temporary.file, std::ios::binary);
      output.write(bytes.data(), std::streamsize(bytes.size()));
      output.close();
      if (output.fail())
        return fail(Error::Storage);
    }
    if (!permitted())
      return fail(Error::Denied);
    if (!current() || !SourcePath(request.root, request.path) ||
        !ExactNames(destination, sidecar) || Read(destination, 96) != disk ||
        Read(sidecar, 4096) != identity)
      return fail(Error::Stale);
    static_assert(std::is_nothrow_move_assignable_v<ContentBrowserModel>);
    static_assert(std::is_nothrow_move_assignable_v<AssetDependencyGraph>);
#if defined(_WIN32)
    const bool replaced = MoveFileExW(temporary.file.c_str(), destination.c_str(),
                                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#else
    const bool replaced = std::rename(temporary.file.c_str(), destination.c_str()) == 0;
#endif
    if (!replaced)
      return {State::Uncertain, Error::Publication};
    content.Browser() = std::move(browser);
    content.Dependencies() = std::move(dependencies);
    return {State::Saved, Error::None};
  } catch (...) {
    return fail(Error::Storage);
  }
}
} // namespace nexora::editor
