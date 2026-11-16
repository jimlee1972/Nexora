#include "Nexora/Editor/TwoDAssetEditing.h"
#include "Nexora/Editor/ProjectContent.h"
#include "ReimportSource.h"

#include <fstream>
#include <random>
#include <sstream>
#include <type_traits>
#include <unordered_set>
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
using Error = TwoDAssetEditingError;
using State = TwoDAssetEditingState;
bool Directory(const std::filesystem::path &path) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  return !ec && std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status);
}
bool SourcePath(const std::filesystem::path &root, const std::filesystem::path &path,
                std::string_view type) {
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
         extension == type && (type == ".nx2atlas" || type == ".nx2clip");
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
bool Identity(std::string_view bytes, runtime::AssetUuid expected, std::string_view expected_type) {
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
  return schema == "schema=1" && parsed && *parsed == expected && type_value == expected_type;
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

std::string_view TwoDAssetEditingMessage(Error error) noexcept {
  switch (error) {
  case Error::None:
    return "Asset saved and imported. UUID and Scene history are preserved.";
  case Error::Denied:
    return "Asset editing requires a writable, idle project and stopped Play.";
  case Error::Stale:
    return "Project, Content or source bytes changed. Reopen asset editing.";
  case Error::Path:
    return "Edit an ordinary imported .nx2atlas or .nx2clip in Content.";
  case Error::Identity:
    return "The original asset and matching identity sidecar are required.";
  case Error::Closure:
    return "An Atlas region or Sprite source dependency is missing or incompatible.";
  case Error::Data:
    return "Asset data exceeds its bounds or violates Atlas/Clip format rules.";
  case Error::Unchanged:
    return "No asset changes to save.";
  case Error::Content:
    return "Content cannot publish the edited asset and dependencies.";
  case Error::Storage:
    return "The edit could not be staged. The original asset is preserved.";
  case Error::Publication:
    return "Asset replacement is uncertain. Inspect or reimport before retrying.";
  }
  return "Asset editing failed.";
}

TwoDAssetEditingResult SaveTwoDAssetFile(const ProjectWorkspace &workspace,
                                         ProjectContentSession &content,
                                         const TwoDAssetEditingRequest &request,
                                         bool authorized) noexcept {
  const auto fail = [](Error error) { return TwoDAssetEditingResult{State::Rejected, error}; };
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
    if (!SourcePath(request.root, request.path, request.type))
      return fail(Error::Path);
    if (request.baseline.size() > kMaximumTwoDSourceBytes ||
        request.proposed.size() > kMaximumTwoDSourceBytes)
      return fail(Error::Data);
    const auto *item = content.Browser().Find(request.id);
    if (!item || !item->twod || item->type != request.type || item->twod->Id() != request.id ||
        item->twod->Type() != request.type || item->thumbnail != ThumbnailState::Ready ||
        item->path != request.path || item->artifact_hash != request.artifact)
      return fail(Error::Stale);
    const auto before = item->twod->Atlas()  ? twod::EncodeAtlas(*item->twod->Atlas())
                        : item->twod->Clip() ? twod::EncodeClip(*item->twod->Clip())
                                             : twod::Bytes{};
    if (before.empty() || before != request.baseline)
      return fail(Error::Stale);
    const auto &encoded = request.proposed;
    const auto imported = ImportTwoDAsset(request.type, encoded, request.id);
    if (!imported.payload)
      return fail(Error::Data);
    if (before == encoded)
      return fail(Error::Unchanged);
    // Bound both retained payloads and dependency validation before staging a replacement.
    std::size_t count{}, retained{};
    std::unordered_map<runtime::AssetUuid, std::shared_ptr<const TwoDAssetPayload>,
                       runtime::AssetUuidHash>
        assets;
    std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> ready;
    for (const auto &entry : content.Browser().Items()) {
      if (!entry.twod)
        continue;
      const auto owner = entry.id == request.id ? imported.payload : entry.twod;
      if (++count > kMaximumWorkspaceTwoDAssets ||
          owner->RetainedBytes() > kMaximumWorkspaceTwoDBytes - retained ||
          owner->Id() != entry.id || owner->Type() != entry.type ||
          !assets.emplace(entry.id, owner).second)
        return fail(Error::Closure);
      retained += owner->RetainedBytes();
      if (entry.thumbnail == ThumbnailState::Ready)
        ready.insert(entry.id);
    }
    const auto atlasFor = [&](runtime::AssetUuid id) -> std::shared_ptr<const twod::Atlas> {
      const auto found = assets.find(id);
      return found == assets.end() || !ready.contains(id) ? nullptr : found->second->Atlas();
    };
    std::unordered_map<runtime::AssetUuid, bool, runtime::AssetUuidHash> sourceClosure;
    const auto sourceValid = [&](const twod::SpriteAsset &source) {
      const auto [checked, inserted] = sourceClosure.try_emplace(source.id, false);
      if (!inserted)
        return checked->second;
      if (!ready.contains(source.id))
        return false;
      const auto atlas = atlasFor(source.atlas);
      // ImportTwoDAsset already validated these exact immutable Atlas/Sprite payloads,
      // including region overlaps and source geometry bounds. Check only their closure,
      // once per source; repeated Clip frames must not repeat full Atlas validation.
      const auto *region = atlas ? atlas->Find(source.region) : nullptr;
      checked->second =
          region && region->width == source.trimWidth && region->height == source.trimHeight;
      return checked->second;
    };
    // Only the changed asset and its direct dependents need closure validation. Unrelated
    // broken assets remain inspectable and do not prevent repairing this imported asset.
    for (const auto &[assetId, owner] : assets) {
      if (const auto source = owner->SpriteSource(); source && source->atlas == request.id)
        if (!sourceValid(*source))
          return fail(Error::Closure);
      const auto clip = owner->Clip();
      if (!clip || (assetId != request.id && clip->atlas != request.id))
        continue;
      if (twod::ClipUsesSources(*clip)) {
        for (const auto &frame : clip->frames) {
          const auto found = assets.find(*frame.source);
          const auto source = found == assets.end() ? nullptr : found->second->SpriteSource();
          if (!source || !sourceValid(*source))
            return fail(Error::Closure);
        }
      } else {
        const auto atlas = atlasFor(clip->atlas);
        if (!atlas)
          return fail(Error::Closure);
        for (const auto &frame : clip->frames)
          if (!atlas->Find(frame.region))
            return fail(Error::Closure);
      }
    }
    const auto destination = request.root / request.path;
    const auto sidecar = AssetWorkspace::IdentitySidecar(destination);
    if (!ExactNames(destination, sidecar))
      return fail(Error::Path);
    const std::string baseline(reinterpret_cast<const char *>(before.data()), before.size());
    const auto identity = Read(sidecar, 4096);
    if (!identity || !Identity(*identity, request.id, request.type))
      return fail(Error::Identity);
    const auto disk = Read(destination, kMaximumTwoDSourceBytes);
    if (!disk || *disk != baseline)
      return fail(Error::Stale);
    const std::string_view bytes(reinterpret_cast<const char *>(encoded.data()), encoded.size());
    const auto source_hash =
        detail::SourceHashHex(detail::HashSourceChunk(bytes, 1469598103934665603ULL));
    const auto artifact = detail::SourceHashHex(detail::HashSourceChunk(
        bytes, detail::HashSourceChunk(request.id.ToString(), 1469598103934665603ULL)));
    auto browser = content.Browser();
    auto dependencies = content.Dependencies();
    ReimportTransaction transaction(browser.ProjectGeneration(), request.id, item->artifact_hash);
    if (!browser.PublishArtifact(request.id, artifact, ThumbnailState::Ready, nullptr, {}, {},
                                 imported.payload) ||
        !transaction.Stage({browser.ProjectGeneration(),
                            request.id,
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
          (".nexora-asset-edit-" + request.id.ToString() + "-" + std::to_string(random()));
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
    if (!current() || !SourcePath(request.root, request.path, request.type) ||
        !ExactNames(destination, sidecar) || Read(destination, kMaximumTwoDSourceBytes) != disk ||
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
