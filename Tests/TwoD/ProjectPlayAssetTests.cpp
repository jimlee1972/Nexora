#include "Nexora/Editor/ProjectTwoDPlayAssets.h"
#include "Nexora/Editor/TwoDAssetImport.h"
#include "Nexora/TwoD/GameplayBridge.h"
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace nexora;
namespace {
constexpr runtime::AssetUuid atlasId{17, 23}, clipId{19, 29}, otherId{31, 37};
constexpr std::uint64_t projectGeneration = 13;
const std::array roots{clipId};
void Check(bool value, const char *reason) {
  if (!value)
    throw std::runtime_error(reason);
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  Check(bool(out), "write fixture");
}
runtime::ByteBuffer Read(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  const std::string bytes{std::istreambuf_iterator<char>(in), {}};
  const auto span = std::as_bytes(std::span(bytes));
  return {span.begin(), span.end()};
}
twod::Bytes Atlas(std::byte color = std::byte{255}) {
  twod::Image image{1, 1, 1, {color, std::byte{0}, std::byte{0}, std::byte{255}}};
  const auto atlas = twod::PackAtlas(atlasId, std::span(&image, 1), 4);
  Check(bool(atlas), "pack fixture");
  return twod::EncodeAtlas(*atlas);
}
struct Fixture final {
  editor::ProjectWorkspace workspace;
  editor::AssetWorkspace index;
  editor::ProjectContentSession content;
  explicit Fixture(const std::filesystem::path &root) {
    Check(workspace.Create(root, "2D Play capture"), "create project");
    Write(root / "Content/Atlas.NX2ATLAS", Atlas());
    Write(root / "Content/Clip.nx2clip",
          twod::EncodeClip({clipId, atlasId, true, {{1, 0.1, "frozen"}}}));
    Refresh();
  }
  void Refresh() {
    Check(index.ImportTree(workspace.Root() / "Content", {}, {},
                           editor::AssetIdentityMode::PersistentReadWrite),
          "index fixture");
    Check(content.Open(workspace, index, projectGeneration), "open content");
  }
  std::shared_ptr<const editor::ProjectTwoDPlayAssets>
  Capture(editor::TwoDPlayAssetLimits limits = {}) const {
    return editor::ProjectTwoDPlayAssets::Capture(workspace, content, projectGeneration, 71, roots,
                                                  true, nullptr, limits);
  }
};
void Isolation(const std::filesystem::path &root) {
  std::shared_ptr<const editor::ProjectTwoDPlayAssets> captured;
  std::shared_ptr<const editor::TwoDPlayAssets> frozen;
  {
    Fixture fixture(root);
    const auto path = root / "Content/Atlas.NX2ATLAS";
    const auto sidecar = editor::AssetWorkspace::IdentitySidecar(path);
    const auto before = Read(path), meta = Read(sidecar);
    const auto revision = fixture.content.Browser().Revision();
    std::string error = "old";
    captured = editor::ProjectTwoDPlayAssets::Capture(fixture.workspace, fixture.content,
                                                      projectGeneration, 71, roots, true, &error);
    Check(captured && error.empty() && captured->Matches(fixture.workspace, fixture.content, true),
          "capture current project");
    frozen = captured->Assets();
    Check(frozen && frozen->Size() == 2 && frozen->Generation() == 71 &&
              captured->Project() == fixture.workspace.Project().id &&
              captured->Root() == fixture.workspace.Root() &&
              captured->ProjectGeneration() == projectGeneration &&
              captured->ContentRevision() == revision,
          "owning project/cooked provenance");
    Check(Read(path) == before && Read(sidecar) == meta &&
              fixture.content.Browser().Revision() == revision && !fixture.content.CanUndo(),
          "capture mutated source, identity or history");
    Check(fixture.content.Rename(atlasId, "Renamed.asset"), "rename authoritative source");
    Check(!captured->Matches(fixture.workspace, fixture.content, true), "rename remained current");
    auto renamed = fixture.Capture();
    Check(renamed && renamed->Assets()->Atlas(atlasId)->rgba == frozen->Atlas(atlasId)->rgba,
          "sidecar importer lost across extension rename");
    Check(fixture.content.Undo(), "undo rename");
    Write(path, Atlas(std::byte{7}));
    Check(!fixture.Capture() && !captured->Matches(fixture.workspace, fixture.content, true),
          "unimported source revision admitted");
    Check(fixture.content.Reimport(atlasId), "reimport new pixels");
    auto replacement = fixture.Capture();
    Check(replacement &&
              replacement->Assets()->Atlas(atlasId)->rgba != frozen->Atlas(atlasId)->rgba,
          "new snapshot did not use reimported payload");
    Check(!captured->Matches(fixture.workspace, fixture.content, true),
          "old Content revision revived");
  }
  Check(frozen->Atlas(atlasId)->Find(1) && frozen->Clip(clipId)->frames[0].event == "frozen",
        "snapshot borrowed destroyed project/cook store");
  runtime::World world(runtime::WorldKind::Play);
  const auto sceneId = world.LoadScene("Frozen project 2D");
  const auto entity = world.CreateEntity(sceneId).id;
  twod::SpriteScene scene(world, sceneId);
  twod::GameplayBridge bridge(world, scene, [frozen](auto id) { return frozen->Clip(id); });
  NexoraTwoDApiV1 api{};
  api.struct_size = sizeof(api);
  Check(bridge.GetApi(&api) == 0, "project frozen resolver");
  const NexoraTwoDSprite sprite{atlasId.high, atlasId.low,  1, 1, {1, 1},
                                {0.5F, 0.5F}, {1, 1, 1, 1}, 0, 0};
  Check(api.set_sprite(api.context, entity, &sprite) == 0 &&
            api.play_animation(api.context, entity, clipId.high, clipId.low, 1) == 0,
        "project frozen Clip unavailable to canonical API");
}
void Rejections(const std::filesystem::path &root) {
  Fixture fixture(root);
  const auto captured = fixture.Capture();
  Check(bool(captured), "baseline capture");
  const auto revision = fixture.content.Browser().Revision();
  const auto original = fixture.content.Browser().Find(atlasId)->twod;
  const auto reject = [&](std::span<const runtime::AssetUuid> selected, std::uint64_t project,
                          std::uint64_t generation, bool authorized,
                          editor::TwoDPlayAssetLimits limits = {}) {
    std::string error;
    Check(!editor::ProjectTwoDPlayAssets::Capture(fixture.workspace, fixture.content, project,
                                                  generation, selected, authorized, &error,
                                                  limits) &&
              !error.empty(),
          "invalid candidate admitted or missing diagnostic");
    Check(fixture.content.Browser().Revision() == revision &&
              fixture.content.Browser().Find(atlasId)->twod == original,
          "rejected capture changed Content");
  };
  reject(roots, projectGeneration, 71, false);
  reject(roots, 0, 71, true);
  reject(roots, projectGeneration + 1, 71, true);
  reject(roots, projectGeneration, 0, true);
  reject({}, projectGeneration, 71, true);
  const std::array duplicate{clipId, clipId};
  const std::array missing{otherId};
  const std::array zero{runtime::AssetUuid{}};
  for (auto selected :
       {std::span<const runtime::AssetUuid>(duplicate),
        std::span<const runtime::AssetUuid>(missing), std::span<const runtime::AssetUuid>(zero)})
    reject(selected, projectGeneration, 71, true);
  const auto size = captured->Assets()->PayloadBytes();
  Check(bool(fixture.Capture({2, size})), "exact closure budget rejected");
  for (auto limits : {editor::TwoDPlayAssetLimits{1, size},
                      {2, size - 1},
                      {0, size},
                      {4097, size},
                      {2, 0},
                      {2, 64 * 1024 * 1024 + 1}})
    reject(roots, projectGeneration, 71, true, limits);
  Check(fixture.content.Dependencies().Set(clipId, {}), "change graph");
  Check(!fixture.Capture() && !captured->Matches(fixture.workspace, fixture.content, true),
        "graph drift admitted");
  Check(fixture.content.Dependencies().Set(clipId, std::array{atlasId}), "restore graph");
  Check(fixture.Capture() && captured->Matches(fixture.workspace, fixture.content, true),
        "exact baseline restore rejected");
  const auto path = root / "Content/Atlas.NX2ATLAS";
  const auto metaPath = editor::AssetWorkspace::IdentitySidecar(path);
  const auto meta = Read(metaPath);
  Write(metaPath, {});
  Check(!fixture.Capture() && !captured->Matches(fixture.workspace, fixture.content, true),
        "corrupt sidecar admitted");
  Write(metaPath, meta);
  const auto recovery = root / ".nexora/workspace.recovery";
  Write(recovery, {});
  Check(!fixture.Capture() && !captured->Matches(fixture.workspace, fixture.content, true),
        "recovery scope admitted");
  std::filesystem::remove(recovery);
  const auto workspacePath = root / ".nexora/workspace";
  const auto workspaceTime = std::filesystem::last_write_time(workspacePath);
  std::filesystem::last_write_time(workspacePath, workspaceTime + std::chrono::seconds(1));
  Check(!fixture.Capture(), "external workspace change admitted");
  std::filesystem::last_write_time(workspacePath, workspaceTime);
  editor::ProjectWorkspace other;
  Check(other.Create(root.parent_path() / "other", "Other project"), "other project");
  Check(!editor::ProjectTwoDPlayAssets::Capture(other, fixture.content, projectGeneration, 71,
                                                roots, true),
        "foreign Content root admitted");
  const auto descriptor = Read(root / "project.nexora");
  Write(root / "project.nexora", Read(other.Root() / "project.nexora"));
  Check(!fixture.Capture() && !captured->Matches(fixture.workspace, fixture.content, true),
        "disk project UUID replacement admitted");
  Write(root / "project.nexora", descriptor);
  editor::ProjectWorkspace observer;
  Check(observer.Open(root, editor::ProjectAccess::ReadOnly), "read-only observer");
  Check(!editor::ProjectTwoDPlayAssets::Capture(observer, fixture.content, projectGeneration, 71,
                                                roots, true),
        "read-only admitted");
  editor::ProjectContentSession readOnly;
  Check(readOnly.Open(observer, fixture.index, projectGeneration, false), "read-only Content");
  Check(!editor::ProjectTwoDPlayAssets::Capture(fixture.workspace, readOnly, projectGeneration, 71,
                                                roots, true),
        "read-only Content admitted");
  Check(fixture.content.Conflicts().Detect(otherId, "editor", "disk", true),
        "unrelated conflict fixture");
  Check(fixture.content.Conflicts().Resolve(otherId, editor::DirtyConflictChoice::Compare),
        "unresolved Compare fixture");
  Check(!fixture.Capture() && !captured->Matches(fixture.workspace, fixture.content, true),
        "Compare conflict outside the selected closure admitted");
  Check(fixture.content.Conflicts().Resolve(otherId, editor::DirtyConflictChoice::Keep) &&
            fixture.Capture() && captured->Matches(fixture.workspace, fixture.content, true),
        "resolved unrelated conflict did not restore admission");
  Check(fixture.content.Conflicts().Detect(atlasId, "editor", "disk", true), "conflict fixture");
  Check(!fixture.Capture(), "unresolved source conflict admitted");
  Check(fixture.content.Conflicts().Resolve(atlasId, editor::DirtyConflictChoice::Keep),
        "resolve fixture");
  const auto alias = root / "Content/hardlink";
  std::error_code ec;
  std::filesystem::create_hard_link(path, alias, ec);
  if (!ec) {
    Check(!fixture.Capture(), "hard-linked source admitted");
    std::filesystem::remove(alias);
  }
  std::filesystem::rename(path, alias);
  std::filesystem::create_symlink(alias, path, ec);
  if (!ec) {
    Check(!fixture.Capture(), "symlink source admitted");
    std::filesystem::remove(path);
  }
  std::filesystem::rename(alias, path);
  Check(fixture.Capture() && captured->Matches(fixture.workspace, fixture.content, true),
        "source conservation after failed candidates");
  {
    core::JobSystem jobs{1};
    jobs.Start();
    editor::AssetImportQueue imports{jobs};
    Check(fixture.content.BeginReimport(imports, atlasId), "pending reimport fixture");
    Check(!fixture.Capture() && !captured->Matches(fixture.workspace, fixture.content, true),
          "pending authoring publication admitted");
    static_cast<void>(fixture.content.CancelReimport());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (fixture.content.ReimportBusy() && std::chrono::steady_clock::now() < deadline) {
      static_cast<void>(fixture.content.PollReimport());
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Check(!fixture.content.ReimportBusy(), "cancelled reimport did not drain");
  }
  Write(root / "Content/Clip.nx2clip",
        twod::EncodeClip({clipId, atlasId, true, {{999, 0.1, "missing region"}}}));
  Check(fixture.content.Reimport(clipId) && !fixture.Capture(),
        "invalid cooked frame region admitted");
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-project-2d-play-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Isolation(root / "isolation");
    Rejections(root / "reject");
    std::filesystem::remove_all(root);
    std::cout << "Project 2D Play assets: exact source scope, cooked closure and immutable "
                 "lifetime passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    std::filesystem::remove_all(root);
    return 1;
  }
}
