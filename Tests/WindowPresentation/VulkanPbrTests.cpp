#include "Nexora/Presentation/Surface.h"
#include "PbrEnvironmentFixtures.h"
#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef None
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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
#if !defined(_WIN32)
unsigned Channel(unsigned long pixel, unsigned long mask) {
  if (!mask)
    return 0;
  while (!(mask & 1)) {
    mask >>= 1;
    pixel >>= 1;
  }
  return static_cast<unsigned>((pixel & mask) * 255 / mask);
}
#endif
using Rgb = std::array<unsigned, 3>;
std::array<Rgb, 4> Read(
#if defined(_WIN32)
    std::nullptr_t display, HWND window,
#else
    Display *display, ::Window window,
#endif
    unsigned width, unsigned height, const std::filesystem::path &capture) {
#if defined(_WIN32)
  (void)display;
  POINT origin{};
  Require(ClientToScreen(window, &origin), "PBR client origin unavailable");
  auto source = GetDC(nullptr);
  auto dc = source ? CreateCompatibleDC(source) : nullptr;
  auto bitmap =
      source ? CreateCompatibleBitmap(source, static_cast<int>(width), static_cast<int>(height))
             : nullptr;
  Require(source && dc && bitmap, "PBR GDI capture unavailable");
  auto previous = SelectObject(dc, bitmap);
  const auto copied = BitBlt(dc, 0, 0, static_cast<int>(width), static_cast<int>(height), source,
                             origin.x, origin.y, SRCCOPY);
  SelectObject(dc, previous);
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = static_cast<LONG>(width);
  info.bmiHeader.biHeight = -static_cast<LONG>(height);
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  std::vector<unsigned char> capturedBytes(static_cast<std::size_t>(width) * height * 4);
  const auto rows = GetDIBits(dc, bitmap, 0, height, capturedBytes.data(), &info, DIB_RGB_COLORS);
  DeleteObject(bitmap);
  DeleteDC(dc);
  ReleaseDC(nullptr, source);
  Require(copied && rows == static_cast<int>(height), "PBR GDI capture failed");
  const auto rgb = [&](unsigned x, unsigned y) {
    const auto index = (static_cast<std::size_t>(y) * width + x) * 4;
    return Rgb{capturedBytes[index + 2], capturedBytes[index + 1], capturedBytes[index]};
  };
#else
  XSync(display, False);
  auto *image = XGetImage(display, window, 0, 0, width, height, AllPlanes, ZPixmap);
  Require(image != nullptr, "PBR native image read failed");
  const auto rgb = [&](unsigned x, unsigned y) {
    const auto pixel = XGetPixel(image, x, y);
    return Rgb{Channel(pixel, image->red_mask), Channel(pixel, image->green_mask),
               Channel(pixel, image->blue_mask)};
  };
#endif
  const std::array result{rgb(width / 4, height / 2), rgb(width * 3 / 4, height / 2),
                          rgb(width / 2, height * 3 / 20), rgb(width / 2, height * 9 / 10)};
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
#if !defined(_WIN32)
  XDestroyImage(image);
#endif
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
#if defined(_WIN32)
    const std::nullptr_t display = nullptr;
    const auto native = reinterpret_cast<HWND>(windows->NativeHandle(window.handle));
    SetForegroundWindow(native);
#else
    auto *display = XOpenDisplay(nullptr);
    Require(display != nullptr, "PBR readback display unavailable");
    const auto native = reinterpret_cast<::Window>(windows->NativeHandle(window.handle));
#endif
    SurfaceDescriptor descriptor{};
    descriptor.window = window.handle;
    descriptor.width = 640;
    descriptor.height = 480;
#if defined(_WIN32)
    descriptor.backend = SurfaceBackend::Dx12;
#else
    descriptor.backend = SurfaceBackend::Vulkan;
#endif
    auto surface = CreateSurface(descriptor, *windows);
    Require(surface != nullptr, "PBR surface unavailable");
    std::array<SceneVertex, 9> vertices{};
    for (std::size_t i = 0; i < 6; ++i) {
      const auto corner = i % 3;
      vertices[i] = {{(i < 3 ? -0.5F : 0.5F) + (corner == 0   ? -0.3F
                                                : corner == 1 ? 0.3F
                                                              : 0),
                      corner == 2 ? 0.8F : -0.8F, 0.2F},
                     {0, 0, 1},
                     {0.5F, 0.5F},
                     {1, 0, 0, 1}};
    }
    vertices[6] = {{-0.1F, 0.6F, 0.2F}, {0, 0, 1}, {0.5F, 0.5F}, {1, 0, 0, 1}};
    vertices[7] = {{0.1F, 0.6F, 0.2F}, {0, 0, 1}, {0.5F, 0.5F}, {1, 0, 0, 1}};
    vertices[8] = {{0, 0.9F, 0.2F}, {0, 0, 1}, {0.5F, 0.5F}, {1, 0, 0, 1}};
    const std::array<std::uint16_t, 9> indices{0, 1, 2, 3, 4, 5, 6, 7, 8};
    const std::array batches{SceneMeshBatch{0, 3, 0, 1, 0}, SceneMeshBatch{3, 3, 0, 1, 1},
                             SceneMeshBatch{6, 3, 0, 1, 2}};
    std::array<SceneMaterial, 3> materials{};
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
    const std::array mirroredBatches{SceneMeshBatch{0, 3, 0, 1, 0}, SceneMeshBatch{3, 3, 1, 1, 1},
                                     SceneMeshBatch{6, 3, 1, 1, 2}};
    const std::array<std::byte, 8> blackWhite{std::byte{0},   std::byte{0},   std::byte{0},
                                              std::byte{255}, std::byte{255}, std::byte{255},
                                              std::byte{255}, std::byte{255}};
    const UiTextureUpload filterUpload{38, 2, 1, 8, blackWhite};
    unsigned width = 640, height = 480;
    Rgb uiBaseline{};
    for (unsigned frame = 0; frame < 24; ++frame) {
      materials = {};
      draw.pbr = true;
      draw.hdr = false;
      draw.exposure = 1;
      draw.instances = {};
      draw.batches = batches;
      draw.light_direction[1] = 0;
      draw.light_direction[2] = -1;
      draw.offscreen = frame % 2 == 0;
      draw.textureUploads = (frame == 0 || frame == 4) ? std::span<const UiTextureUpload>(uploads)
                                                       : std::span<const UiTextureUpload>{};
      const auto mode = frame % 4;
      draw.light_color[0] = draw.light_color[1] = draw.light_color[2] = mode == 0 ? 0.0F : 1.0F;
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
      if (frame >= 9) {
        materials = {};
        draw.light_color[0] = draw.light_color[1] = draw.light_color[2] = 1;
        if (frame == 9) {
          draw.textureUploads = {&filterUpload, 1};
          materials[0].baseColor = {0.5F, 0.5F, 0.5F, 1};
          materials[1].textureId = 38;
        } else if (frame == 10) {
          draw.light_color[0] = draw.light_color[1] = draw.light_color[2] = 0;
          materials[0].emission = {0.5F, 0.5F, 0.5F};
          materials[1].emission = {1, 1, 1};
          materials[1].emissionTextureId = 38;
        } else if (frame == 11) {
          // Legacy maps continue sampling UNORM: midpoint equals encoded gray 128.
          draw.pbr = false;
          materials[0].textureId = 38;
          materials[1].textureId = 34;
        } else {
          // ORM stays linear: midpoint gives AO, roughness and metallic of 0.5.
          materials[0].occlusion = materials[0].roughness = materials[0].metallic = 0.5F;
          materials[1].metallic = materials[1].roughness = 1;
          materials[1].ormTextureId = 38;
        }
      }
      if (frame >= 13 && frame < 20) {
        PbrEnvironmentFixtures::Configure(draw, materials, frame - 13);
        draw.linearTextureUploads =
            (frame == 13 || frame == 19)
                ? std::span<const SceneLinearTextureUpload>(PbrEnvironmentFixtures::uploads)
                : std::span<const SceneLinearTextureUpload>{};
      }
      if (frame >= 20) {
        draw.hdr = draw.offscreen = true;
        draw.exposure = frame == 21 ? 0.125F : frame == 22 ? 0.25F : 1.0F;
        draw.environment.reset();
        draw.linearTextureUploads = {};
        draw.light_color[0] = draw.light_color[1] = draw.light_color[2] = 0;
        materials = {};
        materials[0].baseColor = materials[1].baseColor = {0, 0, 0, 1};
        materials[0].emission = {4, 0, 0};
        materials[1].emission = {1, 0, 0};
      }
      // Require a unique marker from this submission before accepting asynchronously presented
      // X11 pixels. Equality alone could otherwise match the preceding frame's material pair.
      const float marker = 0.02F * static_cast<float>(frame + 1);
      materials[2].baseColor =
          draw.pbr ? std::array<float, 4>{0, 0, 0, 1} : std::array<float, 4>{marker, 0, 0, 1};
      materials[2].emission = {marker, 0, 0};
      materials[2].roughness = 1;
      materials[2].metallic =
          1; // Zero albedo metal has zero direct specular; marker is emission only.
      const auto toSrgb = [](float linear) {
        return linear <= 0.0031308F ? 12.92F * linear
                                    : 1.055F * std::pow(linear, 1.0F / 2.4F) - 0.055F;
      };
      const auto exposedMarker = marker * draw.exposure;
      const auto mappedMarker = exposedMarker * (2.51F * exposedMarker + 0.03F) /
                                (exposedMarker * (2.43F * exposedMarker + 0.59F) + 0.14F);
      const auto markerCode =
          static_cast<int>(std::lround(255.0F * (draw.pbr ? toSrgb(mappedMarker) : marker)));
      const auto srgbLegacyMarker = static_cast<int>(std::lround(255.0F * toSrgb(marker)));
      if (frame == 4 || frame == 19 || frame == 22) {
        width = frame == 4 || frame == 22 ? 480U : 640U;
        height = frame == 4 || frame == 22 ? 360U : 480U;
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
      if (frame == 13) {
        auto invalid = draw;
        invalid.environment->specularTextureId = 999;
        Require(surface->DrawScene(invalid) == SurfaceStatus::InvalidDescriptor,
                "missing environment accepted");
        invalid = draw;
        invalid.environment->specularMipLevels = 2;
        Require(surface->DrawScene(invalid) == SurfaceStatus::InvalidDescriptor,
                "mismatched environment mips accepted");
        invalid = draw;
        auto invalidUploads = PbrEnvironmentFixtures::uploads;
        invalidUploads[0].textureId = 34;
        invalid.environment->diffuseTextureId = 34;
        invalid.linearTextureUploads = invalidUploads;
        Require(surface->DrawScene(invalid) == SurfaceStatus::InvalidDescriptor,
                "linear/RGBA8 alias accepted");
      }
      if (frame == 20) {
        auto invalid = draw;
        invalid.offscreen = false;
        Require(surface->DrawScene(invalid) == SurfaceStatus::InvalidDescriptor,
                "direct HDR accepted");
        invalid = draw;
        invalid.exposure = std::numeric_limits<float>::infinity();
        Require(surface->DrawScene(invalid) == SurfaceStatus::InvalidDescriptor,
                "nonfinite exposure accepted");
      }
      const auto drawStatus = surface->DrawScene(draw);
      if (drawStatus != SurfaceStatus::Ready)
        std::cerr << "PBR draw frame=" << frame << " status=" << static_cast<int>(drawStatus)
                  << '\n';
      Require(drawStatus == SurfaceStatus::Ready, "shared PBR draw failed");
      if (draw.offscreen)
        Require(surface->CompositeScene() == SurfaceStatus::Ready, "PBR composite failed");
      if (frame >= 19) {
        const float y = static_cast<float>(height) * 0.85F;
        const std::array<UiVertex, 4> uiVertices{
            {{{0, y}, {0, 0}, 0xff808080U},
             {{static_cast<float>(width), y}, {0, 0}, 0xff808080U},
             {{static_cast<float>(width), static_cast<float>(height)}, {0, 0}, 0xff808080U},
             {{0, static_cast<float>(height)}, {0, 0}, 0xff808080U}}};
        const std::array<std::uint16_t, 6> uiIndices{0, 1, 2, 0, 2, 3};
        const std::array uiCommands{UiDrawCommand{0, 0, width, height, 901, 6, 0, 0}};
        const std::array<std::byte, 4> white{std::byte{255}, std::byte{255}, std::byte{255},
                                             std::byte{255}};
        const std::array uiUploads{UiTextureUpload{901, 1, 1, 4, white}};
        UiDrawData ui{};
        ui.vertices = uiVertices;
        ui.indices = std::as_bytes(std::span(uiIndices));
        ui.commands = uiCommands;
        ui.textureUploads = uiUploads;
        Require(surface->RenderUi(ui) == SurfaceStatus::Ready, "HDR UI failed");
      }
      Require(surface->Present() == SurfaceStatus::Ready, "PBR present failed");
      bool valid = false;
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
      while (!valid && std::chrono::steady_clock::now() < deadline) {
        const auto pixels = Read(display, native, width, height, {});
        const auto &left = pixels[0];
        const auto &right = pixels[1];
        if (frame >= 20) {
          const auto expected = [&](float value) {
            value *= draw.exposure;
            const float mapped = std::clamp(value * (2.51F * value + 0.03F) /
                                                (value * (2.43F * value + 0.59F) + 0.14F),
                                            0.0F, 1.0F);
            return static_cast<int>(std::lround(255 * toSrgb(mapped)));
          };
          valid = std::abs(static_cast<int>(left[0]) - expected(4)) <= 2 &&
                  std::abs(static_cast<int>(right[0]) - expected(1)) <= 2 && left[1] < 5 &&
                  right[1] < 5;
          for (std::size_t channel = 0; channel < 3; ++channel)
            valid = valid && std::abs(static_cast<int>(pixels[3][channel]) -
                                      static_cast<int>(uiBaseline[channel])) <= 2;
        } else if (frame >= 13)
          valid = PbrEnvironmentFixtures::Pixels(frame - 13, left, right);
        else if (frame >= 9)
          valid =
              left[0] > 40 && std::abs(static_cast<int>(left[0]) - static_cast<int>(right[0])) <= 2;
        else if (frame == 8)
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
        const auto observedMarker = static_cast<int>(pixels[2][0]);
        const bool currentFrame =
            (std::abs(observedMarker - markerCode) <= 2 ||
             (!draw.pbr && std::abs(observedMarker - srgbLegacyMarker) <= 2)) &&
            pixels[2][1] < 5 && pixels[2][2] < 5;
        valid = valid && currentFrame;
        if (valid && frame == 19)
          uiBaseline = pixels[3];
        if (!valid)
          std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
      if (!valid) {
        const auto values = Read(display, native, width, height, argc == 2 ? argv[1] : "");
        std::cerr << "PBR frame=" << frame << " left=" << values[0][0] << ',' << values[0][1] << ','
                  << values[0][2] << " right=" << values[1][0] << ',' << values[1][1] << ','
                  << values[1][2] << " marker=" << values[2][0] << " expected=" << markerCode
                  << " ui=" << values[3][0] << '\n';
      }
      Require(valid, "PBR emission/normal/ORM/sRGB native pixels mismatch");
      if (argc == 2 && frame == 9)
        static_cast<void>(Read(display, native, width, height, argv[1]));
    }
    Require(surface->Diagnostics().sceneDrawCalls == 24 &&
                surface->Diagnostics().sceneComposites == 14,
            "PBR counters mismatch");
    Require(surface->DrainAndDestroy() == SurfaceStatus::Ready, "PBR teardown failed");
    surface.reset();
#if !defined(_WIN32)
    XCloseDisplay(display);
#endif
    Require(windows->Destroy(window.handle) == Window::WindowError::None,
            "PBR window teardown failed");
    std::cout << "PASS: shared Slang PBR emission, normal, mirrored tangent handedness, ORM, "
                 "linear-space sRGB filtering, linear ORM/legacy filtering, missing-map "
                 "defaults, "
                 "invalid descriptors, HDR IBL radiance, roughness mips, metal/diffuse separation, "
                 "reflection rotation/view/seam, IBL disable, frame reuse, direct/offscreen draws "
                 "and resize pixels\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
