#include "Nexora/Editor/ProjectTwoDPlayAssets.h"
#include "Nexora/Editor/TwoDAssetImport.h"
#include "ReimportSource.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace nexora::editor {
namespace {
bool Fail(std::string *error, const char *message) {
  if (error)
    *error = message;
  return false;
}
bool Allowed(const ProjectWorkspace &workspace, const ProjectContentSession &content,
             bool authorized) {
  const auto project = authorized && !workspace.Root().empty()
                           ? ProjectWorkspace::PreviewUpgrade(workspace.Root())
                           : std::nullopt;
  return project && project->state == ProjectUpgradeState::Current &&
         project->root == workspace.Root() && project->project.id == workspace.Project().id &&
         project->project.name == workspace.Project().name &&
         project->project.schema_version == workspace.Project().schema_version &&
         !workspace.Project().id.IsNil() && !workspace.Root().empty() &&
         workspace.Root() == content.Root() && workspace.Writable() && content.Writable() &&
         content.Browser().ProjectGeneration() && !workspace.HasRecoveryJournal() &&
         !workspace.HasExternalChange() && !content.ReimportBusy() &&
         std::ranges::none_of(content.Conflicts().Conflicts(), [](const auto &conflict) {
           return conflict.choice == DirtyConflictChoice::Pending;
         });
}
std::optional<runtime::ByteBuffer> Read(const std::filesystem::path &root,
                                        const std::filesystem::path &relative,
                                        std::size_t maximum) {
  const auto text = relative.generic_u8string();
  if (text.empty() ||
      text.size() > kMaximumIndexedPathBytes + std::string_view("Content/").size() + 5 ||
      text.find(u8'\0') != text.npos ||
      !foundation::IsValidUtf8(
          std::string_view(reinterpret_cast<const char *>(text.data()), text.size())) ||
      relative.has_root_path() || *relative.begin() != "Content")
    return {};
  std::error_code ec;
  if (std::filesystem::canonical(root, ec) != root || ec)
    return {};
  auto path = root;
  for (auto it = relative.begin(); it != relative.end(); ++it) {
    if (*it == "." || *it == "..")
      return {};
    path /= *it;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec || std::filesystem::is_symlink(status))
      return {};
    if (std::next(it) != relative.end()) {
      if (!std::filesystem::is_directory(status))
        return {};
    } else if (!std::filesystem::is_regular_file(status) ||
               std::filesystem::hard_link_count(path, ec) != 1 || ec ||
               std::filesystem::file_size(path, ec) > maximum || ec) {
      return {};
    }
  }
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return {};
  runtime::ByteBuffer bytes;
  std::array<char, 8192> chunk{};
  while (input) {
    input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    const auto count = static_cast<std::size_t>(input.gcount());
    if (count > maximum - bytes.size())
      return {};
    const auto part = std::as_bytes(std::span(chunk.data(), count));
    bytes.insert(bytes.end(), part.begin(), part.end());
  }
  return input.eof() && !input.bad() ? std::optional(std::move(bytes)) : std::nullopt;
}
bool Identity(std::span<const std::byte> bytes, runtime::AssetUuid id, std::string_view type) {
  if (bytes.empty())
    return false;
  std::istringstream input(std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
  std::string schema, uuid, importer, extra;
  if (!std::getline(input, schema) || !std::getline(input, uuid) ||
      !std::getline(input, importer) || std::getline(input, extra))
    return false;
  for (auto *line : {&schema, &uuid, &importer})
    if (line->ends_with('\r'))
      line->pop_back();
  if (!uuid.starts_with("uuid=") || !importer.starts_with("type="))
    return false;
  auto normalized = importer.substr(5);
  for (auto &character : normalized)
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  return schema == "schema=1" && runtime::AssetUuid::Parse(uuid.substr(5)) == id &&
         normalized == type;
}
runtime::ByteBuffer Encode(const TwoDAssetPayload &payload) {
  return payload.Atlas() ? twod::EncodeAtlas(*payload.Atlas()) : twod::EncodeClip(*payload.Clip());
}
bool Admitted(const ContentItem &item, std::span<const std::byte> bytes) {
  if (!item.twod || item.twod->Id() != item.id || item.twod->Type() != item.type ||
      item.thumbnail != ThumbnailState::Ready || item.artifact_hash.empty())
    return false;
  const auto encoded = Encode(*item.twod);
  const auto source = std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  const auto hash = detail::SourceHashHex(detail::HashSourceChunk(
      source, detail::HashSourceChunk(item.id.ToString(), 1469598103934665603ULL)));
  return std::ranges::equal(encoded, bytes) && hash == item.artifact_hash;
}
} // namespace

