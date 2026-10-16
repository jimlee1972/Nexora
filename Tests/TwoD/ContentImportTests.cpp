#include "../../Engine/Editor/src/ReimportSource.h"
#include "Nexora/Editor/ProjectContent.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace nexora;
namespace {
constexpr runtime::AssetUuid atlasId{17, 23}, clipId{19, 29}, otherId{31, 37};
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
std::string Read(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), {}};
}
twod::Bytes Atlas(runtime::AssetUuid id = atlasId, std::byte color = std::byte{255}) {
  twod::Image image{1, 1, 1, {color, std::byte{0}, std::byte{0}, std::byte{255}}};
  auto atlas = twod::PackAtlas(id, std::span(&image, 1), 4);
  Check(bool(atlas), "pack fixture");
  return twod::EncodeAtlas(*atlas);
}
twod::Bytes Clip(runtime::AssetUuid id = clipId, runtime::AssetUuid atlas = atlasId) {
  return twod::EncodeClip({id, atlas, true, {{1, 0.1, "event"}}});
}
editor::ContentItem Item(std::shared_ptr<const editor::TwoDAssetPayload> payload) {
  return {payload->Id(),
          "Content/" + payload->Id().ToString() + std::string(payload->Type()),
          std::string(payload->Type()),
          "initial",
          editor::ThumbnailState::Ready,
          {},
          {},
          payload};
}
void AwaitWorker(editor::ProjectContentSession &content) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < deadline) {
    const auto status = content.ReimportStatus();
    if (status && status->state == editor::ImportOperationState::AwaitingPublish)
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  throw std::runtime_error("2D worker did not stage");
}
void Publish(editor::ProjectContentSession &content) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (content.ReimportBusy() && std::chrono::steady_clock::now() < deadline) {
    static_cast<void>(content.PollReimport());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Check(!content.ReimportBusy(), "2D publication did not finish");
}
void ParserAndIdentity(const std::filesystem::path &root) {
  const auto bytes = Atlas();
  auto imported = editor::ImportTwoDAsset(".nx2atlas", bytes, atlasId);
  Check(imported.payload && imported.error.empty() && imported.payload->Atlas() &&
            !imported.payload->Clip() && imported.payload->Dependencies().empty(),
        "owning atlas decode");
  const auto clip = editor::ImportTwoDAsset(".nx2clip", Clip());
  Check(clip.payload && clip.payload->Dependencies() == std::vector{atlasId} &&
            clip.payload->Clip()->id == clipId && !clip.payload->Atlas(),
        "clip decode");
  Check(!editor::ImportTwoDAsset(".nx2atlas", bytes, otherId).payload &&
            !editor::ImportTwoDAsset(".invalid", bytes).error.empty() &&
            !editor::ImportTwoDAsset(".nx2atlas", Clip()).payload,
        "type or UUID mismatch");
  auto trailing = bytes;
  trailing.push_back(std::byte{0});
  Check(!editor::ImportTwoDAsset(".nx2atlas", trailing).payload, "trailing source bytes");
  unsigned checks{};
  const auto cancelled =
      editor::ImportTwoDAsset(".nx2atlas", bytes, {}, [&] { return ++checks == 2; });
  Check(cancelled.cancelled && !cancelled.payload && cancelled.error.empty(),
        "decode cancellation");
  Write(root / "Atlas.NX2ATLAS", bytes);
  editor::AssetWorkspace workspace;
  std::string error;
  Check(
      !workspace.ImportTree(root, {}, {}, editor::AssetIdentityMode::PersistentReadOnly, &error) &&
          !std::filesystem::exists(root / "Atlas.NX2ATLAS.meta"),
      "read-only created identity");
  Check(workspace.ImportTree(root, {}, {}, editor::AssetIdentityMode::PersistentReadWrite, &error),
        "initial identity import");
  Check(workspace.Find(atlasId) && workspace.Find(atlasId)->twod &&
            Read(root / "Atlas.NX2ATLAS.meta").find(atlasId.ToString()) != std::string::npos,
        "embedded identity was replaced by path hash");
  const auto previous = workspace.Find(atlasId)->twod;
  Write(root / "Duplicate.nx2atlas", bytes);
  Check(
      !workspace.ImportTree(root, {}, {}, editor::AssetIdentityMode::PersistentReadWrite, &error) &&
          workspace.Find(atlasId)->twod == previous &&
          !std::filesystem::exists(root / "Duplicate.nx2atlas.meta"),
      "duplicate identity replaced index");
  std::filesystem::remove(root / "Duplicate.nx2atlas");
  Write(root / "New.nx2clip", trailing);
  Check(
      !workspace.ImportTree(root, {}, {}, editor::AssetIdentityMode::PersistentReadWrite, &error) &&
          workspace.Find(atlasId)->twod == previous &&
          !std::filesystem::exists(root / "New.nx2clip.meta"),
      "malformed new identity replaced index");
  std::filesystem::remove(root / "New.nx2clip");
  Write(root / "Atlas.NX2ATLAS", Atlas(otherId));
  Check(workspace.ImportTree(root, {}, {}, editor::AssetIdentityMode::PersistentReadOnly, &error) &&
            workspace.Find(atlasId)->state == editor::ImportState::Failed &&
            !workspace.Find(atlasId)->twod && workspace.Find(atlasId)->artifact_hash.empty(),
        "sidecar mismatch published typed data");
  Write(root / "Atlas.NX2ATLAS", bytes);
  const auto interrupted = editor::detail::ReadReimportSource(root / "Atlas.NX2ATLAS", ".NX2ATLAS",
                                                              atlasId, [] { return true; });
  Check(interrupted.cancelled && !interrupted.twod && interrupted.source_hash.empty() &&
            interrupted.artifact_hash.empty(),
        "cancelled read exposed data or hashes");
  {
    std::ofstream oversized(root / "Oversized.nx2atlas", std::ios::binary);
    oversized.seekp(static_cast<std::streamoff>(editor::kMaximumTwoDSourceBytes));
    oversized.put('x');
  }
  const auto oversized =
      editor::detail::ReadReimportSource(root / "Oversized.nx2atlas", ".nx2atlas", atlasId);
  Check(!oversized.twod && oversized.source_hash.empty() && oversized.artifact_hash.empty() &&
            oversized.error == "2D source exceeds the 64 MiB limit.",
        "oversized source admission");
}
void ProjectReimport(const std::filesystem::path &root) {
  editor::ProjectWorkspace project;
  Check(project.Create(root, "Typed 2D Content"), "create project");
  auto source = root / "Content/Atlas.nx2atlas";
  Write(source, Atlas());
  Write(root / "Content/Other.nx2atlas", Atlas(otherId));
  Write(root / "Content/Clip.nx2clip", Clip());
  editor::AssetWorkspace index;
  Check(index.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
        "index project");
  editor::ProjectContentSession content;
  Check(content.Open(project, index, 7), "open typed content");
  const auto initial = content.Browser().Find(atlasId)->twod;
  const auto initial_hash = content.Browser().Find(atlasId)->artifact_hash;
  Check(content.Dependencies().Forward(clipId) == std::vector{atlasId},
        "initial Clip dependency missing");
  Write(root / "Content/Clip.nx2clip", Clip(clipId, otherId));
  Check(content.Reimport(clipId) && content.Dependencies().Forward(clipId) == std::vector{otherId},
        "synchronous Clip dependency replacement");
  Write(root / "Content/Clip.nx2clip", Clip());
  Check(content.Reimport(clipId) && content.Dependencies().Forward(clipId) == std::vector{atlasId},
        "synchronous Clip dependency restoration");
  Check(content.Reimport(atlasId) && content.Browser().Find(atlasId)->artifact_hash == initial_hash,
        "initial inferred-UUID artifact hash differs from reimport hash");
  Check(content.Rename(atlasId, "Moved.asset"), "rename typed source");
  source = root / "Content/Moved.asset";
  Write(source, Atlas(atlasId, std::byte{7}));
  Check(content.Reimport(atlasId) &&
            content.Browser().Find(atlasId)->twod->Atlas()->rgba != initial->Atlas()->rgba &&
            content.Undo() &&
            content.Browser().Find(atlasId)->twod->Atlas()->rgba != initial->Atlas()->rgba,
        "sync reimport or Content Undo lost new/retained data");
  source = root / "Content/Atlas.nx2atlas";
  core::JobSystem jobs{1};
  jobs.Start();
  editor::AssetImportQueue imports{jobs};
  const auto before_worker = content.Browser().Find(atlasId)->twod;
  Write(source, Atlas(atlasId, std::byte{9}));
  Check(content.BeginReimport(imports, atlasId), "queue Atlas replacement");
  AwaitWorker(content);
  Check(content.Browser().Find(atlasId)->twod == before_worker,
        "Atlas worker published before Poll");
  Publish(content);
  Check(content.ReimportStatus()->state == editor::ImportOperationState::Succeeded &&
            content.Browser().Find(atlasId)->twod->Atlas()->rgba != before_worker->Atlas()->rgba &&
            initial->Atlas()->rgba != before_worker->Atlas()->rgba,
        "immutable Atlas worker publication");
  Write(root / "Content/Clip.nx2clip", Clip(clipId, otherId));
  Check(content.BeginReimport(imports, clipId), "queue Clip replacement");
  const auto previous_clip = content.Browser().Find(clipId)->twod;
  AwaitWorker(content);
  Check(content.Browser().Find(clipId)->twod == previous_clip &&
            content.Dependencies().Forward(clipId) == std::vector{atlasId},
        "worker published before Poll");
  Publish(content);
  Check(content.ReimportStatus()->state == editor::ImportOperationState::Succeeded &&
            content.Browser().Find(clipId)->twod->Clip()->atlas == otherId &&
            content.Dependencies().Forward(clipId) == std::vector{otherId},
        "atomic typed/dependency publication");
  Write(root / "Content/Clip.nx2clip", Clip(clipId, clipId));
  const auto clip_owner = content.Browser().Find(clipId)->twod;
  const auto revision = content.Browser().Revision();
  Check(!content.Reimport(clipId) && content.Browser().Find(clipId)->twod == clip_owner &&
            content.Browser().Revision() == revision &&
            content.Dependencies().Forward(clipId) == std::vector{otherId},
        "dependency-cycle rollback");
  Check(content.BeginReimport(imports, clipId), "queue dependency cycle");
  Publish(content);
  Check(content.ReimportStatus()->state == editor::ImportOperationState::Failed &&
            content.Browser().Find(clipId)->twod == clip_owner &&
            content.Dependencies().Forward(clipId) == std::vector{otherId},
        "worker cycle rollback");
  const auto owner = content.Browser().Find(atlasId)->twod;
  const auto artifact = content.Browser().Find(atlasId)->artifact_hash;
  Write(source, Atlas(otherId));
  Check(!content.Reimport(atlasId) && content.Browser().Find(atlasId)->twod == owner &&
            content.Browser().Find(atlasId)->artifact_hash == artifact,
        "sync mismatched UUID published");
  Check(content.BeginReimport(imports, atlasId), "queue invalid source");
  Publish(content);
  Check(content.ReimportStatus()->state == editor::ImportOperationState::Failed &&
            content.Browser().Find(atlasId)->twod == owner &&
            std::ranges::any_of(content.ReimportStatus()->diagnostics,
                                [](const auto &d) { return d.code == "reimport.twod_failed"; }),
        "worker invalid-source conservation");
  Write(source, Atlas(atlasId, std::byte{9}));
  Check(content.BeginReimport(imports, atlasId), "queue stale source");
  AwaitWorker(content);
  auto changed = Atlas(atlasId, std::byte{11});
  changed.push_back(std::byte{0});
  Write(source, changed);
  Publish(content);
  Check(content.ReimportStatus()->state == editor::ImportOperationState::Stale &&
            content.Browser().Find(atlasId)->twod == owner,
        "stale source published");
  Write(source, Atlas(atlasId, std::byte{13}));
  Check(content.BeginReimport(imports, atlasId) && content.CancelReimport(), "cancel source job");
  Publish(content);
  Check(content.ReimportStatus()->state == editor::ImportOperationState::Cancelled &&
            content.Browser().Find(atlasId)->twod == owner,
        "cancelled source published");
  editor::ProjectContentSession observer;
  Check(content.BeginReimport(imports, atlasId), "queue stale dependencies");
  AwaitWorker(content);
  Check(content.Dependencies().Set(atlasId, std::array{otherId}), "change observed dependencies");
  Publish(content);
  Check(content.ReimportStatus()->state == editor::ImportOperationState::Stale &&
            content.Browser().Find(atlasId)->twod == owner &&
            content.Dependencies().Forward(atlasId) == std::vector{otherId},
        "stale dependency result published");
  Check(content.Dependencies().Set(atlasId, {}), "restore dependencies");
  Check(content.BeginReimport(imports, atlasId), "queue stale generation");
  AwaitWorker(content);
  const auto current = content.Browser().Items();
  const std::vector<editor::ContentItem> reset(current.begin(), current.end());
  Check(content.Browser().Reset(reset, 9), "change project generation");
  Publish(content);
  Check(content.ReimportStatus()->state == editor::ImportOperationState::Stale &&
            content.Browser().Find(atlasId)->twod == owner,
        "stale generation result published");
  Check(observer.Open(project, index, 8, false) && !observer.Reimport(atlasId) &&
            !observer.BeginReimport(imports, atlasId),
        "read-only source publication");
  jobs.Stop();
}
void CountBudgets(const std::filesystem::path &root) {
  std::vector<editor::ContentItem> items;
  for (std::uint64_t n = 1; n <= 4096; ++n) {
    const runtime::AssetUuid id{71, n};
    const auto bytes = Clip(id);
    const auto payload = editor::ImportTwoDAsset(".nx2clip", bytes).payload;
    Check(bool(payload), "count fixture");
    items.push_back(Item(payload));
    Write(root / (std::to_string(n) + ".nx2clip"), bytes);
  }
  const runtime::AssetUuid extraId{72, 1};
  const auto extra = Item(editor::ImportTwoDAsset(".nx2clip", Clip(extraId)).payload);
  const auto discovered = Item(editor::ImportTwoDAsset(".nx2clip", Clip({72, 2})).payload);
  items.push_back({extraId, "Content/Extra.nx2clip", ".nx2clip", "old"});
  editor::ContentBrowserModel browser;
  Check(browser.Reset(items, 5), "4096 assets rejected");
  auto revision = browser.Revision();
  Check(!browser.PublishArtifact(extraId, "new", editor::ThumbnailState::Ready, nullptr, {}, {},
                                 extra.twod) &&
            browser.Revision() == revision && !browser.Find(extraId)->twod,
        "over-count publication");
  Check(browser.Delete(std::array{items[0].id}), "count delete");
  revision = browser.Revision();
  Check(!browser.PublishArtifact(extraId, "new", editor::ThumbnailState::Ready, nullptr, {}, {},
                                 extra.twod) &&
            !browser.Discover(discovered) && browser.Revision() == revision && browser.Undo(),
        "retained Undo count budget");
  items.back() = extra;
  Check(!browser.Reset(items, 6) && browser.ProjectGeneration() == 5, "over-count Reset");
  Write(root / "extra.nx2clip", Clip(extraId));
  editor::AssetWorkspace index;
  Check(index.ImportTree(root), "budget workspace failed");
  std::size_t successes{}, failures{};
  for (const auto &entry : index.Entries()) {
    if (entry.state == editor::ImportState::Imported) {
      Check(entry.twod && !entry.artifact_hash.empty(), "typed index missing payload");
      ++successes;
    } else {
      Check(entry.state == editor::ImportState::Failed && !entry.twod &&
                entry.artifact_hash.empty(),
            "over-count index published payload");
      ++failures;
    }
  }
  Check(successes == 4096 && failures == 1, "workspace count conservation");
}
void ByteBudgets(const std::filesystem::path &root) {
  std::vector<editor::ContentItem> items;
  for (std::uint64_t n = 1; n <= 4; ++n) {
    twod::Atlas atlas;
    atlas.id = {81, n};
    atlas.width = atlas.height = 2048;
    atlas.regions = {{1, 1, 1, 2046, 2046}};
    atlas.rgba.resize(2048 * 2048 * 4, std::byte{255});
    const auto bytes = twod::EncodeAtlas(atlas);
    Write(root / (std::to_string(n) + ".nx2atlas"), bytes);
    auto payload = editor::ImportTwoDAsset(".nx2atlas", bytes).payload;
    Check(bool(payload), "large Atlas fixture");
    items.push_back(Item(std::move(payload)));
  }
  const auto extra = items.back();
  items.back().twod.reset();
  editor::ContentBrowserModel browser;
  Check(browser.Reset(items, 17), "three large Atlases rejected");
  const auto revision = browser.Revision();
  Check(!browser.PublishArtifact(extra.id, "new", editor::ThumbnailState::Ready, nullptr, {}, {},
                                 extra.twod) &&
            browser.Revision() == revision && !browser.Find(extra.id)->twod,
        "over-byte publication");
  auto wrong = items.front();
  Check(browser.Delete(std::array{items[0].id}) &&
            !browser.PublishArtifact(extra.id, "new", editor::ThumbnailState::Ready, nullptr, {},
                                     {}, extra.twod) &&
            browser.Undo(),
        "retained Undo byte budget");
  wrong.type = ".nx2clip";
  Check(!browser.Reset(std::span(&wrong, 1), 18), "mismatched typed importer admitted");
  wrong = items.front();
  wrong.id = otherId;
  Check(!browser.Reset(std::span(&wrong, 1), 18), "mismatched typed UUID admitted");
  items.back() = extra;
  Check(!browser.Reset(items, 18) && browser.ProjectGeneration() == 17, "over-byte Reset");
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-typed-2d-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    ParserAndIdentity(root / "Identity");
    ProjectReimport(root / "Project");
    CountBudgets(root / "Count");
    ByteBudgets(root / "Bytes");
    editor::AssetWorkspace byte_index;
    Check(byte_index.ImportTree(root / "Bytes"), "byte budget index");
    std::size_t successes{}, failures{};
    for (const auto &entry : byte_index.Entries()) {
      if (entry.state == editor::ImportState::Imported) {
        Check(entry.twod && !entry.artifact_hash.empty(), "byte index lost payload");
        ++successes;
      } else {
        Check(!entry.twod && entry.artifact_hash.empty(), "byte index exceeded retained budget");
        ++failures;
      }
    }
    Check(successes == 3 && failures == 1, "workspace byte budget conservation");
    std::filesystem::remove_all(root);
    std::cout << "2D typed Content: stable identity, immutable reimport, dependency and budget "
                 "conservation passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::filesystem::remove_all(root);
    std::cerr << error.what() << '\n';
    return 1;
  }
}
