#include "Nexora/Editor/AssetImport.h"
#include "Nexora/Editor/MeshImport.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
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

constexpr std::string_view triangle = "v -1 0 0\nv 1 0 0\nv 0 2 0\nf 1 2 3\n";

void TestGeometry() {
  const auto imported = editor::ImportObjMesh(triangle);
  Require(imported.geometry && imported.geometry->vertices.size() == 3 &&
              imported.geometry->indices == std::vector<std::uint16_t>{0, 1, 2} &&
              imported.geometry->minimum == std::array{-1.0F, 0.0F, 0.0F} &&
              imported.geometry->maximum == std::array{1.0F, 2.0F, 0.0F} &&
              imported.geometry->vertices[0].normal == std::array{0.0F, 0.0F, 1.0F} &&
              imported.geometry->vertices[0].uv == std::array{0.0F, 0.0F},
          "generated normals, bounds or indexed triangle geometry failed");
  const auto relative =
      editor::ImportObjMesh("# geometry only\r\nv -1 0 0\r\nv 1 0 0\r\nv 0 2 0\r\n"
                            "vt 0 0\nvt 1 0\nvt 0.5 1\nvn 0 0 3\n"
                            "mtllib never-open.mtl\nusemtl example\no Shape\ng Part\ns off\n"
                            "f -3/-3/-1 -2/-2/-1 -1/-1/-1 # first\n"
                            "f +1/1/1 +2/2/1 +3/3/1\n");
  Require(relative.geometry && relative.geometry->vertices.size() == 3 &&
              relative.geometry->indices == std::vector<std::uint16_t>{0, 1, 2, 0, 1, 2} &&
              relative.geometry->vertices[2].uv == std::array{0.5F, 1.0F} &&
              relative.geometry->vertices[0].normal == std::array{0.0F, 0.0F, 1.0F},
          "OBJ relative indices, UVs, explicit normal normalization or deduplication failed");
  const auto normals_only =
      editor::ImportObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n");
  Require(normals_only.geometry && normals_only.geometry->vertices.size() == 3,
          "OBJ normal-only corner syntax failed");
  const auto flat = editor::ImportObjMesh(std::string(triangle) + "f 1 3 2\n");
  Require(flat.geometry && flat.geometry->vertices.size() == 6 &&
              flat.geometry->vertices[0].normal[2] == 1.0F &&
              flat.geometry->vertices[3].normal[2] == -1.0F,
          "flat normals were incorrectly shared across oppositely wound faces");
  const auto large = editor::ImportObjMesh("v -1e30 0 0\nv 1e30 0 0\nv 0 1e30 0\nf 1 2 3\n");
  Require(large.geometry && large.geometry->vertices[0].normal[2] == 1.0F,
          "finite large positions overflowed face normal construction");
}

void TestRejectionAndCancellation() {
  for (const auto source : {
           "",
           "v nan 0 0\n",
           "v inf 0 0\n",
           "v 1e100 0 0\n",
           "v 0 0\n",
           "v 0 0 0 1\n",
           "vn 0 0 0\n",
           "vt 0\n",
           "vt 0 0 0\n",
           "p 1\n",
           "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 2 3\n",
           "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n",
           "v 0 0 0\nv 1 0 0\nv 0 1 0\nf -4 2 3\n",
           "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2\n",
           "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3 1\n",
           "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1/ 2 3\n",
           "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1/1 2 3\n",
           "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1// 2 3\n",
           "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1//1/2 2 3\n",
           "v 0 0 0\nv 1 0 0\nv 2 0 0\nf 1 2 3\n",
           "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 9223372036854775808 2 3\n",
           "v 0 0 0\nv 1 0 0\nv 0 1 0\nf -9223372036854775808 2 3\n",
       }) {
    const auto result = editor::ImportObjMesh(source);
    Require(!result.geometry && !result.error.empty() && !result.cancelled,
            "malformed OBJ input produced partial geometry or no diagnostic");
  }
  const auto missing = editor::ImportObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n");
  Require(missing.line == 4, "OBJ diagnostic lost its source line");
  const std::string nul(1, '\0');
  Require(!editor::ImportObjMesh(nul).geometry, "NUL bytes accepted");
  const std::string oversized(editor::kMaximumObjSourceBytes + 1, ' ');
  Require(!editor::ImportObjMesh(oversized).geometry, "oversized OBJ source accepted");
  std::size_t polls{};
  const auto cancelled = editor::ImportObjMesh(triangle, [&] { return ++polls == 4; });
  Require(cancelled.cancelled && !cancelled.geometry && cancelled.error.empty() &&
              cancelled.line == 4,
          "cancellation published partial geometry or lost its state");
  auto expanded = std::string(triangle.substr(0, triangle.find('f')));
  for (std::size_t face = 0; face < 21845; ++face)
    expanded += "f 1 2 3\n";
  const auto maximum = editor::ImportObjMesh(expanded);
  Require(maximum.geometry && maximum.geometry->vertices.size() == 65535,
          "maximum expanded OBJ vertex boundary rejected");
  expanded += "f 1 2 3\n";
  Require(!editor::ImportObjMesh(expanded).geometry, "expanded OBJ vertex overflow accepted");
}