std::shared_ptr<const ProjectTwoDPlayAssets> ProjectTwoDPlayAssets::Capture(
    const ProjectWorkspace &workspace, const ProjectContentSession &content,
    std::uint64_t expected_project_generation, std::uint64_t cooked_generation,
    std::span<const runtime::AssetUuid> roots, bool authorized, std::string *error,
    TwoDPlayAssetLimits limits) {
  if (error)
    error->clear();
  const auto reject = [&](const char *message) -> std::shared_ptr<const ProjectTwoDPlayAssets> {
    Fail(error, message);
    return {};
  };
  if (!Allowed(workspace, content, authorized) || !expected_project_generation ||
      content.Browser().ProjectGeneration() != expected_project_generation || !cooked_generation ||
      roots.empty() || !limits.assets || limits.assets > 4096 || !limits.payload_bytes ||
      limits.payload_bytes > 64 * 1024 * 1024 || roots.size() > limits.assets)
    return reject(
        "2D project capture requires current writer scope, authorization and bounded roots.");
  auto result = std::shared_ptr<ProjectTwoDPlayAssets>(new ProjectTwoDPlayAssets);
  result->project_ = workspace.Project().id;
  result->root_ = workspace.Root();
  result->project_generation_ = expected_project_generation;
  result->content_revision_ = content.Browser().Revision();
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> seen;
  std::vector<runtime::AssetUuid> pending;
  for (auto id : roots) {
    if (id == runtime::AssetUuid{} || !seen.insert(id).second)
      return reject("2D project roots contain a zero or duplicate identity.");
    pending.push_back(id);
  }
  runtime::ImporterRegistry importers;
  if (!twod::RegisterImporters(importers))
    return reject("2D source importers could not initialize.");
  runtime::AssetCooker cooker;
  runtime::DerivedDataCache cache;
  std::vector<runtime::RuntimeBlob> blobs;
  std::size_t source_bytes{};
  for (std::size_t index = 0; index < pending.size(); ++index) {
    const auto *item = content.Browser().Find(pending[index]);
    if (!item || !item->twod)
      return reject("A selected 2D project source or dependency is unavailable.");
    auto bytes = Read(result->root_, item->path, limits.payload_bytes - source_bytes);
    auto metadata = Read(result->root_, AssetWorkspace::IdentitySidecar(item->path), 4096);
    if (!bytes || !metadata || !Identity(*metadata, item->id, item->type) ||
        !Admitted(*item, *bytes))
      return reject("A 2D source or sidecar is unsafe, stale, inconsistent or over budget.");
    auto dependencies = item->twod->Dependencies();
    if (content.Dependencies().Forward(item->id) != dependencies)
      return reject("The current 2D dependency graph disagrees with admitted source data.");
    for (auto dependency : dependencies)
      if (seen.insert(dependency).second) {
        if (pending.size() == limits.assets)
          return reject("The project 2D closure exceeds its asset budget.");
        pending.push_back(dependency);
      }
    // Registry dispatch uses a filename extension. The validated sidecar importer remains
    // authoritative after rename, so supply a logical importer key rather than the IO path.
    auto canonical =
        importers.Import({item->id, item->type, item->id.ToString() + item->type, *bytes});
    auto blob =
        canonical ? cooker.Cook(*canonical, "editor-play", "project-2d-v1", cache) : std::nullopt;
    if (!blob)
      return reject("A current 2D project source could not be cooked.");
    blobs.push_back(std::move(*blob));
    source_bytes += bytes->size();
    result->sources_.push_back({item->id, item->path, item->type, item->artifact_hash,
                                std::move(*bytes), std::move(*metadata), std::move(dependencies)});
  }
  auto bundle = runtime::BundleBuilder::Build("project-2d-play", cooked_generation, {}, blobs);
  runtime::AssetGenerationStore store;
  if (!bundle || !store.Stage({std::move(*bundle)}) || !store.ActivateStaged())
    return reject("The project 2D cooked generation could not be verified.");
  result->assets_ = TwoDPlayAssets::Capture(store, cooked_generation, roots, error, limits);
  if (!result->assets_ || !result->Matches(workspace, content, authorized, error))
    return {};
  return result;
}

bool ProjectTwoDPlayAssets::Matches(const ProjectWorkspace &workspace,
                                    const ProjectContentSession &content, bool authorized,
                                    std::string *error) const {
  if (error)
    error->clear();
  if (!Allowed(workspace, content, authorized) || workspace.Project().id != project_ ||
      workspace.Root() != root_ || content.Browser().ProjectGeneration() != project_generation_ ||
      content.Browser().Revision() != content_revision_)
    return Fail(error, "The 2D project capture scope or Content revision is stale.");
  for (const auto &source : sources_) {
    const auto *item = content.Browser().Find(source.id);
    if (!item || item->path != source.path || item->type != source.type ||
        item->artifact_hash != source.artifact ||
        content.Dependencies().Forward(source.id) != source.dependencies)
      return Fail(error, "A captured 2D Content identity, path, artifact or dependency changed.");
    const auto bytes = Read(root_, source.path, source.bytes.size());
    const auto metadata = Read(root_, AssetWorkspace::IdentitySidecar(source.path), 4096);
    if (!bytes || !metadata || *bytes != source.bytes || *metadata != source.metadata ||
        !Admitted(*item, *bytes))
      return Fail(error, "A captured 2D source or sidecar no longer matches its exact baseline.");
  }
  return (Allowed(workspace, content, authorized) && workspace.Project().id == project_ &&
          workspace.Root() == root_ &&
          content.Browser().ProjectGeneration() == project_generation_ &&
          content.Browser().Revision() == content_revision_) ||
         Fail(error, "The 2D project scope changed during source validation.");
}
} // namespace nexora::editor
