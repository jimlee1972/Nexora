#define NEXORA_METAL_SCENE_TESTING 1
// Compile the real private adapter into this test to read back GPU output without adding public
// handles.
#include "../../Engine/Presentation/src/MetalSurface.mm"
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
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>

#include <array>
#include <fstream>
#include <iostream>
#include <limits>

using namespace Nexora;
using namespace Nexora::Presentation;

int main(int argc, char **argv) {
  const auto fail = [](int line) {
    std::cerr << "Metal scene contract failed at line " << line << '\n';
    return 1;
  };
  @autoreleasepool {
    if (!MTLCreateSystemDefaultDevice() || NSScreen.screens.count == 0) {
      std::cout << "UNSUPPORTED: Metal device or WindowServer display unavailable\n";
      return 77;
    }
    auto windows = Window::CreateWindowSystem();
    const auto window = windows->Create({"Nexora Metal scene contracts", 640, 360, true, true});
    if (!window)
      return fail(__LINE__);
    // Synthetic Cocoa events verify native physical-key and top-left pixel pointer translation.
    auto *nativeWindow = (__bridge NSWindow *)windows->NativeHandle(window.handle);
    // Initialize AppKit's event queue before posting synthetic events; its first dequeue can
    // perform launch processing and discard events queued before that initialization.
    static_cast<void>(windows->PumpEvents());
    auto *keyEvent = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                      location:NSMakePoint(0, 0)
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:nativeWindow.windowNumber
                                       context:nil
                                    characters:@"1"
                   charactersIgnoringModifiers:@"1"
                                     isARepeat:NO
                                       keyCode:18];
    [NSApp postEvent:keyEvent atStart:NO];
    auto *mouseEvent = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
                                          location:NSMakePoint(20, 20)
                                     modifierFlags:0
                                         timestamp:0
                                      windowNumber:nativeWindow.windowNumber
                                           context:nil
                                       eventNumber:1
                                        clickCount:1
                                          pressure:1];
    [NSApp postEvent:mouseEvent atStart:NO];
    bool keySeen = false, pointerSeen = false, buttonSeen = false;
    for (const auto &event : windows->PumpEvents()) {
      if (event.type == Window::WindowEventType::Key)
        keySeen |=
            event.value0 == static_cast<std::int32_t>(Window::Key::Digit1) && event.value1 == 1;
      if (event.type == Window::WindowEventType::Pointer)
        pointerSeen |=
            event.value0 == static_cast<std::int32_t>(20 * nativeWindow.backingScaleFactor) &&
            event.value1 > 0;
      if (event.type == Window::WindowEventType::PointerButton)
        buttonSeen |= event.value0 == 0 && event.value1 == 1;
    }
    if (!keySeen || !pointerSeen || !buttonSeen) {
      std::cerr << "Cocoa input: key=" << keySeen << " pointer=" << pointerSeen
                << " button=" << buttonSeen << " window=" << nativeWindow.windowNumber
                << " keyWindow=" << keyEvent.window.windowNumber
                << " mouseWindow=" << mouseEvent.window.windowNumber << '\n';
      return fail(__LINE__);
    }
    auto surface = std::make_unique<MetalSurface>(
        SurfaceDescriptor{window.handle, 640, 360, 2, PresentMode::VSync, ColorSpace::Srgb,
                          SurfaceBackend::Metal},
        *windows);
    const auto identity = surface->Diagnostics().device;
    if (!identity.name[0] || identity.name.back() != '\0' || identity.deviceIdsAvailable ||
        identity.driverVersionFormat != DriverVersionFormat::Unavailable)
      return fail(__LINE__);
    const auto require = [](SurfaceStatus actual, SurfaceStatus expected) {
      if (actual != expected) {
        std::cerr << "Unexpected surface status: " << static_cast<int>(actual) << " expected "
                  << static_cast<int>(expected) << '\n';
        return false;
      }
      return true;
    };
    if (!surface || !require(surface->Acquire(), SurfaceStatus::Ready))
      return fail(__LINE__);
    const std::array<SceneVertex, 3> vertices{{{{-0.5F, -0.5F, 0.5F}, {0, 0, 1}, {0, 0}},
                                               {{0.5F, -0.5F, 0.5F}, {0, 0, 1}, {1, 0}},
                                               {{0, 0.5F, 0.5F}, {0, 0, 1}, {0.5F, 1}}}};
    const std::array<std::uint16_t, 3> indices{0, 1, 2};
    std::array<SceneInstance, 2> instances{};
    instances[0].translation[0] = -0.4F;
    instances[1].translation[0] = 0.4F;
    instances[1].color[1] = 0.25F;
    const std::array<std::byte, 8> pixels{std::byte{255}, std::byte{0},  std::byte{0},
                                          std::byte{255}, std::byte{0},  std::byte{255},
                                          std::byte{0},   std::byte{255}};
    const UiTextureUpload upload{1, 2, 1, 8, pixels};
    SceneDrawData draw{vertices, indices, instances, 1, {&upload, 1}, true};
    const std::array<SceneMeshBatch, 2> batches{{{0, 3, 0, 1}, {0, 3, 1, 1}}};
    draw.batches = batches;
    if (!require(surface->Acquire(), SurfaceStatus::InvalidDescriptor) ||
        !require(surface->CompositeScene(), SurfaceStatus::InvalidDescriptor))
      return fail(__LINE__);
    auto malformed = draw;
    malformed.model_view_projection[0] = std::numeric_limits<float>::quiet_NaN();
    if (!require(surface->DrawScene(malformed), SurfaceStatus::InvalidDescriptor))
      return fail(__LINE__);
    instances[0].scale[0] = 0;
    if (!require(surface->DrawScene(draw), SurfaceStatus::InvalidDescriptor))
      return fail(__LINE__);
    instances[0].scale[0] = 1;
    if (!require(surface->DrawScene(draw), SurfaceStatus::Ready) ||
        !require(surface->DrawScene(draw), SurfaceStatus::InvalidDescriptor) ||
        !require(surface->Present(), SurfaceStatus::InvalidDescriptor) ||
        !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
        !require(surface->CompositeScene(), SurfaceStatus::InvalidDescriptor))
      return fail(__LINE__);
    const std::array<UiVertex, 3> ui{{{{0, 0}, {0, 0}, 0xffffffff},
                                      {{30, 0}, {1, 0}, 0xffffffff},
                                      {{0, 30}, {0, 1}, 0xffffffff}}};
    const UiDrawCommand command{0, 0, 640, 360, 1, 3, 0, 0};
    const UiDrawData uiDraw{
        ui, std::as_bytes(std::span(indices)), {&command, 1}, {&upload, 1}, false};
    if (!require(surface->RenderUi(uiDraw), SurfaceStatus::Ready) ||
        !require(surface->RenderUi(uiDraw), SurfaceStatus::InvalidDescriptor) ||
        !require(surface->Present(), SurfaceStatus::Ready))
      return fail(__LINE__);
    // Reuse immutable scene textures and each fence-protected frame slot; GPU failures must
    // surface.
    for (int i = 0; i < 6; ++i) {
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(draw), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
    }
    const auto captured = surface->ReadScenePixelsForTesting();
    if (captured.size() != 640U * 360U * 4U)
      return fail(__LINE__);
    const auto channel = [&](std::size_t x, std::size_t y, std::size_t c) {
      return std::to_integer<int>(captured[(y * 640 + x) * 4 + c]);
    };
    // BGRA readback: both instances select the original red/green texture, while clear stays dark
    // blue.
    if (channel(130, 200, 2) <= channel(130, 200, 1) + 10 ||
        channel(230, 200, 1) <= channel(230, 200, 2) + 10 ||
        channel(10, 10, 0) <= channel(10, 10, 2)) {
      std::cerr << "Metal instance/texture/clear pixels mismatch\n";
      return fail(__LINE__);
    }
    if (surface->Diagnostics().sceneTextureUploads != 1)
      return fail(__LINE__);
    if (argc > 1) {
      std::ofstream image(argv[1], std::ios::binary);
      image << "P6\n640 360\n255\n";
      for (std::size_t i = 0; i < captured.size(); i += 4) {
        const char rgb[]{static_cast<char>(captured[i + 2]), static_cast<char>(captured[i + 1]),
                         static_cast<char>(captured[i])};
        image.write(rgb, 3);
      }
      if (!image)
        return fail(__LINE__);
    }
    // Material slots override the legacy texture/color independently per batch.
    auto materialDraw = draw;
    materialDraw.light_direction[0] = materialDraw.light_direction[1] = 0;
    materialDraw.light_direction[2] = -1;
    materialDraw.light_color[0] = materialDraw.light_color[1] = materialDraw.light_color[2] = 1;
    std::array<SceneMaterial, 2> materialSlots{{{{0.8F, 0, 0, 1}, 0}, {{0, 0.8F, 0, 1}, 0}}};
    std::array materialRanges{SceneMeshBatch{0, 3, 0, 1, 0}, SceneMeshBatch{0, 3, 1, 1, 1}};
    materialDraw.materials = materialSlots;
    materialDraw.batches = materialRanges;
    auto materialInstances = instances;
    materialInstances[1].color[1] = 1;
    materialDraw.instances = materialInstances;
    const std::array<std::byte, 4> solidRed{std::byte{255}, std::byte{0}, std::byte{0},
                                            std::byte{255}};
    const std::array<std::byte, 4> solidGreen{std::byte{0}, std::byte{255}, std::byte{0},
                                              std::byte{255}};
    const std::array materialUploads{UiTextureUpload{21, 1, 1, 4, solidRed},
                                     UiTextureUpload{22, 1, 1, 4, solidGreen}};
    for (int frame = 0; frame < 5; ++frame) {
      if (frame == 1) {
        materialSlots[0] = {{0.8F, 0.8F, 0.8F, 1}, 21};
        materialSlots[1] = {{0.8F, 0.8F, 0.8F, 1}, 22};
        materialDraw.textureUploads = materialUploads;
      } else if (frame > 1) {
        materialDraw.textureUploads = {};
      }
      if (!require(surface->Acquire(), SurfaceStatus::Ready))
        return fail(__LINE__);
      if (frame == 0) {
        materialRanges[1].materialIndex = 2;
        if (!require(surface->DrawScene(materialDraw), SurfaceStatus::InvalidDescriptor))
          return fail(__LINE__);
        materialRanges[1].materialIndex = 1;
        materialSlots[1].textureId = 987;
        if (!require(surface->DrawScene(materialDraw), SurfaceStatus::InvalidDescriptor))
          return fail(__LINE__);
        materialSlots[1].textureId = 0;
      }
      if (!require(surface->DrawScene(materialDraw), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto materialPixels = surface->ReadScenePixelsForTesting();
      if (materialPixels.size() != captured.size())
        return fail(__LINE__);
      const auto left = (200 * 640 + 190) * 4;
      const auto right = (200 * 640 + 450) * 4;
      if (std::to_integer<int>(materialPixels[left + 2]) < 100 ||
          std::to_integer<int>(materialPixels[left + 1]) > 10 ||
          std::to_integer<int>(materialPixels[right + 1]) < 100 ||
          std::to_integer<int>(materialPixels[right + 2]) > 10)
        return fail(__LINE__);
    }
    materialDraw.pbr = true;
    materialDraw.cameraPosition = {0, 0, 3};
    const std::array<std::byte, 4> sideNormal{std::byte{255}, std::byte{128}, std::byte{128},
                                              std::byte{255}};
    const std::array<std::byte, 4> gray{std::byte{128}, std::byte{128}, std::byte{128},
                                        std::byte{255}};
    const std::array<std::byte, 4> metalOrm{std::byte{255}, std::byte{128}, std::byte{255},
                                            std::byte{255}};
    const std::array<std::byte, 4> dielectricOrm{std::byte{255}, std::byte{128}, std::byte{0},
                                                 std::byte{255}};
    const std::array<std::byte, 8> blackWhite{std::byte{0},   std::byte{0},   std::byte{0},
                                              std::byte{255}, std::byte{255}, std::byte{255},
                                              std::byte{255}, std::byte{255}};
    // Fixed midpoint UV gives a linear 0.5 color after hardware sRGB filtering.
    auto filterVertices = vertices;
    for (auto &vertex : filterVertices) {
      vertex.uv[0] = vertex.uv[1] = 0.5F;
    }
    const auto originalVertices = materialDraw.vertices;
    const UiTextureUpload filterUpload{46, 2, 1, 8, blackWhite};
    const std::array pbrUploads{
        UiTextureUpload{41, 1, 1, 4, sideNormal}, UiTextureUpload{42, 1, 1, 4, gray},
        UiTextureUpload{43, 1, 1, 4, metalOrm}, UiTextureUpload{44, 1, 1, 4, dielectricOrm}};
    for (int frame = 0; frame < 12; ++frame) {
      materialSlots = {};
      materialDraw.textureUploads = frame == 0 ? std::span<const UiTextureUpload>(pbrUploads)
                                               : std::span<const UiTextureUpload>{};
      const auto mode = frame % 4;
      materialDraw.light_color[0] = materialDraw.light_color[1] = materialDraw.light_color[2] =
          mode == 0 ? 0 : 1;
      if (mode == 0) {
        materialSlots[0].baseColor = materialSlots[1].baseColor = {0, 0, 0, 1};
        materialSlots[0].emission = materialSlots[1].emission = {1, 1, 1};
        materialSlots[0].emissionTextureId = 21;
        materialSlots[1].emissionTextureId = 22;
      } else if (mode == 1) {
        materialSlots[0].normalTextureId = 41;
      } else if (mode == 2) {
        materialSlots[0].metallic = materialSlots[1].metallic = 1;
        materialSlots[0].ormTextureId = 43;
        materialSlots[1].ormTextureId = 44;
      } else {
        const auto linear = static_cast<float>(std::pow((128.0 / 255.0 + 0.055) / 1.055, 2.4));
        materialSlots[0].baseColor = {linear, linear, linear, 1};
        materialSlots[1].textureId = 42;
      }
      if (frame >= 8) {
        materialDraw.vertices = filterVertices;
        materialSlots = {};
        materialDraw.light_color[0] = materialDraw.light_color[1] = materialDraw.light_color[2] = 1;
        if (frame == 8) {
          materialDraw.textureUploads = {&filterUpload, 1};
          materialSlots[0].baseColor = {0.5F, 0.5F, 0.5F, 1};
          materialSlots[1].textureId = 46;
        } else if (frame == 9) {
          materialDraw.light_color[0] = materialDraw.light_color[1] = materialDraw.light_color[2] =
              0;
          materialSlots[0].emission = {0.5F, 0.5F, 0.5F};
          materialSlots[1].emission = {1, 1, 1};
          materialSlots[1].emissionTextureId = 46;
        } else if (frame == 10) {
          materialDraw.pbr = false;
          materialSlots[0].textureId = 46;
          materialSlots[1].textureId = 42;
        } else {
          materialDraw.pbr = true;
          materialSlots[0].occlusion = materialSlots[0].roughness = materialSlots[0].metallic =
              0.5F;
          materialSlots[1].metallic = materialSlots[1].roughness = 1;
          materialSlots[1].ormTextureId = 46;
        }
      }
      if (!require(surface->Acquire(), SurfaceStatus::Ready))
        return fail(__LINE__);
      if (frame == 0) {
        materialSlots[0].normalTextureId = 987;
        if (!require(surface->DrawScene(materialDraw), SurfaceStatus::InvalidDescriptor))
          return fail(__LINE__);
        materialSlots[0].normalTextureId = 0;
      }
      if (!require(surface->DrawScene(materialDraw), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto pbrPixels = surface->ReadScenePixelsForTesting();
      if (pbrPixels.size() != captured.size())
        return fail(__LINE__);
      const auto left = (200 * 640 + 190) * 4;
      const auto right = (200 * 640 + 450) * 4;
      const auto lr = std::to_integer<int>(pbrPixels[left + 2]);
      const auto lg = std::to_integer<int>(pbrPixels[left + 1]);
      const auto rr = std::to_integer<int>(pbrPixels[right + 2]);
      const auto rg = std::to_integer<int>(pbrPixels[right + 1]);
      const bool valid =
          frame >= 8  ? lr > 40 && std::abs(lr - rr) <= 2
          : mode == 0 ? std::abs(lr - 232) <= 2 && lg < 10 && std::abs(rg - 232) <= 2 && rr < 10
          : mode == 1 ? lr < 30 && rr > 100
          : mode == 2 ? std::abs(lr - rr) > 10
                      : lr > 80 && std::abs(lr - rr) <= 2;
      if (!valid) {
        std::cerr << "Metal PBR frame=" << frame << " left=" << lr << ',' << lg << " right=" << rr
                  << ',' << rg << '\n';
        return fail(__LINE__);
      }
    }
    materialDraw.vertices = originalVertices;
    const std::array<std::byte, 4> upNormal{std::byte{128}, std::byte{255}, std::byte{128},
                                            std::byte{255}};
    const UiTextureUpload upUpload{45, 1, 1, 4, upNormal};
    materialSlots = {};
    materialSlots[0].normalTextureId = materialSlots[1].normalTextureId = 45;
    materialDraw.textureUploads = {&upUpload, 1};
    materialInstances[0].model_transform =
        std::array<float, 16>{-1, 0, 0, -0.4F, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    materialDraw.light_direction[1] = -1;
    materialDraw.light_direction[2] = 0;
    materialDraw.light_color[0] = materialDraw.light_color[1] = materialDraw.light_color[2] = 1;
    if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
        !require(surface->DrawScene(materialDraw), SurfaceStatus::Ready) ||
        !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
        !require(surface->Present(), SurfaceStatus::Ready))
      return fail(__LINE__);
    const auto mirroredPixels = surface->ReadScenePixelsForTesting();
    if (mirroredPixels.size() != captured.size())
      return fail(__LINE__);
    const auto mirrorLeft = std::to_integer<int>(mirroredPixels[(200 * 640 + 190) * 4 + 2]);
    const auto mirrorRight = std::to_integer<int>(mirroredPixels[(200 * 640 + 450) * 4 + 2]);
    if (mirrorLeft < 100 || mirrorRight < 100 || std::abs(mirrorLeft - mirrorRight) > 2)
      return fail(__LINE__);
    materialInstances = instances;
    materialInstances[1].color[1] = 1;
    materialDraw.vertices = originalVertices;
    for (unsigned mode = 0; mode < 7; ++mode) {
      PbrEnvironmentFixtures::Configure(materialDraw, materialSlots, mode);
      materialDraw.linearTextureUploads =
          mode == 0 ? std::span<const SceneLinearTextureUpload>(PbrEnvironmentFixtures::uploads)
                    : std::span<const SceneLinearTextureUpload>{};
      if (!require(surface->Acquire(), SurfaceStatus::Ready))
        return fail(__LINE__);
      if (mode == 0) {
        auto invalid = materialDraw;
        invalid.environment->specularTextureId = 999;
        if (!require(surface->DrawScene(invalid), SurfaceStatus::InvalidDescriptor))
          return fail(__LINE__);
      }
      if (!require(surface->DrawScene(materialDraw), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto environmentPixels = surface->ReadScenePixelsForTesting();
      if (environmentPixels.size() != captured.size())
        return fail(__LINE__);
      const auto read = [&](std::size_t x) {
        const auto index = (200 * 640 + x) * 4;
        return std::array<unsigned, 3>{std::to_integer<unsigned>(environmentPixels[index + 2]),
                                       std::to_integer<unsigned>(environmentPixels[index + 1]),
                                       std::to_integer<unsigned>(environmentPixels[index])};
      };
      const auto left = read(190), right = read(450);
      if (!PbrEnvironmentFixtures::Pixels(mode, left, right)) {
        std::cerr << "Metal IBL mode=" << mode << " left=" << left[0] << ',' << left[1] << ','
                  << left[2] << " right=" << right[0] << ',' << right[1] << ',' << right[2] << '\n';
        return fail(__LINE__);
      }
    }
    // Read the actual GPU tone-mapped drawable; the private test seam performs no CPU shading.
    materialDraw.environment.reset();
    materialDraw.linearTextureUploads = {};
    materialDraw.hdr = true;
    materialDraw.offscreen = true;
    materialDraw.cameraPosition = {0, 0, 3};
    for (auto &instance : materialInstances)
      for (auto &channel : instance.color)
        channel = 1;
    for (auto &channel : materialDraw.light_color)
      channel = 0;
    materialSlots = {};
    materialSlots[0].baseColor = materialSlots[1].baseColor = {0, 0, 0, 1};
    materialSlots[0].emission = {4, 0, 0};
    materialSlots[1].emission = {1, 0, 0};
    const std::array<UiVertex, 4> hdrUiVertices{{{{0, 300}, {0, 0}, 0xff808080U},
                                                 {{640, 300}, {0, 0}, 0xff808080U},
                                                 {{640, 360}, {0, 0}, 0xff808080U},
                                                 {{0, 360}, {0, 0}, 0xff808080U}}};
    const std::array<std::uint16_t, 6> hdrUiIndices{0, 1, 2, 0, 2, 3};
    const std::array hdrUiCommands{UiDrawCommand{0, 0, 640, 360, 901, 6, 0, 0}};
    const std::array<std::byte, 4> hdrUiWhite{std::byte{255}, std::byte{255}, std::byte{255},
                                              std::byte{255}};
    const std::array hdrUiUploads{UiTextureUpload{901, 1, 1, 4, hdrUiWhite}};
    UiDrawData hdrUi{};
    hdrUi.vertices = hdrUiVertices;
    hdrUi.indices = std::as_bytes(std::span(hdrUiIndices));
    hdrUi.commands = hdrUiCommands;
    hdrUi.textureUploads = hdrUiUploads;
    for (const float exposure : {1.0F, 0.125F, 0.25F, 1.0F}) {
      materialDraw.exposure = exposure;
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(materialDraw), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->RenderUi(hdrUi), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto hdrPixels = surface->ReadScenePixelsForTesting();
      if (hdrPixels.size() != captured.size())
        return fail(__LINE__);
      const auto expected = [&](float value) {
        value *= exposure;
        const float mapped =
            std::clamp(value * (2.51F * value + 0.03F) / (value * (2.43F * value + 0.59F) + 0.14F),
                       0.0F, 1.0F);
        const float srgb = mapped <= 0.0031308F ? mapped * 12.92F
                                                : 1.055F * std::pow(mapped, 1.0F / 2.4F) - 0.055F;
        return static_cast<int>(std::lround(255 * srgb));
      };
      const auto left = std::to_integer<int>(hdrPixels[(200 * 640 + 190) * 4 + 2]);
      const auto right = std::to_integer<int>(hdrPixels[(200 * 640 + 450) * 4 + 2]);
      if (std::abs(left - expected(4)) > 2 || std::abs(right - expected(1)) > 2)
        return fail(__LINE__);
      for (std::size_t channel = 0; channel < 3; ++channel)
        if (std::abs(std::to_integer<int>(hdrPixels[(330 * 640 + 320) * 4 + channel]) - 128) > 2)
          return fail(__LINE__);
    }
    materialDraw.hdr = false;
    materialDraw.exposure = 1;
    for (unsigned mode = 0; mode < 6; ++mode) {
      PbrShadowFixtures::Fixture fixture(mode);
      auto shadowDraw = fixture.Draw(mode);
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(shadowDraw), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto pixels = surface->ReadScenePixelsForTesting();
      if (pixels.size() != captured.size())
        return fail(__LINE__);
      const auto read = [&](std::size_t x, std::size_t y) {
        const auto index = (y * 640 + x) * 4;
        return std::array<unsigned, 3>{std::to_integer<unsigned>(pixels[index + 2]),
                                       std::to_integer<unsigned>(pixels[index + 1]),
                                       std::to_integer<unsigned>(pixels[index])};
      };
      const auto left = read(190, 200), right = read(450, 200), edge = read(206, 180);
      if (!PbrShadowFixtures::Pixels(mode, left, right) ||
          (mode == 0 && !(edge[0] > 20 && edge[0] + 10 < right[0]))) {
        std::cerr << "Metal shadow mode=" << mode << " left=" << left[0] << " right=" << right[0]
                  << " pcf=" << edge[0] << '\n';
        return fail(__LINE__);
      }
    }
    for (unsigned mode = 0; mode < 5; ++mode) {
      PbrBloomFixtures::Fixture fixture;
      auto bloomDraw = fixture.Draw(mode);
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(bloomDraw), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto pixels = surface->ReadScenePixelsForTesting();
      if (pixels.size() != captured.size())
        return fail(__LINE__);
      const auto read = [&](std::size_t x, std::size_t y) {
        const auto index = (y * 640 + x) * 4;
        return std::array<unsigned, 3>{std::to_integer<unsigned>(pixels[index + 2]),
                                       std::to_integer<unsigned>(pixels[index + 1]),
                                       std::to_integer<unsigned>(pixels[index])};
      };
      if (!PbrBloomFixtures::Pixels(mode, read(640 * 58 / 100, 180), read(320, 180)))
        return fail(__LINE__);
    }
    std::array<unsigned, 3> mipLeft{}, mipRight{};
    for (unsigned mode = 0; mode < 2; ++mode) {
      PbrMipFixtures::Fixture fixture(mode);
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(fixture.Draw()), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto pixels = surface->ReadScenePixelsForTesting();
      if (pixels.size() != captured.size())
        return fail(__LINE__);
      const auto read = [&](std::size_t x) {
        const auto index = (180 * 640 + x) * 4;
        return std::array<unsigned, 3>{std::to_integer<unsigned>(pixels[index + 2]),
                                       std::to_integer<unsigned>(pixels[index + 1]),
                                       std::to_integer<unsigned>(pixels[index])};
      };
      const auto left = read(160), right = read(480);
      if (!PbrMipFixtures::Pixels(left, right))
        return fail(__LINE__);
      if (mode == 0) {
        mipLeft = left;
        mipRight = right;
      }
      if (mode == 1)
        for (unsigned c = 0; c < 3; ++c)
          if (std::abs(static_cast<int>(left[c]) - static_cast<int>(mipLeft[c])) > 2 ||
              std::abs(static_cast<int>(right[c]) - static_cast<int>(mipRight[c])) > 2)
            return fail(__LINE__);
    }
    std::array<unsigned, 3> pointLeft{}, pointRight{};
    for (unsigned mode = 0; mode < 6; ++mode) {
      PbrPointLightFixtures::Fixture fixture(mode);
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(fixture.Draw()), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto pixels = surface->ReadScenePixelsForTesting();
      if (pixels.size() != captured.size())
        return fail(__LINE__);
      const auto read = [&](std::size_t x) {
        const auto index = (180 * 640 + x) * 4;
        return std::array<unsigned, 3>{std::to_integer<unsigned>(pixels[index + 2]),
                                       std::to_integer<unsigned>(pixels[index + 1]),
                                       std::to_integer<unsigned>(pixels[index])};
      };
      const auto left = read(160), right = read(480);
      if (!PbrPointLightFixtures::Pixels(mode, left, right))
        return fail(__LINE__);
      if (mode == 1) {
        pointLeft = left;
        pointRight = right;
      }
      if (mode == 3 && (left != pointLeft || right != pointRight))
        return fail(__LINE__);
    }
    for (unsigned mode = 0; mode < 4; ++mode) {
      PbrTwoSidedFixtures::Fixture fixture(mode);
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(fixture.Draw()), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto pixels = surface->ReadScenePixelsForTesting();
      if (pixels.size() != captured.size())
        return fail(__LINE__);
      const auto read = [&](std::size_t x) {
        const auto index = (180 * 640 + x) * 4;
        return std::array<unsigned, 3>{std::to_integer<unsigned>(pixels[index + 2]),
                                       std::to_integer<unsigned>(pixels[index + 1]),
                                       std::to_integer<unsigned>(pixels[index])};
      };
      const auto left = read(160), right = read(480);
      if (!PbrTwoSidedFixtures::Pixels(mode, left, right))
        return fail(__LINE__);
      if (mode == 0) {
        pointLeft = left;
        pointRight = right;
      }
      if (mode >= 2 && (left != pointLeft || right != pointRight))
        return fail(__LINE__);
    }
    unsigned aaBaseline = 0;
    std::vector<std::byte> aaReference;
    for (unsigned mode = 0; mode < 4; ++mode) {
      PbrAntiAliasFixtures::Fixture fixture(mode);
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(fixture.Draw()), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto pixels = surface->ReadScenePixelsForTesting();
      if (pixels.size() != captured.size())
        return fail(__LINE__);
      const auto read = [&](unsigned x, unsigned y) {
        const auto index = (static_cast<std::size_t>(y) * 640 + x) * 4;
        return std::array<unsigned, 3>{std::to_integer<unsigned>(pixels[index + 2]),
                                       std::to_integer<unsigned>(pixels[index + 1]),
                                       std::to_integer<unsigned>(pixels[index])};
      };
      const auto intermediate = PbrAntiAliasFixtures::Intermediate(640, 360, read);
      if (mode == 0) {
        aaBaseline = intermediate;
        aaReference = pixels;
      } else if (mode == 1 && intermediate < aaBaseline + 40)
        return fail(__LINE__);
      else if (mode == 2 && pixels != aaReference)
        return fail(__LINE__);
      else if (mode == 3 && (intermediate != 0 || read(320, 180)[0] < 245))
        return fail(__LINE__);
    }
    std::array<unsigned, 3> refractedLeft{}, refractedRight{};
    std::vector<std::byte> dielectricReference;
    for (unsigned mode = 0; mode < 12; ++mode) {
      PbrRefractionFixtures::Fixture fixture(mode);
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(fixture.Draw()), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto pixels = surface->ReadScenePixelsForTesting();
      if (pixels.size() != captured.size())
        return fail(__LINE__);
      const auto read = [&](std::size_t x) {
        const auto index = (180 * 640 + x) * 4;
        return std::array<unsigned, 3>{std::to_integer<unsigned>(pixels[index + 2]),
                                       std::to_integer<unsigned>(pixels[index + 1]),
                                       std::to_integer<unsigned>(pixels[index])};
      };
      const auto left = read(640 * 49 / 100), right = read(640 * 51 / 100);
      if (mode == 8)
        dielectricReference = pixels;
      if (mode == 11 && pixels != dielectricReference)
        return fail(__LINE__);
      if (!PbrRefractionFixtures::Pixels(mode, left, right))
        return fail(__LINE__);
      if (mode == 1) {
        refractedLeft = left;
        refractedRight = right;
      }
      if (mode == 3 && (left != refractedLeft || right != refractedRight))
        return fail(__LINE__);
    }
    std::array<unsigned, 3> flatLeft{}, flatRight{}, tiltedLeft{}, tiltedRight{};
    for (unsigned mode = 0; mode < 4; ++mode) {
      PbrWorldNormalFixtures::Fixture fixture(mode);
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(fixture.Draw()), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto pixels = surface->ReadScenePixelsForTesting();
      if (pixels.size() != captured.size())
        return fail(__LINE__);
      const auto read = [&](std::size_t x) {
        const auto index = (180 * 640 + x) * 4;
        return std::array<unsigned, 3>{std::to_integer<unsigned>(pixels[index + 2]),
                                       std::to_integer<unsigned>(pixels[index + 1]),
                                       std::to_integer<unsigned>(pixels[index])};
      };
      const auto left = read(160), right = read(480);
      if (!PbrWorldNormalFixtures::Pixels(left, right))
        return fail(__LINE__);
      if (mode == 0) {
        flatLeft = left;
        flatRight = right;
      }
      if (mode == 1) {
        if (left[1] <= flatLeft[1] + 6 || right[0] <= flatRight[0] + 6)
          return fail(__LINE__);
        tiltedLeft = left;
        tiltedRight = right;
      }
      if (mode == 2 && (std::abs(static_cast<int>(left[1]) - static_cast<int>(flatLeft[1])) > 2 ||
                        std::abs(static_cast<int>(right[0]) - static_cast<int>(flatRight[0])) > 2))
        return fail(__LINE__);
      if (mode == 3 && (left != tiltedLeft || right != tiltedRight))
        return fail(__LINE__);
    }
    std::array<unsigned, 3> fogReference{};
    for (unsigned mode = 0; mode < 6; ++mode) {
      PbrAtmosphereFixtures::Fixture fixture(mode);
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(fixture.Draw(mode)), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto pixels = surface->ReadScenePixelsForTesting();
      if (pixels.size() != captured.size())
        return fail(__LINE__);
      const auto read = [&](std::size_t x) {
        const auto index = (180 * 640 + x) * 4;
        return std::array<unsigned, 3>{std::to_integer<unsigned>(pixels[index + 2]),
                                       std::to_integer<unsigned>(pixels[index + 1]),
                                       std::to_integer<unsigned>(pixels[index])};
      };
      const auto center = read(320);
      if (!PbrAtmosphereFixtures::Pixels(mode, read(160), center))
        return fail(__LINE__);
      if (mode == 1)
        fogReference = center;
      if (mode == 3 && center != fogReference)
        return fail(__LINE__);
    }
    std::array<unsigned, 3> mappedReference{};
    for (unsigned mode = 0; mode < 4; ++mode) {
      PbrWorldMappingFixtures::Fixture fixture(mode);
      auto mappingDraw = fixture.Draw(mode);
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(mappingDraw), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto pixels = surface->ReadScenePixelsForTesting();
      if (pixels.size() != captured.size())
        return fail(__LINE__);
      const auto read = [&](std::size_t x) {
        const auto index = (180 * 640 + x) * 4;
        return std::array<unsigned, 3>{std::to_integer<unsigned>(pixels[index + 2]),
                                       std::to_integer<unsigned>(pixels[index + 1]),
                                       std::to_integer<unsigned>(pixels[index])};
      };
      const auto left = read(160);
      if (!PbrWorldMappingFixtures::Pixels(mode, left, read(480)))
        return fail(__LINE__);
      if (mode == 1)
        mappedReference = left;
      if (mode == 3 && left != mappedReference)
        return fail(__LINE__);
    }
    std::array<unsigned, 3> blendedReference{};
    for (unsigned mode = 0; mode < 6; ++mode) {
      PbrTransparencyFixtures::Fixture fixture(mode);
      auto blendDraw = fixture.Draw(mode);
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(blendDraw), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto pixels = surface->ReadScenePixelsForTesting();
      if (pixels.size() != captured.size())
        return fail(__LINE__);
      const auto read = [&](std::size_t x) {
        const auto index = (180 * 640 + x) * 4;
        return std::array<unsigned, 3>{std::to_integer<unsigned>(pixels[index + 2]),
                                       std::to_integer<unsigned>(pixels[index + 1]),
                                       std::to_integer<unsigned>(pixels[index])};
      };
      const auto center = read(320);
      if (!PbrTransparencyFixtures::Pixels(mode, read(160), center))
        return fail(__LINE__);
      if (mode == 1)
        blendedReference = center;
      if (mode == 3 && center != blendedReference)
        return fail(__LINE__);
    }
    std::array<unsigned, 3> reflectedReference{};
    for (unsigned mode = 0; mode < 6; ++mode) {
      PbrReflectionFixtures::Fixture fixture(mode);
      auto reflectionDraw = fixture.Draw(mode);
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(reflectionDraw), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto pixels = surface->ReadScenePixelsForTesting();
      if (pixels.size() != captured.size())
        return fail(__LINE__);
      const auto read = [&](std::size_t x) {
        const auto index = (180 * 640 + x) * 4;
        return std::array<unsigned, 3>{std::to_integer<unsigned>(pixels[index + 2]),
                                       std::to_integer<unsigned>(pixels[index + 1]),
                                       std::to_integer<unsigned>(pixels[index])};
      };
      const auto left = read(160);
      if (!PbrReflectionFixtures::Pixels(mode, left, read(480)))
        return fail(__LINE__);
      if (mode == 1)
        reflectedReference = left;
      if ((mode == 3 || mode == 5) && left != reflectedReference)
        return fail(__LINE__);
    }
    // Depth must select the bright near triangle regardless of index order.
    std::uint64_t windReference{}, windMoved{};
    for (unsigned mode = 0; mode < 9; ++mode) {
      PbrVegetationFixtures::Fixture fixture(mode);
      auto vegetationDraw = fixture.Draw(mode);
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(vegetationDraw), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      const auto pixels = surface->ReadScenePixelsForTesting();
      if (pixels.size() != 640 * 360 * 4)
        return fail(__LINE__);
      const auto read = [&](unsigned x, unsigned y) {
        const auto offset = (y * 640 + x) * 4;
        return std::array<unsigned, 3>{std::to_integer<unsigned>(pixels[offset + 2]),
                                       std::to_integer<unsigned>(pixels[offset + 1]),
                                       std::to_integer<unsigned>(pixels[offset])};
      };
      if (!PbrVegetationFixtures::Pixels(mode, read(160, 180), read(320, 180)))
        return fail(__LINE__);
      std::uint64_t region = 14695981039346656037ULL;
      for (unsigned y = 108; y < 252; ++y)
        for (unsigned x = 192; x < 448; ++x)
          for (const auto channel : read(x, y)) {
            region ^= channel;
            region *= 1099511628211ULL;
          }
      if (mode == 3)
        windReference = region;
      if (mode == 4) {
        windMoved = region;
        if (windMoved == windReference)
          return fail(__LINE__);
      }
      if (mode == 5 && (region != windReference || region == windMoved))
        return fail(__LINE__);
    }
    std::array<SceneVertex, 6> layered{};
    for (std::size_t i = 0; i < 3; ++i) {
      layered[i] = vertices[i];
      layered[i].position[2] = 0.2F;
      layered[i].normal[2] = -1;
      layered[i + 3] = vertices[i];
      layered[i + 3].position[2] = 0.8F;
    }
    const std::array<std::uint16_t, 6> nearFirst{0, 1, 2, 3, 4, 5};
    const std::array<std::uint16_t, 6> farFirst{3, 4, 5, 0, 1, 2};
    SceneDrawData depthDraw{layered, nearFirst};
    depthDraw.offscreen = true;
    depthDraw.light_direction[0] = depthDraw.light_direction[1] = 0;
    depthDraw.light_direction[2] = 1;
    std::vector<std::byte> nearImage;
    for (const auto order :
         {std::span<const std::uint16_t>(nearFirst), std::span<const std::uint16_t>(farFirst)}) {
      depthDraw.indices = order;
      if (!require(surface->Acquire(), SurfaceStatus::Ready) ||
          !require(surface->DrawScene(depthDraw), SurfaceStatus::Ready) ||
          !require(surface->CompositeScene(), SurfaceStatus::Ready) ||
          !require(surface->Present(), SurfaceStatus::Ready))
        return fail(__LINE__);
      auto image = surface->ReadScenePixelsForTesting();
      if (image.size() != captured.size() ||
          std::to_integer<int>(image[(180 * 640 + 320) * 4 + 2]) < 200)
        return fail(__LINE__);
      if (nearImage.empty())
        nearImage = std::move(image);
      else if (image != nearImage) {
        std::cerr << "Metal depth pixels depend on draw order\n";
        return fail(__LINE__);
      }
    }
    if (!require(surface->NotifyWindowExtent(0, 0), SurfaceStatus::ZeroExtent) ||
        !require(surface->Acquire(), SurfaceStatus::ZeroExtent) ||
        !require(surface->Acquire(), SurfaceStatus::ZeroExtent) ||
        !require(surface->NotifyWindowExtent(800, 450), SurfaceStatus::Ready) ||
        !require(surface->Acquire(), SurfaceStatus::Ready))
      return fail(__LINE__);
    draw.offscreen = false;
    draw.viewport = {100, 100, 400, 200};
    if (!require(surface->DrawScene(draw), SurfaceStatus::Ready) ||
        !require(surface->Present(), SurfaceStatus::Ready) ||
        !require(surface->DrainAndDestroy(), SurfaceStatus::Ready) ||
        !require(surface->DrainAndDestroy(), SurfaceStatus::Ready))
      return fail(__LINE__);
    // An abandoned recording must be dropped safely, without submitting it at destruction.
    auto abandoned = std::make_unique<MetalSurface>(
        SurfaceDescriptor{window.handle, 800, 450, 2, PresentMode::VSync, ColorSpace::Srgb,
                          SurfaceBackend::Metal},
        *windows);
    if (!abandoned || !require(abandoned->Acquire(), SurfaceStatus::Ready) ||
        !require(abandoned->DrawScene(draw), SurfaceStatus::Ready) ||
        !require(abandoned->DrainAndDestroy(), SurfaceStatus::Ready))
      return fail(__LINE__);
    abandoned.reset();
    surface.reset();
    if (windows->Destroy(window.handle) != Window::WindowError::None)
      return fail(__LINE__);
    std::cout << "PASS: Metal native scene/UI/copy, instancing, textures, rejection, resize and "
                 "teardown\n";
    return 0;
  }
}
