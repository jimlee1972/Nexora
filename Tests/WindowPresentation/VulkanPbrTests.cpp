#include "Nexora/Presentation/Surface.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef None
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace Nexora;
using namespace Presentation;
void Require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
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
using Rgb = std::array<unsigned, 3>;
std::array<Rgb, 2> Read(Display *display, ::Window window, unsigned width, unsigned height,
                        const std::filesystem::path &capture) {
  XSync(display, False);
  auto *image = XGetImage(display, window, 0, 0, width, height, AllPlanes, ZPixmap);
  Require(image != nullptr, "PBR native image read failed");
  const auto rgb = [&](unsigned x, unsigned y) {
    const auto pixel = XGetPixel(image, x, y);
    return Rgb{Channel(pixel, image->red_mask), Channel(pixel, image->green_mask),
               Channel(pixel, image->blue_mask)};
  };
  const std::array result{rgb(width / 4, height / 2), rgb(width * 3 / 4, height / 2)};
  if (!capture.empty()) {
    std::ofstream file(capture, std::ios::binary);
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (unsigned y = 0; y < height; ++y)
      for (unsigned x = 0; x < width; ++x) {
        const auto pixel = rgb(x, y);
        const std::array<char, 3> bytes{static_cast<char>(pixel[0]), static_cast<char>(pixel[1]),
                                        static_cast<char>(pixel[2])};
        file.write(bytes.data(), bytes.size());
      }
    Require(file.good(), "PBR capture write failed");
  }
  XDestroyImage(image);
  return result;
}
} // namespace

