// Engine pixel contract only. Interactive/game behavior belongs in the Zig diagnostic.
#if defined(_WIN32)
#define NEXORA_DX12_UI_TESTING 1
#include "../../Engine/Presentation/src/Dx12Surface.cpp"
#elif defined(__APPLE__)
#define NEXORA_METAL_SCENE_TESTING 1
#include "../../Engine/Presentation/src/MetalSurface.mm"
#else
#include "Nexora/Presentation/Surface.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef None
#endif
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
using namespace Nexora;
using namespace Presentation;
namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
unsigned Encode(double linear) {
  const double mapped = std::clamp(
      linear * (2.51 * linear + 0.03) / (linear * (2.43 * linear + 0.59) + 0.14), 0.0, 1.0);
  const double srgb =
      mapped <= 0.0031308 ? 12.92 * mapped : 1.055 * std::pow(mapped, 1 / 2.4) - 0.055;
  return unsigned(std::lround(srgb * 255));
}
#if !defined(_WIN32) && !defined(__APPLE__)
unsigned Channel(unsigned long pixel, unsigned long mask) {
  while (!(mask & 1)) {
    mask >>= 1;
    pixel >>= 1;
  }
  return unsigned((pixel & mask) * 255 / mask);
}
#endif
struct Packet {
  std::array<UiVertex, 8> world{};
  std::array<UiVertex, 4> hud{};
  std::array<std::uint32_t, 12> indices{0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
  std::array<std::uint32_t, 6> hudIndices{0, 1, 2, 0, 2, 3};
  std::array<std::byte, 4> gray{std::byte{128}, std::byte{128}, std::byte{128}, std::byte{255}};
  std::array<std::byte, 4> blue{std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255}};
  std::array<std::byte, 4> red{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{128}};
  std::array<UiTextureUpload, 2> worldUploads{};
  UiTextureUpload blueUpload{};
  std::array<UiDrawCommand, 2> worldCommands{};
  UiDrawCommand hudCommand{};
  UiDrawData source{}, overlay{};
  PreparedSpriteDraws prepared;
  Packet(unsigned width, unsigned height, bool instances, unsigned tint = 255) {
    const auto packed = 0xff000000U | (tint * 0x010101U);
    world = {{{{0, 0}, {0, 0}, packed},
              {{float(width), 0}, {1, 0}, packed},
              {{float(width), float(height)}, {1, 1}, packed},
              {{0, float(height)}, {0, 1}, packed},
              {{float(width) / 3, float(height) / 3}, {0, 0}, 0xffffffff},
              {{float(width) * 2 / 3, float(height) / 3}, {1, 0}, 0xffffffff},
              {{float(width) * 2 / 3, float(height) * 2 / 3}, {1, 1}, 0xffffffff},
              {{float(width) / 3, float(height) * 2 / 3}, {0, 1}, 0xffffffff}}};
    hud = {{{{10, 10}, {0, 0}, 0xffffffff},
            {{30, 10}, {1, 0}, 0xffffffff},
            {{30, 30}, {1, 1}, 0xffffffff},
            {{10, 30}, {0, 1}, 0xffffffff}}};
    worldUploads = {UiTextureUpload{100, 1, 1, 4, gray, true, true},
                    UiTextureUpload{99, 1, 1, 4, red, true, true}};
    blueUpload = {101, 1, 1, 4, blue, true, true};
    worldCommands = {UiDrawCommand{0, 0, width, height, 100, 6, 0, 0},
                     UiDrawCommand{0, 0, width, height, 99, 6, 6, 0}};
    hudCommand = {0, 0, width, height, 101, 6, 0, 0};
    source = {world, std::as_bytes(std::span(indices)), worldCommands, worldUploads, true};
    overlay = {hud, std::as_bytes(std::span(hudIndices)), std::span(&hudCommand, 1),
               std::span(&blueUpload, 1), true};
    auto result = PrepareSpriteDraws(source, {}, width, height, instances, 0xff000000);
    Require(bool(result), "Sprite preparation");
    prepared = std::move(*result);
  }
};
} // namespace
int main(int argc, char **argv) {
  try {
    auto windows = Window::CreateWindowSystem();
    Require(bool(windows), "window system");
    auto window = windows->Create({"Nexora linear 2D contract", 320, 240, true, true});
    Require(bool(window), "native window");
    SurfaceDescriptor descriptor;
    descriptor.window = window.handle;
    descriptor.width = 320;
    descriptor.height = 240;
    descriptor.srgbUiOutput = true;
    descriptor.enableGpuTiming = true;
#if defined(_WIN32)
    descriptor.backend = SurfaceBackend::Dx12;
    auto surface = std::make_unique<Dx12Surface>(descriptor, *windows);
#elif defined(__APPLE__)
    if (!MTLCreateSystemDefaultDevice()) {
      std::cout << "SKIP: Metal device unavailable\n";
      return 77;
    }
    descriptor.backend = SurfaceBackend::Metal;
    auto surface = std::make_unique<MetalSurface>(descriptor, *windows);
#else
    descriptor.backend = SurfaceBackend::Vulkan;
    auto surface = CreateSurface(descriptor, *windows);
    auto *display = XOpenDisplay(nullptr);
    Require(display, "X11 display");
    const auto handle = reinterpret_cast<::Window>(windows->NativeHandle(window.handle));
#endif
    Require(bool(surface) && surface->SupportsTwoDLinear(), "native linear FP16 capability");
    const auto read = [&](unsigned width, unsigned height, const std::array<unsigned, 3> &expected,
                          unsigned expectedGray, bool bars, bool hasHud) {
      (void)expected;
      (void)expectedGray;
      (void)bars;
      (void)hasHud;
#if defined(_WIN32)
      auto pixels = surface->PresentUiPixelsForTesting();
      Require(pixels.size() == std::size_t(width) * height * 4, "DX12 GPU readback");
      return pixels;
#elif defined(__APPLE__)
      Require(surface->Present() == SurfaceStatus::Ready, "Metal present");
      auto pixels = surface->ReadScenePixelsForTesting();
      Require(pixels.size() == std::size_t(width) * height * 4, "Metal GPU readback");
      for (std::size_t i = 0; i < pixels.size(); i += 4)
        std::swap(pixels[i], pixels[i + 2]);
      return pixels;
#else
      Require(surface->Present() == SurfaceStatus::Ready, "Vulkan present");
      std::vector<std::byte> pixels(std::size_t(width) * height * 4);
      bool complete = false;
      for (unsigned retry = 0; retry < 100 && !complete; ++retry) {
        XSync(display, False);
        auto *image = XGetImage(display, handle, 0, 0, width, height, AllPlanes, ZPixmap);
        Require(image, "Vulkan X11 readback");
        const auto center = XGetPixel(image, width / 2, height / 2);
        const auto hudPixel = XGetPixel(image, 20, 20), left = XGetPixel(image, 2, height / 2),
                   right = XGetPixel(image, width - 2, height / 2);
        complete =
            std::abs(int(Channel(center, image->red_mask)) - int(expected[0])) <= 2 &&
            std::abs(int(Channel(center, image->green_mask)) - int(expected[1])) <= 2 &&
            std::abs(int(Channel(center, image->blue_mask)) - int(expected[2])) <= 2 &&
            ((hasHud && Channel(hudPixel, image->blue_mask) >= 253) ||
             (!hasHud &&
              std::abs(int(Channel(hudPixel, image->blue_mask)) - int(expectedGray)) <= 2)) &&
            std::abs(int(Channel(left, image->red_mask)) - int(bars ? 0 : expectedGray)) <= 2 &&
            std::abs(int(Channel(right, image->red_mask)) - int(bars ? 0 : expectedGray)) <= 2;
        if (complete)
          for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x) {
              const auto color = XGetPixel(image, x, y);
              auto *p = pixels.data() + 4 * (std::size_t(y) * width + x);
              p[0] = std::byte(Channel(color, image->red_mask));
              p[1] = std::byte(Channel(color, image->green_mask));
              p[2] = std::byte(Channel(color, image->blue_mask));
              p[3] = std::byte{255};
            }
        XDestroyImage(image);
        if (!complete)
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      Require(complete, "Vulkan presentation completion");
      return pixels;
#endif
    };
    const std::array<std::array<unsigned, 2>, 5> sizes{
        {{320, 240}, {321, 239}, {1920, 1080}, {2520, 1080}, {640, 480}}};
    std::vector<std::byte> finalPixels;
    unsigned finalWidth{}, finalHeight{};
    std::uint64_t frames = 0;
    for (const auto &size : sizes) {
      const auto width = size[0], height = size[1];
      Require(windows->Resize(window.handle, width, height) == Window::WindowError::None,
              "window resize");
      (void)windows->PumpEvents();
      Require(surface->NotifyWindowExtent(width, height) == SurfaceStatus::Ready,
              "extent notification");
      for (unsigned frame = 0; frame < 8; ++frame) {
        auto status = surface->Acquire();
        for (unsigned retry = 0; status == SurfaceStatus::OutOfDate && retry < 20; ++retry) {
          (void)windows->PumpEvents();
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
          status = surface->Acquire();
        }
        Require(status == SurfaceStatus::Ready, "acquire after resize");
        const unsigned tint = 255 - frame * 32;
        const double base = (128. / 255) * (double(tint) / 255), alpha = 128. / 255;
        const std::array<unsigned, 3> expected{Encode(alpha + (1 - alpha) * base),
                                               Encode((1 - alpha) * base),
                                               Encode((1 - alpha) * base)};
        Packet packet(width, height, (frame % 2) == 0, tint);
        TwoDLinearDrawData data{
            packet.prepared.View(), packet.overlay, {40, 0, width - 80, height}};
        auto bad = data;
        bad.viewport = {width, 0, 1, height};
        Require(surface->DrawTwoDLinear(bad) == SurfaceStatus::InvalidDescriptor,
                "invalid frame rejected");
        Require(surface->DrawTwoDLinear(data) == SurfaceStatus::Ready,
                "native FP16 world/composite/HUD");
        auto pixels = read(width, height, expected, Encode(base), true, true);
        const auto channel = [&](unsigned x, unsigned y, unsigned c) {
          return std::to_integer<unsigned>(pixels[4 * (std::size_t(y) * width + x) + c]);
        };
        for (unsigned c = 0; c < 3; ++c)
          Require(std::abs(int(channel(width / 2, height / 2, c)) - int(expected[c])) <= 2,
                  "CPU tone/sRGB oracle");
        Require(channel(20, 20, 2) >= 253 && channel(20, 20, 0) <= 2, "HUD is not tone mapped");
        Require(channel(2, height / 2, 0) <= 2 && channel(width - 2, height / 2, 0) <= 2,
                "black safe-frame bars");
        ++frames;
        finalPixels = std::move(pixels);
        finalWidth = width;
        finalHeight = height;
      }
    }
    Require(surface->NotifyWindowExtent(0, 0) == SurfaceStatus::ZeroExtent, "zero extent request");
    Require(surface->Acquire() == SurfaceStatus::ZeroExtent, "minimized frame suspended");
    Require(surface->NotifyWindowExtent(finalWidth, finalHeight) == SurfaceStatus::Ready,
            "restore request");
    Require(surface->Acquire() == SurfaceStatus::Ready, "restore acquire");
    Packet restored(finalWidth, finalHeight, true);
    Require(surface->DrawTwoDLinear({restored.prepared.View(), restored.overlay, {}}) ==
                SurfaceStatus::Ready,
            "restored draw");
    const double alpha = 128. / 255, base = 128. / 255;
    const std::array<unsigned, 3> restoredExpected{
        Encode(alpha + (1 - alpha) * base), Encode((1 - alpha) * base), Encode((1 - alpha) * base)};
    (void)read(finalWidth, finalHeight, restoredExpected, Encode(base), false, true);
    ++frames;
    Require(surface->Acquire() == SurfaceStatus::Ready, "world-only acquire");
    Require(surface->DrawTwoDLinear({restored.prepared.View(), {}, {}}) == SurfaceStatus::Ready,
            "world without HUD");
    (void)read(finalWidth, finalHeight, restoredExpected, Encode(base), false, false);
    ++frames;
    const auto state = surface->Diagnostics();
    Require(state.twoDLinearFrames == frames && state.twoDLinearTargetBytes > 0,
            "native frame/resource accounting");
    Require(state.twoDLinearTargetAllocations <= 18, "bounded completed-slot target reuse");
    Require(state.spriteGpuInstances > 0, "native instanced path exercised");
    if (argc == 2) {
      const auto parent = std::filesystem::path(argv[1]).parent_path();
      if (!parent.empty())
        std::filesystem::create_directories(parent);
      std::ofstream image(std::filesystem::path(argv[1]), std::ios::binary);
      image << "P6\n" << finalWidth << " " << finalHeight << "\n255\n";
      for (std::size_t i = 0; i < finalPixels.size(); i += 4)
        image.write(reinterpret_cast<const char *>(finalPixels.data() + i), 3);
      Require(bool(image), "pixel evidence write");
    }
    Require(surface->DrainAndDestroy() == SurfaceStatus::Ready, "native drain");
    Require(surface->Diagnostics().twoDLinearTargetBytes == 0 && !surface->SupportsTwoDLinear(),
            "target teardown");
#if !defined(_WIN32) && !defined(__APPLE__)
    XCloseDisplay(display);
#endif
    std::cout << "Linear 2D native CPU oracle/HUD/black bars/instancing/1080p/21:9/odd "
                 "sizes/resize/zero restore/retirement PASS frames="
              << frames << "\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
