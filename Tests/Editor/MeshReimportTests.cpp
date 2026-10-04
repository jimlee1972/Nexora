#include "Nexora/Editor/MeshAssetCatalog.h"
#include "Nexora/Editor/ProjectContent.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void WriteTriangle(const std::filesystem::path &path, int width) {
  std::ofstream(path) << "v 0 0 0\nv " << width << " 0 0\nv 0 1 0\nf 1 2 3\n";
}
void AwaitWorker(editor::ProjectContentSession &content) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    const auto status = content.ReimportStatus();
    if (status && status->state == editor::ImportOperationState::AwaitingPublish)
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  throw std::runtime_error("mesh worker did not stage its result");
}
void AwaitPublication(editor::ProjectContentSession &content) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (content.ReimportBusy() && std::chrono::steady_clock::now() < deadline) {
    static_cast<void>(content.PollReimport());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Require(!content.ReimportBusy(), "mesh reimport did not finish");
}
void TestBudget() {
  auto mesh = std::make_shared<editor::MeshGeometry>();
  mesh->vertices.resize(10000);
  mesh->indices = {0, 1, 2};
  const auto bytes = mesh->vertices.capacity() * sizeof(editor::MeshVertex) +
                     mesh->indices.capacity() * sizeof(std::uint16_t);
  const auto count = editor::kMaximumWorkspaceMeshBytes / bytes;
  std::vector<editor::ContentItem> items;
  for (std::size_t i = 0; i < count; ++i)
    items.push_back({{1, i + 1},
                     "Content/" + std::to_string(i) + ".obj",
                     ".obj",
                     "old",
                     editor::ThumbnailState::Ready,
                     mesh});
  editor::ContentBrowserModel browser;
  Require(browser.Reset(items, 77), "bounded content fixture failed");
  const auto revision = browser.Revision();
  auto larger = std::make_shared<editor::MeshGeometry>(*mesh);
  larger->vertices.resize(20000);
  std::string error;
  Require(!browser.PublishArtifact(items.front().id, "new", editor::ThumbnailState::Ready, &error,
                                   larger) &&
              !error.empty() && browser.Revision() == revision &&
              browser.Find(items.front().id)->artifact_hash == "old" &&
              browser.Find(items.front().id)->mesh == mesh,
          "over-budget replacement published hash or partial geometry");
  items.push_back(
      {{2, 1}, "Content/Overflow.obj", ".obj", "overflow", editor::ThumbnailState::Ready, mesh});
  Require(!browser.Reset(items, 78) && browser.ProjectGeneration() == 77 &&
              browser.Revision() == revision && browser.Items().size() == count,
          "over-budget reset replaced the previous live model");
}
} // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-mesh-reimport-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    TestBudget();
    editor::ProjectWorkspace workspace;
    std::string error;
    Require(workspace.Create(root, "Mesh Reimport", &error), "workspace creation failed");
    auto source = root / "Content/Triangle.obj";
    WriteTriangle(source, 1);
    editor::AssetWorkspace assets;
    Require(assets.ImportTree(root / "Content", {}, {},
                              editor::AssetIdentityMode::PersistentReadWrite, &error),
            "mesh import failed");
    const auto asset = assets.Entries().front().id;
    // Pending reimports borrow this queue, including during exception unwinding.
    core::JobSystem jobs{1};
    jobs.Start();
    editor::AssetImportQueue imports{jobs};
    editor::ProjectContentSession content;
    Require(content.Open(workspace, assets, 7, true, &error), "content opening failed");
    const auto opened_revision = content.Browser().Revision();
    Require(content.Open(workspace, assets, 7, true, &error) &&
                content.Browser().Revision() > opened_revision,
            "reopening content reused the live catalog revision");
    editor::MeshAssetCatalog catalog;
    Require(catalog.PublishContent(content.Browser()), "content catalog publication failed");
    const auto original = *catalog.ResolveAsset(asset, 7);
    Require(content.Rename(asset, "Moved.mesh", &error), "mesh rename failed");
    source = root / "Content/Moved.mesh";
    WriteTriangle(source, 2);
    const auto before_sync = content.Browser().Revision();
    Require(content.Reimport(asset, &error) && content.Browser().Revision() > before_sync &&
                content.Browser().Find(asset)->mesh->maximum[0] == 2 && content.Undo(&error) &&
                content.Browser().Find(asset)->mesh->maximum[0] == 2,
            "synchronous reimport or rename Undo lost the newer geometry");
    source = root / "Content/Triangle.obj";
    Require(catalog.PublishContent(content.Browser()) &&
                catalog.ResolveAsset(asset, 7)->resource == original.resource &&
                original.geometry->maximum[0] == 1,
            "reimport changed persistent identity or invalidated a retained snapshot");
    Require(content.Rename(asset, "Worker.mesh", &error), "worker mesh rename failed");
    source = root / "Content/Worker.mesh";
    WriteTriangle(source, 3);
    Require(content.BeginReimport(imports, asset, &error),
            "background mesh reimport failed to start");
    AwaitWorker(content);
    Require(content.Browser().Find(asset)->mesh->maximum[0] == 2,
            "worker mutated live geometry before publication");
    AwaitPublication(content);
    Require(content.ReimportStatus()->state == editor::ImportOperationState::Succeeded &&
                content.Browser().Find(asset)->mesh->maximum[0] == 3 &&
                catalog.PublishContent(content.Browser()) &&
                catalog.ResolveAsset(asset, 7)->resource == original.resource &&
                catalog.ResolveAsset(asset, 7)->geometry->maximum[0] == 3,
            "background mesh publication lost geometry or resource identity");
    Require(content.Undo(&error) && content.Browser().Find(asset)->mesh->maximum[0] == 3,
            "background publication was lost after rename Undo");
    source = root / "Content/Triangle.obj";
    const auto published = content.Browser().Find(asset)->mesh;
    const auto artifact = content.Browser().Find(asset)->artifact_hash;
    const auto revision = content.Browser().Revision();
    std::ofstream(source) << "invalid OBJ\n";
    Require(content.BeginReimport(imports, asset), "invalid mesh request failed to start");
    AwaitPublication(content);
    Require(content.ReimportStatus()->state == editor::ImportOperationState::Failed &&
                content.Browser().Find(asset)->mesh == published &&
                content.Browser().Find(asset)->artifact_hash == artifact &&
                content.Browser().Revision() == revision,
            "invalid mesh reimport replaced live geometry/hash/revision");
    WriteTriangle(source, 4);
    Require(content.BeginReimport(imports, asset) && content.CancelReimport(),
            "mesh cancellation failed");
    AwaitPublication(content);
    Require(content.ReimportStatus()->state == editor::ImportOperationState::Cancelled &&
                content.Browser().Find(asset)->mesh == published,
            "cancelled mesh reimport published geometry");
    Require(content.BeginReimport(imports, asset), "stale mesh request failed to start");
    AwaitWorker(content);
    std::ofstream(source) << "a different source revision\n";
    AwaitPublication(content);
    Require(content.ReimportStatus()->state == editor::ImportOperationState::Stale &&
                content.Browser().Find(asset)->mesh == published &&
                content.Browser().Find(asset)->artifact_hash == artifact,
            "stale mesh reimport published geometry or hash");
    {
      std::ofstream oversized(source, std::ios::binary);
      oversized.seekp(editor::kMaximumObjSourceBytes);
      oversized.put('x');
    }
    Require(content.BeginReimport(imports, asset), "oversized mesh request failed to start");
    AwaitPublication(content);
    Require(content.ReimportStatus()->state == editor::ImportOperationState::Failed &&
                content.Browser().Find(asset)->mesh == published,
            "oversized mesh reimport published geometry");
    Require(content.Delete(std::array{asset}) && catalog.PublishContent(content.Browser()) &&
                !catalog.ResolveAsset(asset, 7) && content.Undo() &&
                catalog.PublishContent(content.Browser()) &&
                catalog.ResolveAsset(asset, 7)->geometry == published,
            "delete/Undo or live catalog refresh lost newer geometry");
    imports.Shutdown();
    jobs.Stop();
    workspace = {};
    std::filesystem::remove_all(root);
    std::cout << "Mesh reimport publication contracts passed\n";
    return 0;
  } catch (const std::exception &failure) {
    std::filesystem::remove_all(root);
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
