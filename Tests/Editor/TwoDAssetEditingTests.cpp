#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/Editor/TwoDAssetEditing.h"
#include "TemporaryDirectoryCleanup.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace nexora;
namespace {
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::ofstream stream(path, std::ios::binary);
  stream.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  Check(bool(stream), "write");
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), {}};
}
auto Files(const std::filesystem::path &root) {
  std::map<std::filesystem::path, std::string> result;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file())
      result.emplace(entry.path().lexically_relative(root), Read(entry.path()));
  return result;
}
void Run() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-asset-edit-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup(root);
  editor::ProjectWorkspace workspace;
  Check(workspace.Create(root, "Atlas Clip authoring"), "workspace");
  constexpr runtime::AssetUuid atlasId{101, 1}, sourceId{202, 2}, clipId{303, 3}, legacyId{404, 4};
  twod::Atlas atlas{atlasId,
                    16,
                    16,
                    1,
                    false,
                    {{7, 3, 4, 7, 9}, {8, 12, 1, 1, 1}},
                    twod::Bytes(16 * 16 * 4, std::byte{255})};
  twod::SpriteAsset source{sourceId, atlasId, 7, 19, 23, 3, 5, 7, 9, 4.5F, 8.25F, 2, {}};
  twod::Clip clip{clipId, {}, true, {{0, .1, "first", sourceId}, {0, .2, "last", sourceId}}};
  twod::Clip legacy{legacyId, atlasId, true, {{8, .1, ""}}};
  Write(root / "Content/Atlas.nx2atlas", twod::EncodeAtlas(atlas));
  Write(root / "Content/Source.nx2sprite", twod::EncodeSpriteAsset(source));
  Write(root / "Content/Clip.nx2clip", twod::EncodeClip(clip));
  Write(root / "Content/Legacy.nx2clip", twod::EncodeClip(legacy));
  editor::AssetWorkspace assets;
  Check(assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
        "index");
  editor::ProjectContentSession content;
  Check(content.Open(workspace, assets, 5), "Content");
  Check(content.Rename(sourceId, "Renamed.nx2sprite") && content.CanUndo(), "file Undo");
  const auto requestFor = [&](runtime::AssetUuid id) {
    const auto *item = content.Browser().Find(id);
    const auto bytes = item->twod->Atlas() ? twod::EncodeAtlas(*item->twod->Atlas())
                                           : twod::EncodeClip(*item->twod->Clip());
    return editor::TwoDAssetEditingRequest{
        workspace.Project().id,
        workspace.Root(),
        item->path,
        {content.Browser().ProjectGeneration(), content.Browser().Revision()},
        id,
        item->type,
        item->artifact_hash,
        bytes,
        bytes};
  };
  using State = editor::TwoDAssetEditingState;
  using Error = editor::TwoDAssetEditingError;
  const auto reject = [&](const auto &request, bool allowed = true) {
    const auto before = Files(root);
    const auto revision = content.Browser().Revision();
    const auto undo = content.CanUndo();
    const auto owner = content.Browser().Find(request.id)->twod;
    const auto result = editor::SaveTwoDAssetFile(workspace, content, request, allowed);
    Check(result.state == State::Rejected && result.error != Error::None, "bad edit admitted");
    Check(before == Files(root) && content.Browser().Revision() == revision &&
              content.Browser().Find(request.id)->twod == owner && content.CanUndo() == undo,
          "rejection changed state");
    return result.error;
  };
  auto request = requestFor(atlasId);
  auto candidate = atlas;
  candidate.regions.front().width = 6;
  request.proposed = twod::EncodeAtlas(candidate);
  Check(reject(request) == Error::Closure, "source trim mismatch");
  candidate = atlas;
  candidate.regions.pop_back();
  request.proposed = twod::EncodeAtlas(candidate);
  Check(reject(request) == Error::Closure, "legacy Clip region removed");
  candidate = atlas;
  candidate.regions[1] = {8, 13, 2, 1, 1};
  request.proposed = twod::EncodeAtlas(candidate);
  Check(reject(request, false) == Error::Denied, "authority");
  auto stale = request;
  ++stale.version.content_revision;
  Check(reject(stale) == Error::Stale, "revision");
  stale = request;
  stale.path = "Content/../Atlas.nx2atlas";
  Check(reject(stale) == Error::Path, "path");
  stale = request;
  stale.baseline.back() ^= std::byte{1};
  Check(reject(stale) == Error::Stale, "baseline");
  const auto atlasSidecar = editor::AssetWorkspace::IdentitySidecar(root / request.path);
  const auto identity = Read(atlasSidecar);
  const auto sourceBytes = Read(root / "Content/Renamed.nx2sprite");
  const auto retainedAtlas = content.Browser().Find(atlasId)->twod;
  // The edited rectangle can move without replacing pixels, identity or dependent source geometry.
  Check(editor::SaveTwoDAssetFile(workspace, content, request, true).state == State::Saved,
        "save Atlas");
  Check(Read(atlasSidecar) == identity && content.Browser().Find(atlasId)->twod != retainedAtlas &&
            retainedAtlas->Atlas()->Find(8)->x == 12 &&
            content.Browser().Find(atlasId)->twod->Atlas()->Find(8)->x == 13 &&
            Read(root / "Content/Renamed.nx2sprite") == sourceBytes && content.CanUndo(),
        "Atlas ownership/identity/history");
  // Failed/loading imports retain immutable last-good owners for preview, not new closure.
  for (const auto &[dependencyId, state] :
       std::array{std::pair{atlasId, editor::ThumbnailState::Failed},
                  std::pair{sourceId, editor::ThumbnailState::Failed},
                  std::pair{atlasId, editor::ThumbnailState::Loading}}) {
    auto savedBrowser = content.Browser();
    const auto *dependency = content.Browser().Find(dependencyId);
    const auto lastGood = dependency->twod;
    Check(content.Browser().PublishArtifact(dependencyId, dependency->artifact_hash, state),
          "retained dependency import state");
    Check(content.Browser().Find(dependencyId)->twod == lastGood, "last-good fixture lost owner");
    auto invalidClosure = requestFor(clipId);
    auto changedClip = clip;
    changedClip.frames.front().event = "new";
    invalidClosure.proposed = twod::EncodeClip(changedClip);
    Check(reject(invalidClosure) == Error::Closure, "failed/loading last-good dependency admitted");
    content.Browser() = std::move(savedBrowser);
  }
  // An unrelated failed Clip must not block a valid source Clip edit.
  const auto *unrelated = content.Browser().Find(legacyId);
  Check(content.Browser().PublishArtifact(legacyId, unrelated->artifact_hash,
                                          editor::ThumbnailState::Failed),
        "unrelated failed Clip");
  request = requestFor(clipId);
  auto editedClip = clip;
  editedClip.loop = false;
  editedClip.frames[0].seconds = .375;
  editedClip.frames[0].event = "hit";
  std::swap(editedClip.frames[0], editedClip.frames[1]);
  request.proposed = twod::EncodeClip(editedClip);
  const auto clipIdentityPath = editor::AssetWorkspace::IdentitySidecar(root / request.path);
  const auto clipIdentity = Read(clipIdentityPath);
  auto missing = editedClip;
  missing.frames.front().source = runtime::AssetUuid{999, 999};
  auto bad = request;
  bad.proposed = twod::EncodeClip(missing);
  Check(reject(bad) == Error::Closure, "missing source");
  const auto originalBytes = twod::EncodeClip(clip);
  auto external = clip;
  external.frames[0].event = "external";
  Write(root / request.path, twod::EncodeClip(external));
  Check(reject(request) == Error::Stale, "external source overwrite");
  Write(root / request.path, originalBytes);
  Check(editor::SaveTwoDAssetFile(workspace, content, request, true).state == State::Saved,
        "save Clip");
  Check(Read(clipIdentityPath) == clipIdentity &&
            content.Browser().Find(clipId)->twod->Clip()->frames[1].seconds == .375 &&
            !content.Browser().Find(clipId)->twod->Clip()->loop && content.CanUndo(),
        "Clip identity/time/loop");
  Check(content.Undo() && Read(root / "Content/Source.nx2sprite") == sourceBytes,
        "Content Undo survived saves");
  auto noChange = requestFor(clipId);
  Check(reject(noChange) == Error::Unchanged, "no change");
  // Reopen from bytes rather than relying on the published imported owner.
  const auto persisted = Files(root);
  editor::AssetWorkspace reopenedAssets;
  Check(reopenedAssets.ImportTree(root / "Content", {}, {},
                                  editor::AssetIdentityMode::PersistentReadOnly),
        "read-only index");
  editor::ProjectContentSession reopened;
  Check(reopened.Open(workspace, reopenedAssets, 6, false), "reopen");
  Check(reopened.Browser().Find(atlasId)->twod->Atlas()->Find(8)->x == 13 &&
            reopened.Browser().Find(clipId)->twod->Clip()->frames[1].event == "hit" &&
            Files(root) == persisted,
        "persistence");
  editor::ProjectWorkspace reader;
  Check(reader.Open(root, editor::ProjectAccess::ReadOnly), "reader");
  const auto denied = editor::SaveTwoDAssetFile(reader, reopened, noChange, true);
  Check(denied.error == Error::Denied && Files(root) == persisted, "read-only save");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Atlas/Clip asset editing passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
