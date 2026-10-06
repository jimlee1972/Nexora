#include "Nexora/Presentation/Surface.h"
#include "PbrAntiAliasFixtures.h"
#include "PbrAtmosphereFixtures.h"
#include "PbrBloomFixtures.h"
#include "PbrEnvironmentFixtures.h"
#include "PbrMipFixtures.h"
#include "PbrPointLightFixtures.h"
#include "PbrReflectionFixtures.h"
#include "PbrRefractionFixtures.h"
#include "PbrShadowFixtures.h"
#include "PbrTransparencyFixtures.h"
#include "PbrTwoSidedFixtures.h"
#include "PbrVegetationFixtures.h"
#include "PbrWorldMappingFixtures.h"
#include "PbrWorldNormalFixtures.h"
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
std::array<Rgb, 9> Read(
#if defined(_WIN32)
    std::nullptr_t display, HWND window,
#else
    Display *display, ::Window window,
#endif
    unsigned width, unsigned height, const std::filesystem::path &capture,
    std::uint64_t *sceneHash = nullptr, unsigned *intermediate = nullptr) {
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
  const std::array result{rgb(width / 4, height / 2),         rgb(width * 3 / 4, height / 2),
                          rgb(width / 2, height * 3 / 20),    rgb(width / 2, height * 9 / 10),
                          rgb(width * 129 / 400, height / 2), rgb(width * 58 / 100, height / 2),
                          rgb(width / 2, height / 2),         rgb(width * 49 / 100, height / 2),
                          rgb(width * 51 / 100, height / 2)};
  if (sceneHash) {
    *sceneHash = 14695981039346656037ULL;
    for (unsigned y = height * 3 / 10; y < height * 7 / 10; ++y)
      for (unsigned x = width * 3 / 10; x < width * 7 / 10; ++x)
        for (const auto channel : rgb(x, y)) {
          *sceneHash ^= channel;
          *sceneHash *= 1099511628211ULL;
        }
  }
  if (intermediate)
    *intermediate = PbrAntiAliasFixtures::Intermediate(width, height, rgb);
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
    std::array<SceneMaterial, 4> materials{};
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
    Rgb uiBaseline{}, reflectedReference{}, blendedReference{}, mappedReference{}, fogReference{},
        flatNormalLeft{}, flatNormalRight{}, tiltedNormalLeft{}, tiltedNormalRight{},
        refractedLeft{}, refractedRight{}, mipLeft{}, mipRight{};
    std::uint64_t windReference{}, windMoved{};
    std::array<unsigned, 3> pointLeft{}, pointRight{};
    unsigned aaBaseline = 0;
    std::uint64_t aaHash = 0;
    for (unsigned frame = 0; frame < 93; ++frame) {
      PbrShadowFixtures::Fixture shadowFixture(frame >= 24 ? frame - 24 : 0);
      PbrBloomFixtures::Fixture bloomFixture;
      PbrReflectionFixtures::Fixture reflectionFixture(frame >= 45 ? frame - 45 : 0);
      PbrVegetationFixtures::Fixture vegetationFixture(frame >= 34 ? frame - 34 : 0);
      PbrTransparencyFixtures::Fixture transparencyFixture(frame >= 49 ? frame - 49 : 0);
      PbrWorldMappingFixtures::Fixture mappingFixture(frame >= 55 ? frame - 55 : 0);
      PbrAtmosphereFixtures::Fixture atmosphereFixture(frame >= 59 ? frame - 59 : 0);
      PbrWorldNormalFixtures::Fixture worldNormalFixture(frame >= 65 ? frame - 65 : 0);
      PbrRefractionFixtures::Fixture refractionFixture(frame >= 77   ? frame - 71
                                                       : frame >= 69 ? frame - 69
                                                                     : 0);
      PbrMipFixtures::Fixture mipFixture(frame >= 75 ? frame - 75 : 0);
      PbrPointLightFixtures::Fixture pointFixture(frame >= 79 ? frame - 79 : 0);
      PbrTwoSidedFixtures::Fixture twoSidedFixture(frame >= 85 ? frame - 85 : 0);
      PbrAntiAliasFixtures::Fixture aaFixture(frame >= 89 ? frame - 89 : 0);
      materials = {};
      draw.shadow.reset();
      draw.lightingStyle.reset();
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
        draw.shadow.reset();
        draw.lightingStyle.reset();
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
        draw.shadow.reset();
        draw.lightingStyle.reset();
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
        draw.shadow.reset();
        draw.lightingStyle.reset();
        materials[0].baseColor = materials[1].baseColor = {0, 0, 0, 1};
        materials[0].emission = {4, 0, 0};
        materials[1].emission = {1, 0, 0};
      }
      // Require a unique marker from this submission before accepting asynchronously presented
      // X11 pixels. Equality alone could otherwise match the preceding frame's material pair.
      if (frame >= 24 && frame < 30) {
        draw = shadowFixture.Draw(frame - 24);
        materials = shadowFixture.materials;
        draw.materials = materials;
      }
      const float marker = frame >= 34 ? 0.08F + 0.12F * static_cast<float>(frame % 4)
                                       : 0.02F * static_cast<float>(frame + 1);
      materials[2].baseColor =
          draw.pbr ? std::array<float, 4>{0, 0, 0, 1} : std::array<float, 4>{marker, 0, 0, 1};
      if (frame >= 30 && frame < 34) {
        draw = bloomFixture.Draw(frame - 30);
        materials = bloomFixture.geometry.materials;
        draw.materials = materials;
      }
      if (frame >= 34) {
        draw = vegetationFixture.Draw(frame - 34);
        materials = vegetationFixture.geometry.materials;
        draw.materials = materials;
      }
      if (frame >= 43 && frame < 45) {
        draw = bloomFixture.Draw(frame == 44 ? 4 : 0);
        materials = bloomFixture.geometry.materials;
        draw.materials = materials;
      }
      if (frame >= 45) {
        draw = reflectionFixture.Draw(frame - 45);
        materials = reflectionFixture.materials;
        draw.materials = materials;
      }
      if (frame >= 49) {
        draw = transparencyFixture.Draw(frame - 49);
        materials = transparencyFixture.geometry.materials;
        draw.materials = materials;
      }
      if (frame >= 55) {
        draw = mappingFixture.Draw(frame - 55);
        materials = mappingFixture.geometry.materials;
        draw.materials = materials;
      }
      if (frame >= 59) {
        draw = atmosphereFixture.Draw(frame - 59);
        materials = atmosphereFixture.geometry.materials;
        draw.materials = materials;
      }
      if (frame >= 65) {
        draw = worldNormalFixture.Draw();
        materials = worldNormalFixture.geometry.geometry.materials;
        draw.materials = materials;
      }
      if (frame >= 69) {
        draw = refractionFixture.Draw();
        materials = refractionFixture.materials;
        draw.materials = materials;
      }
      if (frame >= 75 && frame < 77) {
        draw = mipFixture.Draw();
        materials = mipFixture.geometry.geometry.materials;
        draw.materials = materials;
      }
      if (frame >= 77) {
        draw = refractionFixture.Draw();
        materials = refractionFixture.materials;
        draw.materials = materials;
      }
      if (frame >= 79) {
        draw = pointFixture.Draw();
        materials = pointFixture.materials;
        draw.materials = materials;
      }
      if (frame >= 85) {
        draw = twoSidedFixture.Draw();
        materials = twoSidedFixture.geometry.materials;
        draw.materials = materials;
      }
      if (frame >= 89) {
        draw = aaFixture.Draw();
        materials = aaFixture.materials;
        draw.materials = materials;
      }
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
#if defined(_WIN32)
      const auto legacyMarker = marker * 1.15F; // Existing DX12 Lambert ambient + direct light.
#else
      const auto legacyMarker =
          marker; // Existing Vulkan Lambert ambient/direct weights sum to one.
#endif
      const auto markerCode =
          static_cast<int>(std::lround(255.0F * (draw.pbr ? toSrgb(mappedMarker) : legacyMarker)));
      const auto srgbLegacyMarker = static_cast<int>(std::lround(255.0F * toSrgb(legacyMarker)));
      if (frame == 4 || frame == 19 || frame == 22 || frame == 27) {
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
      if (frame == 24) {
        auto invalid = draw;
        invalid.shadow->resolution = 300;
        Require(surface->DrawScene(invalid) == SurfaceStatus::InvalidDescriptor,
                "unbounded shadow resolution accepted");
        invalid = draw;
        invalid.shadow->normalBias = std::numeric_limits<float>::quiet_NaN();
        Require(surface->DrawScene(invalid) == SurfaceStatus::InvalidDescriptor,
                "NaN shadow bias accepted");
      }
      if (frame == 30) {
        auto invalid = draw;
        invalid.bloom = SceneBloom{};
        invalid.bloom->radiusPixels = 0;
        Require(surface->DrawScene(invalid) == SurfaceStatus::InvalidDescriptor,
                "zero bloom radius accepted");
        invalid.bloom->radiusPixels = 12;
        invalid.bloom->intensity = std::numeric_limits<float>::quiet_NaN();
        Require(surface->DrawScene(invalid) == SurfaceStatus::InvalidDescriptor,
                "NaN bloom intensity accepted");
        invalid = draw;
        invalid.colorGrade = SceneColorGrade{0, std::numeric_limits<float>::quiet_NaN()};
        Require(surface->DrawScene(invalid) == SurfaceStatus::InvalidDescriptor,
                "NaN color grade accepted");
      }
      if (frame == 34) {
        auto invalid = draw;
        invalid.vegetationTime = std::numeric_limits<float>::quiet_NaN();
        Require(surface->DrawScene(invalid) == SurfaceStatus::InvalidDescriptor,
                "NaN vegetation time accepted");
        materials[1].alphaCutoff = 1.1F;
        Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
                "invalid alpha cutoff accepted");
        materials[1].alphaCutoff = 0;
        materials[1].windAmplitude = -0.1F;
        Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
                "negative wind amplitude accepted");
        materials[1].windAmplitude = 0;
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
        std::vector<UiVertex> uiVertices{
            {{{0, y}, {0, 0}, 0xff808080U},
             {{static_cast<float>(width), y}, {0, 0}, 0xff808080U},
             {{static_cast<float>(width), static_cast<float>(height)}, {0, 0}, 0xff808080U},
             {{0, static_cast<float>(height)}, {0, 0}, 0xff808080U}}};
        std::vector<std::uint16_t> uiIndices{0, 1, 2, 0, 2, 3};
        if (frame >= 43) {
          // Focus intentionally blurs the scene marker. A unique post-composite UI marker
          // identifies these frames without assuming that an HDR pixel stays sharp.
          const auto color = 0xff000000U | static_cast<std::uint32_t>(std::lround(marker * 255));
          for (const auto point : {std::array{0.48F, 0.13F}, std::array{0.52F, 0.13F},
                                   std::array{0.52F, 0.17F}, std::array{0.48F, 0.17F}})
            uiVertices.push_back({{point[0] * width, point[1] * height}, {0, 0}, color});
          for (const auto index : {4, 5, 6, 4, 6, 7})
            uiIndices.push_back(static_cast<std::uint16_t>(index));
        }
        const std::array uiCommands{UiDrawCommand{
            0, 0, width, height, 901, static_cast<std::uint32_t>(uiIndices.size()), 0, 0}};
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
        std::uint64_t region{};
        unsigned intermediate = 0;
        const auto pixels = Read(display, native, width, height, {}, &region,
                                 frame >= 89 ? &intermediate : nullptr);
        const auto &left = pixels[0];
        const auto &right = pixels[1];
        if (frame >= 89) {
          if (frame == 89) {
            aaBaseline = intermediate;
            aaHash = region;
            valid = true;
          } else if (frame == 90)
            valid = intermediate >= aaBaseline + width / 16;
          else if (frame == 91)
            valid = intermediate == aaBaseline && region == aaHash;
          else
            valid = intermediate == 0 && left[0] >= 245 && right[0] >= 245;
        } else if (frame >= 85) {
          valid = PbrTwoSidedFixtures::Pixels(frame - 85, left, right);
          if (valid && frame == 85) {
            pointLeft = left;
            pointRight = right;
          }
          if (frame >= 87)
            valid = valid && left == pointLeft && right == pointRight;
        } else if (frame >= 79) {
          valid = PbrPointLightFixtures::Pixels(frame - 79, left, right);
          if (valid && frame == 80) {
            pointLeft = left;
            pointRight = right;
          }
          if (frame == 82)
            valid = valid && left == pointLeft && right == pointRight;
        } else if (frame >= 77) {
          valid = PbrRefractionFixtures::Pixels(frame - 71, pixels[7], pixels[8]);
        } else if (frame >= 75) {
          valid = PbrMipFixtures::Pixels(left, right);
          if (valid && frame == 75) {
            mipLeft = left;
            mipRight = right;
          }
          if (frame == 76)
            for (unsigned c = 0; c < 3; ++c)
              valid = valid &&
                      std::abs(static_cast<int>(left[c]) - static_cast<int>(mipLeft[c])) <= 2 &&
                      std::abs(static_cast<int>(right[c]) - static_cast<int>(mipRight[c])) <= 2;
        } else if (frame >= 69) {
          valid = PbrRefractionFixtures::Pixels(frame - 69, pixels[7], pixels[8]);
          if (valid && frame == 70) {
            refractedLeft = pixels[7];
            refractedRight = pixels[8];
          }
          if (frame == 72)
            valid = valid && pixels[7] == refractedLeft && pixels[8] == refractedRight;
        } else if (frame >= 65) {
          valid = PbrWorldNormalFixtures::Pixels(left, right);
          if (valid && frame == 65) {
            flatNormalLeft = left;
            flatNormalRight = right;
          }
          if (frame == 66) {
            valid = valid && left[1] > flatNormalLeft[1] + 6 && right[0] > flatNormalRight[0] + 6;
            if (valid) {
              tiltedNormalLeft = left;
              tiltedNormalRight = right;
            }
          }
          if (frame == 67)
            valid =
                valid &&
                std::abs(static_cast<int>(left[1]) - static_cast<int>(flatNormalLeft[1])) <= 2 &&
                std::abs(static_cast<int>(right[0]) - static_cast<int>(flatNormalRight[0])) <= 2;
          if (frame == 68)
            valid = valid && left == tiltedNormalLeft && right == tiltedNormalRight;
        } else if (frame >= 59) {
          valid = PbrAtmosphereFixtures::Pixels(frame - 59, left, pixels[6]);
          if (valid && frame == 60)
            fogReference = pixels[6];
          if (frame == 62)
            valid = valid && pixels[6] == fogReference;
        } else if (frame >= 55) {
          valid = PbrWorldMappingFixtures::Pixels(frame - 55, left, right);
          if (valid && frame == 56)
            mappedReference = left;
          if (frame == 58)
            valid = valid && left == mappedReference;
        } else if (frame >= 49) {
          valid = PbrTransparencyFixtures::Pixels(frame - 49, left, pixels[6]);
          if (valid && frame == 50)
            blendedReference = pixels[6];
          if (frame == 52)
            valid = valid && pixels[6] == blendedReference;
        } else if (frame >= 45) {
          valid = PbrReflectionFixtures::Pixels(frame - 45, left, right);
          if (valid && frame == 46)
            reflectedReference = left;
          if (frame == 48)
            valid = valid && left == reflectedReference;
        } else if (frame >= 43) {
          valid = PbrBloomFixtures::Pixels(frame == 44 ? 4 : 0, pixels[5], pixels[6]);
          for (std::size_t channel = 0; channel < 3; ++channel)
            valid = valid && std::abs(static_cast<int>(pixels[3][channel]) -
                                      static_cast<int>(uiBaseline[channel])) <= 2;
        } else if (frame >= 34) {
          valid = PbrVegetationFixtures::Pixels(frame - 34, pixels[0], pixels[6]);
          if (frame == 37)
            windReference = region;
          if (frame == 38) {
            windMoved = region;
            valid = valid && windMoved != windReference;
          }
          if (frame == 39)
            valid = valid && region == windReference && region != windMoved;
        } else if (frame >= 30) {
          valid = PbrBloomFixtures::Pixels(frame - 30, pixels[5], pixels[6]);
          for (std::size_t channel = 0; channel < 3; ++channel)
            valid = valid && std::abs(static_cast<int>(pixels[3][channel]) -
                                      static_cast<int>(uiBaseline[channel])) <= 2;
        } else if (frame >= 24) {
          valid = PbrShadowFixtures::Pixels(frame - 24, left, right);
          if (frame == 24)
            valid = valid && pixels[4][0] > 20 && pixels[4][0] + 10 < right[0];
        } else if (frame >= 20) {
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
        const auto uiMarker = static_cast<float>(std::lround(marker * 255)) / 255;
#if defined(_WIN32)
        // DX12 UI submits encoded byte colors directly to its UNORM swapchain.
        const auto uiMarkerCode = static_cast<int>(std::lround(255 * uiMarker));
#else
        // Vulkan's sRGB swapchain performs the UI output transfer in hardware.
        const auto uiMarkerCode = static_cast<int>(std::lround(255 * toSrgb(uiMarker)));
#endif
        const auto expectedPhaseMarker = frame >= 43 ? uiMarkerCode : markerCode;
        const bool currentFrame =
            (std::abs(observedMarker - expectedPhaseMarker) <= 2 ||
             (!draw.pbr && std::abs(observedMarker - srgbLegacyMarker) <= 2)) &&
            pixels[2][1] < 5 && pixels[2][2] < 5;
        if (frame == 33) {
          const auto grayMarkerCode =
              static_cast<int>(std::lround(255 * toSrgb(mappedMarker * 0.2126F)));
          valid = valid && std::abs(observedMarker - grayMarkerCode) <= 2 &&
                  std::abs(static_cast<int>(pixels[2][1]) - grayMarkerCode) <= 2 &&
                  std::abs(static_cast<int>(pixels[2][2]) - grayMarkerCode) <= 2;
        } else
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
                  << " ui=" << values[3][0] << " pcf=" << values[4][0] << " halo=" << values[5][0]
                  << " center=" << values[6][0] << '\n';
      }
      Require(valid, "PBR emission/normal/ORM/sRGB native pixels mismatch");
      if (argc == 2 && frame == 9)
        static_cast<void>(Read(display, native, width, height, argv[1]));
      if (argc == 2 && frame >= 30) {
        auto capture = std::filesystem::path(argv[1]);
        capture.replace_filename((frame >= 89   ? "anti-alias-"
                                  : frame >= 85 ? "two-sided-"
                                  : frame >= 79 ? "point-light-"
                                  : frame >= 77 ? "refraction-front-"
                                  : frame >= 75 ? "mip-filter-"
                                  : frame >= 69 ? "refraction-"
                                  : frame >= 65 ? "world-normal-"
                                  : frame >= 59 ? "atmosphere-"
                                  : frame >= 55 ? "world-mapping-"
                                  : frame >= 49 ? "transparency-"
                                  : frame >= 45 ? "planar-reflection-"
                                  : frame >= 43 ? "depth-of-field-"
                                  : frame < 34  ? "bloom-"
                                                : "vegetation-") +
                                 std::to_string(frame >= 89   ? frame - 89
                                                : frame >= 85 ? frame - 85
                                                : frame >= 79 ? frame - 79
                                                : frame >= 77 ? frame - 77
                                                : frame >= 75 ? frame - 75
                                                : frame >= 69 ? frame - 69
                                                : frame >= 65 ? frame - 65
                                                : frame >= 59 ? frame - 59
                                                : frame >= 55 ? frame - 55
                                                : frame >= 49 ? frame - 49
                                                : frame >= 45 ? frame - 45
                                                : frame >= 43 ? frame - 43
                                                : frame < 34  ? frame - 30
                                                              : frame - 34) +
                                 ".ppm");
        static_cast<void>(Read(display, native, width, height, capture));
      }
    }
    Require(surface->Diagnostics().sceneDrawCalls == 93 &&
                surface->Diagnostics().sceneComposites == 83 &&
                surface->Diagnostics().sceneShadowPasses == 11 &&
                surface->Diagnostics().sceneShadowInstances == 32,
            "PBR counters mismatch");
    Require(surface->DrainAndDestroy() == SurfaceStatus::Ready, "PBR teardown failed");
    surface.reset();
#if !defined(_WIN32)
    XCloseDisplay(display);
#endif
    Require(windows->Destroy(window.handle) == Window::WindowError::None,
            "PBR window teardown failed");
    std::cout
        << "PASS: shared Slang PBR emission, normal, mirrored tangent handedness, ORM, "
           "linear-space sRGB filtering, linear ORM/legacy filtering, missing-map "
           "defaults, "
           "invalid descriptors, HDR IBL radiance, roughness mips, metal/diffuse separation, "
           "reflection rotation/view/seam, IBL disable, frame reuse, direct/offscreen draws "
           "and resize pixels; directional shadow movement, XY projection, PCF edge, map reuse, "
           "shadow disable, stylized tint and thresholded HDR bloom, depth-aware focus and UI "
           "invariance, linear HDR translucent/tinted blending, bounded planar mirrors with source "
           "movement and restoration, bounded opaque-HDR refraction, color-correct mip "
           "minification, world-projected maps/normals, "
           "linear HDR atmosphere and "
           "unlit "
           "exclusion, alpha "
           "cutout/shadow agreement, GPU wind/replay, leaf transmission and bounded HDR "
           "anti-aliasing/restoration\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
