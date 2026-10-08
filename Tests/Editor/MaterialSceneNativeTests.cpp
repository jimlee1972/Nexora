#include "MaterialScenePreview.h"
#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/Presentation/Surface.h"
#include "ScenePreviewCandidates.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef None
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using namespace Nexora::Presentation;
void Require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
void Source(const std::filesystem::path &path, unsigned channel) {
  std::ofstream out(path);
  out << "NEXORA_MATERIAL 1\nbase_color " << (channel == 0) << ' ' << (channel == 1) << ' '
      << (channel == 2) << "\nmetallic 0\nroughness 0.5\nocclusion 1\nemission 0 0 0\n";
  Require(out.good(), "material source write failed");
}
unsigned Channel(unsigned long pixel, unsigned long mask) {
  if (!mask)
    return 0;
  while (!(mask & 1)) {
    mask >>= 1;
    pixel >>= 1;
  }
  return static_cast<unsigned>((pixel & mask) * 255 / mask);
}
void Pixels(Display *display, ::Window window, unsigned left_channel,
            const std::filesystem::path &capture) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    XSync(display, False);
    auto *image = XGetImage(display, window, 0, 0, 640, 480, AllPlanes, ZPixmap);
    Require(image != nullptr, "native material pixels unavailable");
    const auto rgb = [&](unsigned x, unsigned y) {
      const auto p = XGetPixel(image, x, y);
      return std::array{Channel(p, image->red_mask), Channel(p, image->green_mask),
                        Channel(p, image->blue_mask)};
    };
    const auto left = rgb(160, 240), right = rgb(480, 240);
    const bool matches = left[left_channel] > 50 &&
                         left[left_channel] > left[(left_channel + 1) % 3] + 30 &&
                         left[left_channel] > left[(left_channel + 2) % 3] + 30 && right[1] > 50 &&
                         right[1] > right[0] + 30 && right[1] > right[2] + 30;
    if (matches && !capture.empty()) {
      std::ofstream out(capture, std::ios::binary);
      out << "P6\n640 480\n255\n";
      for (unsigned y = 0; y < 480; ++y)
        for (unsigned x = 0; x < 640; ++x) {
          const auto color = rgb(x, y);
          const char bytes[]{static_cast<char>(color[0]), static_cast<char>(color[1]),
                             static_cast<char>(color[2])};
          out.write(bytes, 3);
        }
      Require(out.good(), "material pixel capture write failed");
    }
    XDestroyImage(image);
    if (matches)
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  throw std::runtime_error(
      "source material assignment/reimport did not reach independent GPU pixels");
}
struct Fixture {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-material-native-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace workspace;
  editor::AssetWorkspace assets;
  editor::ProjectContentSession content;
  editor::MaterialAssetCatalog materials;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Materials");
  editor::SceneDocument scene{world, scene_id};
  ~Fixture() {
    workspace = {};
    std::filesystem::remove_all(root);
  }
};
} // namespace
int main(int argc, char **argv) {
  try {
    Fixture f;
    Require(f.workspace.Create(f.root, "Native Materials") && f.world.Activate(f.scene_id),
            "native material project failed");
    Source(f.root / "Content/Left.nmaterial", 0);
    Source(f.root / "Content/Right.nmaterial", 1);
    std::ofstream(f.root / "Content/Triangle.obj")
        << "v -0.35 -0.5 0.5\nv 0.35 -0.5 0.5\nv 0 0.5 0.5\nf 1 2 3\n";
    Require(f.assets.ImportTree(f.root / "Content", {}, {},
                                editor::AssetIdentityMode::PersistentReadWrite) &&
                f.content.Open(f.workspace, f.assets, 7, true) &&
                f.materials.PublishContent(f.content.Browser()),
            "native typed material import failed");
    const auto &entries = f.assets.Entries();
    const auto left = entries[0].id, right = entries[1].id;
    Require(entries[0].material && entries[1].material && entries[2].mesh,
            "native typed asset sort mismatch");
    const std::array ids{f.scene.Create("Left"), f.scene.Create("Right")};
    Require(f.scene.SetMeshRenderers(
                std::array{*f.scene.Key(ids[0]), *f.scene.Key(ids[1])},
                std::array<std::optional<runtime::MeshComponent>, 2>{
                    runtime::MeshComponent{editor::MeshResourceId(entries[2].id), {UINT64_MAX}},
                    runtime::MeshComponent{editor::MeshResourceId(entries[2].id), {71}}}) &&
                f.scene.SetTransforms(
                    std::array{*f.scene.Key(ids[0]), *f.scene.Key(ids[1])},
                    std::array{runtime::Transform{-0.5, 0, 0}, runtime::Transform{0.5, 0, 0}}),
            "native scene components failed");
    for (unsigned i = 0; i < 2; ++i) {
      Require(f.scene.Select(std::array{ids[i]}) &&
                  editor::AssignMaterialAsset(f.scene, *f.scene.Key(ids[i]), i ? right : left, 7,
                                              f.content, f.materials, true),
              "native material UUID assignment failed");
    }
    const auto saved = f.root / "Content/Test.scene";
    Require(f.scene.Save(saved) && f.scene.Reload(saved),
            "native material reference reopen failed");
    Require(f.scene.MeshRenderer(*f.scene.Key(ids[0]))->material.shader == UINT64_MAX,
            "legacy shader ID changed");
    editor::MeshAssetCatalog mesh_catalog;
    Require(mesh_catalog.PublishContent(f.content.Browser()), "native mesh catalog failed");
    auto prepared = editor::preview::PrepareNativeSceneMeshes(f.scene, mesh_catalog, f.content, 7);
    Require(prepared.entities.size() == 2 && prepared.unavailable == 0,
            "native authored mesh catalog resolution failed");
    const auto range = prepared.ranges.at(editor::MeshResourceId(entries[2].id));
    auto geometry = std::move(prepared.geometry);
    Require(editor::preview::PrepareMaterialTangents(geometry),
            "native material tangent conversion failed");
    std::array<SceneInstance, 2> instances;
    for (unsigned i = 0; i < 2; ++i) {
      const auto exact = editor::preview::AffineInstance(*f.scene.WorldMatrix(ids[i]));
      Require(exact.has_value(), "native material affine instance failed");
      instances[i] = *exact;
    }
    auto windows = Nexora::Window::CreateWindowSystem();
    Require(windows != nullptr, "native material window system failed");
    const auto window = windows->Create({"Nexora Editor material pixels", 640, 480, true, false});
    Require(static_cast<bool>(window), "native material window failed");
    Require(windows->Show(window.handle, true) == Nexora::Window::WindowError::None,
            "native material window show failed");
    SurfaceDescriptor descriptor{};
    descriptor.window = window.handle;
    descriptor.width = 640;
    descriptor.height = 480;
    descriptor.backend = SurfaceBackend::Vulkan;
    auto surface = CreateSurface(descriptor, *windows);
    Require(surface != nullptr, "native material Vulkan surface failed");
    auto *display = XOpenDisplay(nullptr);
    Require(display != nullptr, "native material display failed");
    const auto native = reinterpret_cast<::Window>(windows->NativeHandle(window.handle));
    const auto submit = [&](unsigned expected, const std::filesystem::path &capture) {
      const auto palette = editor::preview::PrepareMaterialPalette(f.scene, f.materials, 7, ids);
      Require(palette.authored && palette.materials.size() == 3 && palette.unavailable == 0,
              "native source palette failed");
      std::array<SceneMeshBatch, 2> batches{
          {{range.firstIndex, range.indexCount, 0, 1, palette.entities.at(ids[0])},
           {range.firstIndex, range.indexCount, 1, 1, palette.entities.at(ids[1])}}};
      SceneDrawData draw;
      draw.vertices = geometry.vertices;
      draw.indices = geometry.indices;
      draw.instances = instances;
      draw.batches = batches;
      draw.materials = palette.materials;
      draw.pbr = true;
      draw.cameraPosition = {0, 0, 3};
      draw.light_direction[0] = draw.light_direction[1] = 0;
      draw.light_direction[2] = -1;
      draw.light_color[0] = draw.light_color[1] = draw.light_color[2] = 1;
      Require(surface->Acquire() == SurfaceStatus::Ready &&
                  surface->DrawScene(draw) == SurfaceStatus::Ready &&
                  surface->Present() == SurfaceStatus::Ready,
              "native source PBR submission failed");
      Pixels(display, native, expected, capture);
    };
    const std::filesystem::path capture = argc > 1 ? argv[1] : "";
    submit(0, capture.empty() ? capture : capture.parent_path() / "assigned.ppm");
    Source(f.root / "Content/Left.nmaterial", 2);
    Require(f.content.Reimport(left) && f.materials.PublishContent(f.content.Browser()),
            "native material reimport publication failed");
    submit(2, capture);
    std::ofstream(f.root / "Content/Left.nmaterial") << "NEXORA_MATERIAL 2";
    Require(!f.content.Reimport(left) && f.materials.PublishContent(f.content.Browser()),
            "invalid material reimport was published");
    submit(2, {});
    Require(f.scene.Select(std::array{ids[0]}) &&
                editor::AssignMaterialAsset(f.scene, *f.scene.Key(ids[0]), right, 7, f.content,
                                            f.materials, true) &&
                f.scene.Undo(),
            "native material assignment Undo failed");
    submit(2, {});
    Require(f.scene.Reload(saved), "native saved UUID reopen after reimport failed");
    submit(2, {});
    Require(surface->Diagnostics().sceneDrawCalls == 5, "native material scene count failed");
    surface.reset();
    XCloseDisplay(display);
    windows->Destroy(window.handle);
    std::cout << "PASS: imported UUID material assignment, two PBR colors, reimport pixels, "
                 "rejected version, Undo/reopen; legacy shader IDs preserved\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
