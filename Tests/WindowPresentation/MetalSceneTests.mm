#define NEXORA_METAL_SCENE_TESTING 1
// Compile the real private adapter into this test to read back GPU output without adding public
// handles.
#include "../../Engine/Presentation/src/MetalSurface.mm"
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
    // Depth must select the bright near triangle regardless of index order.
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
