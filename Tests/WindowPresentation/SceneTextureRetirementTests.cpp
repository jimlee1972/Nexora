#include "Nexora/Presentation/Surface.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void Require(bool passed, const char *message) {
  if (!passed)
    throw std::runtime_error(message);
}
} // namespace

int main() {
  using namespace Nexora;
  using namespace Nexora::Presentation;
  try {
    auto windows = Window::CreateWindowSystem();
    if (!windows) {
      std::cout << "SKIP: native window system unavailable\n";
      return 77;
    }
    const auto window =
        windows->Create({"Nexora Scene texture retirement", 320, 240, false, false});
    Require(bool(window), "native retirement window creation failed");
    Require(windows->Show(window.handle, true) == Window::WindowError::None, "show failed");
    SurfaceDescriptor descriptor{};
    descriptor.window = window.handle;
    descriptor.width = 320;
    descriptor.height = 240;
    auto surface = CreateSurface(descriptor, *windows);
    Require(bool(surface), "native retirement surface creation failed");
    const std::array<SceneVertex, 3> vertices{{{{-.8F, -.8F, .5F}, {0, 0, 1}, {0, 0}},
                                               {{.8F, -.8F, .5F}, {0, 0, 1}, {1, 0}},
                                               {{0, .8F, .5F}, {0, 0, 1}, {.5F, 1}}}};
    const std::array<std::uint16_t, 3> indices{0, 1, 2};
    const std::array<std::byte, 4> rgba{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}};
    const std::array<std::byte, 8> linear{std::byte{0}, std::byte{60}, std::byte{0}, std::byte{60},
                                          std::byte{0}, std::byte{60}, std::byte{0}, std::byte{60}};
    constexpr std::uint64_t retained = 47;
    std::array<SceneMaterial, 1> materials;
    materials[0].unlit = true;
    materials[0].emission = {1, 1, 1};
    materials[0].castsShadow = false;
    SceneDrawData draw{vertices, indices};
    draw.pbr = true;
    draw.materials = materials;
    draw.cameraPosition = {0, 0, 3};
    const std::array<UiVertex, 3> uiVertices{{{{0, 0}, {0, 0}, UINT32_MAX},
                                              {{10, 0}, {1, 0}, UINT32_MAX},
                                              {{0, 10}, {0, 1}, UINT32_MAX}}};
    const std::array<UiDrawCommand, 1> commands{{{0, 0, 320, 240, retained, 3}}};
    const std::array uiUpload{UiTextureUpload{retained, 1, 1, 4, rgba, true, true}};
    UiDrawData ui{uiVertices, std::as_bytes(std::span(indices)), commands, {}};
    for (std::uint64_t generation = 0; generation < 80; ++generation) {
      static_cast<void>(windows->PumpEvents());
      const std::uint64_t colorId = 1000 + generation * 2, linearId = colorId + 1;
      std::array uploads{UiTextureUpload{colorId, 1, 1, 4, rgba},
                         UiTextureUpload{retained, 1, 1, 4, rgba}};
      const std::array linearUploads{SceneLinearTextureUpload{linearId, 1, 1, 1, linear}};
      draw.textureUploads = std::span(uploads).first(generation == 0 ? 2 : 1);
      draw.linearTextureUploads = linearUploads;
      draw.environment = SceneEnvironment{linearId, linearId, linearId, 0, 0, 1};
      materials[0].textureId = materials[0].emissionTextureId = colorId;
      const std::array drops{colorId, colorId, linearId, UINT64_C(700000)};
      Require(surface->Acquire() == SurfaceStatus::Ready, "generation acquire failed");
      Require(surface->ReleaseSceneTextures(drops) == SurfaceStatus::InvalidDescriptor,
              "release accepted an acquired frame");
      Require(surface->DrawScene(draw) == SurfaceStatus::Ready, "generation upload/draw failed");
      ui.textureUploads =
          generation == 0 ? std::span(uiUpload) : std::span<const UiTextureUpload>{};
      Require(surface->RenderUi(ui) == SurfaceStatus::Ready &&
                  surface->Present() == SurfaceStatus::Ready,
              "generation GUI/present failed");
      if (generation == 0) {
        SurfaceStatus foreign{};
        std::thread other([&] { foreign = surface->ReleaseSceneTextures(drops); });
        other.join();
        Require(foreign == SurfaceStatus::WrongThread, "foreign release changed resources");
        const std::array invalid{retained, UINT64_MAX};
        Require(surface->ReleaseSceneTextures(invalid) == SurfaceStatus::InvalidDescriptor,
                "reserved texture release accepted");
        std::array<std::uint64_t, 65> oversized{};
        oversized.fill(retained);
        Require(surface->ReleaseSceneTextures(oversized) == SurfaceStatus::InvalidDescriptor,
                "unbounded release accepted");
      }
      Require(surface->ReleaseSceneTextures(drops) == SurfaceStatus::Ready &&
                  surface->ReleaseSceneTextures(drops) == SurfaceStatus::Ready,
              "generation retirement/idempotence failed");
      draw.textureUploads = {};
      draw.linearTextureUploads = {};
      draw.environment.reset();
      Require(surface->Acquire() == SurfaceStatus::Ready, "retained owner acquire failed");
      Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
              "retired color remained resident");
      materials[0].textureId = materials[0].emissionTextureId = retained;
      draw.environment = SceneEnvironment{linearId, linearId, linearId, 0, 0, 1};
      Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
              "retired linear environment remained resident");
      draw.environment.reset();
      Require(surface->DrawScene(draw) == SurfaceStatus::Ready,
              "another owner's Scene texture was removed");
      ui.textureUploads = {};
      Require(surface->RenderUi(ui) == SurfaceStatus::Ready &&
                  surface->Present() == SurfaceStatus::Ready,
              "Scene retirement changed the independent GUI cache");
    }
    const auto stats = surface->Diagnostics();
    Require(stats.presentedFrames == 160 && stats.sceneDrawCalls == 160 &&
                stats.sceneTextureUploads >= 160 && stats.nativeUiTextureUploads == 1 &&
                stats.nativeUiRejectedTextures == 0,
            "retirement native work counts differ");
    const std::array last{retained};
    Require(surface->ReleaseSceneTextures(last) == SurfaceStatus::Ready &&
                surface->DrainAndDestroy() == SurfaceStatus::Ready &&
                surface->ReleaseSceneTextures(last) == SurfaceStatus::SurfaceLost,
            "Scene retirement shutdown failed");
    Require(windows->Destroy(window.handle) == Window::WindowError::None, "window close failed");
    std::cout
        << "PASS:80 actual RGBA/linear Scene generations,160 frames; retained Scene/GUI "
           "owners, stale-reference rejection, atomic invalid admission and drained teardown\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