void TestWorkspace() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-mesh-import-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  struct Cleanup final {
    std::filesystem::path root;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove_all(root, error);
    }
  } cleanup{root};
  std::ofstream(root / "Shape.obj") << triangle;
  std::ofstream(root / "Broken.obj") << "f 0 0 0\n";
  editor::AssetWorkspace workspace;
  std::string error;
  Require(
      workspace.ImportTree(root, {}, {}, editor::AssetIdentityMode::PersistentReadWrite, &error),
      "OBJ workspace import failed");
  const auto shape = std::ranges::find(workspace.Entries(), std::string("Shape.obj"),
                                       &editor::AssetEntry::relative_path);
  const auto broken = std::ranges::find(workspace.Entries(), std::string("Broken.obj"),
                                        &editor::AssetEntry::relative_path);
  Require(shape != workspace.Entries().end() && shape->state == editor::ImportState::Imported &&
              shape->mesh && shape->mesh->indices.size() == 3 && !shape->artifact_hash.empty() &&
              broken != workspace.Entries().end() && broken->state == editor::ImportState::Failed &&
              !broken->mesh && broken->error.find("line 1") != std::string::npos,
          "workspace lost OBJ geometry, identity or failed-asset diagnostics");
  const auto id = shape->id;
  const auto hash = shape->artifact_hash;
  const auto retained = shape->mesh;
  std::filesystem::rename(root / "Shape.obj", root / "Moved.obj");
  std::filesystem::rename(root / "Shape.obj.meta", root / "Moved.obj.meta");
  Require(workspace.ImportTree(root, {}, {}, editor::AssetIdentityMode::PersistentReadOnly) &&
              workspace.Find(id) && workspace.Find(id)->mesh &&
              workspace.Find(id)->artifact_hash == hash && retained->indices.size() == 3,
          "OBJ move/reopen changed UUID/hash or invalidated owning geometry");

  core::JobSystem jobs{1};
  jobs.Start();
  editor::AssetImportQueue imports{jobs};
  const auto operation = imports.Start(
      editor::WorkspaceImportRequest{77, root, editor::AssetIdentityMode::PersistentReadOnly});
  Require(operation != 0, "OBJ background import did not start");
  std::optional<editor::ImportOperationResult> staged;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!staged && std::chrono::steady_clock::now() < deadline) {
    staged = imports.TakeResult(operation);
    std::this_thread::yield();
  }
  Require(staged && staged->snapshot.project_generation == 77 && staged->workspace &&
              staged->workspace->Find(id)->mesh &&
              std::ranges::any_of(staged->snapshot.diagnostics,
                                  [](const auto &diagnostic) {
                                    return diagnostic.code == "asset.import_failed" &&
                                           diagnostic.message.find("line 1") != std::string::npos;
                                  }),
          "background OBJ import lost owning geometry, generation or structured diagnostic");
  imports.Shutdown();
  jobs.Stop();
}
} // namespace

int main() {
  try {
    TestGeometry();
    TestRejectionAndCancellation();
    TestWorkspace();
    std::cout
        << "PASS: bounded OBJ geometry, rejection, cancellation and background workspace import\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