int main(int argc, char **argv) {
  try {
    auto windows = Window::CreateWindowSystem();
    Require(windows != nullptr, "PBR window system unavailable");
    const auto window = windows->Create({"Nexora shared PBR acceptance", 640, 480, true, false});
    Require(static_cast<bool>(window), "PBR window creation failed");
    Require(windows->Show(window.handle, true) == Window::WindowError::None, "PBR show failed");
    auto *display = XOpenDisplay(nullptr);
    Require(display != nullptr, "PBR readback display unavailable");
    const auto native = reinterpret_cast<::Window>(windows->NativeHandle(window.handle));
    SurfaceDescriptor descriptor{};
    descriptor.window = window.handle;
    descriptor.width = 640;
    descriptor.height = 480;
    descriptor.backend = SurfaceBackend::Vulkan;
    auto surface = CreateSurface(descriptor, *windows);
    Require(surface != nullptr, "PBR surface unavailable");
    std::array<SceneVertex, 6> vertices{};
    for (std::size_t i = 0; i < vertices.size(); ++i) {
      const auto corner = i % 3;
      vertices[i] = {{(i < 3 ? -0.5F : 0.5F) + (corner == 0   ? -0.3F
                                                : corner == 1 ? 0.3F
                                                              : 0),
                      corner == 2 ? 0.8F : -0.8F, 0.2F},
                     {0, 0, 1},
                     {0.5F, 0.5F},
                     {1, 0, 0, 1}};
    }
    const std::array<std::uint16_t, 6> indices{0, 1, 2, 3, 4, 5};
    const std::array batches{SceneMeshBatch{0, 3, 0, 1, 0}, SceneMeshBatch{3, 3, 0, 1, 1}};
    std::array<SceneMaterial, 2> materials{};
    SceneDrawData draw{};
    draw.vertices = vertices;
    draw.indices = indices;
    draw.materials = materials;
    draw.batches = batches;
    draw.pbr = true;
    draw.offscreen = true;
    draw.cameraPosition = {0, 0, 3};
    draw.light_direction[0] = draw.light_direction[1] = 0;
    draw.light_direction[2] = -1;
    draw.light_color[0] = draw.light_color[1] = draw.light_color[2] = 1;
    const std::array<std::byte, 4> red{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}};
    const std::array<std::byte, 4> green{std::byte{0}, std::byte{255}, std::byte{0},
                                         std::byte{255}};
    const std::array<std::byte, 4> sideNormal{std::byte{255}, std::byte{128}, std::byte{128},
                                              std::byte{255}};
    const std::array<std::byte, 4> gray{std::byte{128}, std::byte{128}, std::byte{128},
                                        std::byte{255}};
    const std::array<std::byte, 4> metalOrm{std::byte{255}, std::byte{128}, std::byte{255},
                                            std::byte{255}};
    const std::array<std::byte, 4> dielectricOrm{std::byte{255}, std::byte{128}, std::byte{0},
                                                 std::byte{255}};
    const std::array uploads{
        UiTextureUpload{31, 1, 1, 4, red},        UiTextureUpload{32, 1, 1, 4, green},
        UiTextureUpload{33, 1, 1, 4, sideNormal}, UiTextureUpload{34, 1, 1, 4, gray},
        UiTextureUpload{35, 1, 1, 4, metalOrm},   UiTextureUpload{36, 1, 1, 4, dielectricOrm}};
    const std::array<std::byte, 4> upNormal{std::byte{128}, std::byte{255}, std::byte{128},
                                            std::byte{255}};
    const UiTextureUpload upUpload{37, 1, 1, 4, upNormal};
    std::array<SceneInstance, 2> mirroredInstances{};
    const std::array mirroredBatches{SceneMeshBatch{0, 3, 0, 1, 0}, SceneMeshBatch{3, 3, 1, 1, 1}};
    unsigned width = 640, height = 480;
    for (unsigned frame = 0; frame < 9; ++frame) {
      materials = {};
      draw.offscreen = frame % 2 == 0;
      draw.textureUploads = (frame == 0 || frame == 4) ? std::span<const UiTextureUpload>(uploads)
                                                       : std::span<const UiTextureUpload>{};
      const auto mode = frame % 4;
      draw.light_color[0] = draw.light_color[1] = draw.light_color[2] = mode == 0 ? 0 : 1;
      if (mode == 0) {
        materials[0].baseColor = materials[1].baseColor = {0, 0, 0, 1};
        materials[0].emission = materials[1].emission = {1, 1, 1};
        materials[0].emissionTextureId = 31;
        materials[1].emissionTextureId = 32;
      } else if (mode == 1) {
        materials[0].normalTextureId = 33;
      } else if (mode == 2) {
        materials[0].metallic = materials[1].metallic = 1;
        materials[0].ormTextureId = 35;
        materials[1].ormTextureId = 36;
      } else {
        // Encoded sRGB 128 equals this linear factor; double or absent decoding breaks equality.
        const auto linear = static_cast<float>(std::pow((128.0 / 255.0 + 0.055) / 1.055, 2.4));
        materials[0].baseColor = {linear, linear, linear, 1};
        materials[1].textureId = 34;
      }
      if (frame == 8) {
        materials = {};
        materials[0].normalTextureId = materials[1].normalTextureId = 37;
        draw.textureUploads = {&upUpload, 1};
        draw.instances = mirroredInstances;
        draw.batches = mirroredBatches;
        mirroredInstances[0].model_transform =
            std::array<float, 16>{-1, 0, 0, -1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        draw.light_direction[1] = -1;
        draw.light_direction[2] = 0;
        draw.light_color[0] = draw.light_color[1] = draw.light_color[2] = 1;
      }
      if (frame == 4) {
        width = 480;
        height = 360;
        Require(windows->Resize(window.handle, width, height) == Window::WindowError::None,
                "PBR resize failed");
        Require(surface->NotifyWindowExtent(width, height) == SurfaceStatus::Ready,
                "PBR extent failed");
      }
      static_cast<void>(windows->PumpEvents());
      Require(surface->Acquire() == SurfaceStatus::Ready, "PBR acquire failed");
      if (frame == 0) {
        materials[0].normalTextureId = 987;
        Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
                "missing PBR map accepted");
        materials[0].normalTextureId = 0;
        materials[0].roughness = -1;
        Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
                "invalid roughness accepted");
        materials[0].roughness = 0.5F;
      }
      const auto drawStatus = surface->DrawScene(draw);
      if (drawStatus != SurfaceStatus::Ready)
        std::cerr << "PBR draw frame=" << frame << " status=" << static_cast<int>(drawStatus)
                  << '\n';
      Require(drawStatus == SurfaceStatus::Ready, "shared PBR draw failed");
      if (draw.offscreen)
        Require(surface->CompositeScene() == SurfaceStatus::Ready, "PBR composite failed");
      Require(surface->Present() == SurfaceStatus::Ready, "PBR present failed");
      bool valid = false;
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
      while (!valid && std::chrono::steady_clock::now() < deadline) {
        const auto pixels = Read(display, native, width, height, {});
        const auto &left = pixels[0];
        const auto &right = pixels[1];
        if (frame == 8)
          valid = left[0] > 100 && right[0] > 100 &&
                  std::abs(static_cast<int>(left[0]) - static_cast<int>(right[0])) <= 2;
        else if (mode == 0)
          valid = std::abs(static_cast<int>(left[0]) - 232) <= 2 && left[1] < 10 &&
                  std::abs(static_cast<int>(right[1]) - 232) <= 2 && right[0] < 10;
        else if (mode == 1)
          valid = left[0] < 30 && right[0] > 100;
        else if (mode == 2)
          valid = std::abs(static_cast<int>(left[0]) - static_cast<int>(right[0])) > 10;
        else
          valid =
              left[0] > 80 && std::abs(static_cast<int>(left[0]) - static_cast<int>(right[0])) <= 2;
        if (!valid)
          std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
      if (!valid) {
        const auto values = Read(display, native, width, height, argc == 2 ? argv[1] : "");
        std::cerr << "PBR frame=" << frame << " left=" << values[0][0] << ',' << values[0][1] << ','
                  << values[0][2] << " right=" << values[1][0] << ',' << values[1][1] << ','
                  << values[1][2] << '\n';
      }
      Require(valid, "PBR emission/normal/ORM/sRGB native pixels mismatch");
      if (argc == 2 && frame == 4)
        static_cast<void>(Read(display, native, width, height, argv[1]));
    }
    Require(surface->Diagnostics().sceneDrawCalls == 9 &&
                surface->Diagnostics().sceneComposites == 5,
            "PBR counters mismatch");
    Require(surface->DrainAndDestroy() == SurfaceStatus::Ready, "PBR teardown failed");
    surface.reset();
    XCloseDisplay(display);
    Require(windows->Destroy(window.handle) == Window::WindowError::None,
            "PBR window teardown failed");
    std::cout << "PASS: shared Slang PBR emission, normal, mirrored tangent handedness, ORM, "
                 "single sRGB decoding, missing-map "
                 "defaults, "
                 "invalid descriptors, frame reuse, direct/offscreen draws and resize pixels\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
