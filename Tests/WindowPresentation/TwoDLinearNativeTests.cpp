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
#include "TwoDBloomReference.h"
#include "TwoDShadowReference.h"
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
std::array<double, 3> LightReference(const TwoDLightingUniform &u, const TwoDLightingStyle &style,
                                     double x, double y,
                                     const TwoDShadowReference::Frame *shadows = nullptr) {
  if (!style.receiveLight)
    return {1, 1, 1};
  const double wx = x * u.worldX[0] + y * u.worldX[1] + u.worldX[2];
  const double wy = x * u.worldY[0] + y * u.worldY[1] + u.worldY[2];
  std::array<double, 3> result{u.ambientStrength[0], u.ambientStrength[1], u.ambientStrength[2]};
  for (unsigned i = 0; i < u.counts[0]; ++i) {
    const auto &l = u.lights[i];
    if (!(l.masks[0] & style.groups))
      continue;
    const double q =
        std::hypot(wx - l.positionRadiusIntensity[0], wy - l.positionRadiusIntensity[1]) /
        l.positionRadiusIntensity[2];
    double attenuation = q < 1 ? std::pow(1 - q * q, 2) : 0;
    if (shadows && style.receiveShadow)
      attenuation *= shadows->Visibility(l, wx, wy);
    for (unsigned c = 0; c < 3; ++c)
      result[c] += l.color[c] * l.positionRadiusIntensity[3] * attenuation * u.ambientStrength[3];
  }
  return result;
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
  std::array<UiTextureUpload, 3> worldUploads{};
  UiTextureUpload blueUpload{};
  std::array<UiDrawCommand, 2> worldCommands{};
  UiDrawCommand hudCommand{};
  UiDrawData source{}, overlay{};
  PreparedSpriteDraws prepared;
  Packet(unsigned width, unsigned height, bool instances, unsigned tint = 255,
         bool opaqueForeground = false) {
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
    if (opaqueForeground)
      red[3] = std::byte{255};
    const uint64_t redId = opaqueForeground ? 104 : 99;
    worldUploads = {UiTextureUpload{100, 1, 1, 4, gray, true, true},
                    UiTextureUpload{redId, 1, 1, 4, red, true, true},
                    UiTextureUpload{102, 1, 1, 4, gray, false, true}};
    blueUpload = {101, 1, 1, 4, blue, true, true};
    worldCommands = {UiDrawCommand{0, 0, width, height, 100, 6, 0, 0},
                     UiDrawCommand{0, 0, width, height, redId, 6, 6, 0}};
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
    Require(surface->SupportsTwoDShadows(), "native compute shadow capability");
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
    std::uint64_t frames = 0, litFrames = 0;
    std::uint32_t maximumLights{}, maximumShadowLights{};
    std::uint64_t shadowFrames{}, shadowSamples{};
    for (const auto &size : sizes) {
      const auto width = size[0], height = size[1];
      Require(windows->Resize(window.handle, width, height) == Window::WindowError::None,
              "window resize");
      (void)windows->PumpEvents();
      Require(surface->NotifyWindowExtent(width, height) == SurfaceStatus::Ready,
              "extent notification");
      for (unsigned frame = 0; frame < 10; ++frame) {
        auto status = surface->Acquire();
        for (unsigned retry = 0; status == SurfaceStatus::OutOfDate && retry < 20; ++retry) {
          (void)windows->PumpEvents();
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
          status = surface->Acquire();
        }
        Require(status == SurfaceStatus::Ready, "acquire after resize");
        const unsigned tint = 255 - (frame % 8) * 32;
        const double base = (128. / 255) * (double(tint) / 255),
                     alpha = frame == 8 ? 1. : 128. / 255;
        Packet packet(width, height, (frame % 2) == 0, tint, frame == 8);
        TwoDLighting lighting;
        std::array<TwoDLightingStyle, 3> styles{
            {{UINT32_MAX, 0, 0, 0},
             {frame == 1 ? 1U : 3U, frame == 4 ? 0U : 1U, 1, 0},
             {frame == 6 ? 3U : 2U, frame == 2 || frame == 6 ? 1U : 0U, 1, 0}}};
        if (frame == 8 || frame == 9) {
          styles[1].emissionTexture = 102;
          styles[1].emission = {0.1F, 0.7F, 1.F, 4.F};
        }
        lighting.styles = styles;
        auto &uniform = lighting.uniforms;
        uniform.worldX = {0.01F, 0, -float(width) * 0.005F, 0};
        uniform.worldY = {0, -0.01F, float(height) * 0.005F, 0};
        uniform.ambientStrength = {0.15F, 0.2F, 0.25F,
                                   frame == 5   ? 0.F
                                   : frame == 2 ? 0.5F
                                   : frame == 1 ? 0.75F
                                                : 1.F};
        uniform.counts[0] = frame == 2 ? 32 : frame == 4 ? 0 : 2;
        for (unsigned i = 0; i < uniform.counts[0]; ++i) {
          auto &light = uniform.lights[i];
          light.positionRadiusIntensity = {float(std::sin(double(i) + frame * 0.3)) * 0.7F,
                                           float(std::cos(double(i) + frame * 0.2)) * 0.4F, 1.5F,
                                           (frame == 1 ? 24.F : 2.F) / uniform.counts[0]};
          light.color = i % 2 ? std::array<float, 4>{0, 0.3F, 0.8F, 0}
                              : std::array<float, 4>{0.8F, 0.1F, 0, 0};
          light.masks[0] = i % 2 ? 2 : 1;
        }
        const bool lit = frame % 4 != 3;
        const bool shadowsEnabled = lit && frame != 4 && frame != 5;
        std::array<TwoDShadowEdge, 4> edges{};
        const float left = frame == 6 ? -1.F : -0.2F + 0.1F * float(std::sin(double(frame)));
        const float right = frame == 6 ? -0.5F : 0.2F + 0.1F * float(std::sin(double(frame)));
        const float bottom = frame == 6 ? -0.25F : 0.12F, top = frame == 6 ? 0.25F : 0.22F;
        const std::array<std::array<float, 4>, 4> contour{{{left, bottom, right, bottom},
                                                           {right, bottom, right, top},
                                                           {right, top, left, top},
                                                           {left, top, left, bottom}}};
        for (unsigned i = 0; i < 4; ++i) {
          edges[i].points = contour[i];
          edges[i].masks[0] = frame == 1 ? 4 : frame == 2 ? 2 : frame == 6 ? 3 : 1;
        }
        if (frame == 6)
          uniform.lights[0].positionRadiusIntensity[0] = -1;
        if (frame == 6)
          uniform.lights[0].positionRadiusIntensity[1] = 0;
        if (shadowsEnabled) {
          uniform.counts[1] = std::min(8U, uniform.counts[0]);
          lighting.edges = frame == 8 ? std::span<const TwoDShadowEdge>{} : edges;
          uniform.counts[2] = uint32_t(lighting.edges.size());
          for (unsigned i = 0; i < uniform.counts[1]; ++i) {
            uniform.shadowLights[i / 4][i % 4] = i;
            uniform.lights[i].masks[1] = i + 1;
            uniform.lights[i].shadow = {frame == 0   ? 0.F
                                        : frame == 1 ? 0.25F
                                        : frame == 2 ? 0.75F
                                                     : 1.F,
                                        0.002F, 0, 0};
          }
        }
        TwoDBloomReference::Image bloomReference;
        if (frame == 0 || frame == 8 || frame == 9) {
          uniform.counts[3] = 5;
          uniform.bloom = {0.6F, 16.F, 0, 0};
        }
        if (frame == 8 || frame == 9) {
          TwoDBloomReference::Image emission{width, height, {}};
          emission.pixels.resize(std::size_t(width) * height);
          for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 40; x < width - 40; ++x) {
              const bool foreground =
                  x + 0.5 > double(width) / 3 && x + 0.5 < double(width) * 2 / 3 &&
                  y + 0.5 > double(height) / 3 && y + 0.5 < double(height) * 2 / 3;
              emission.pixels[std::size_t(y) * width + x] =
                  (128. / 255) * (foreground ? 1 - alpha : 1);
            }
          bloomReference = TwoDBloomReference::Filter(std::move(emission), 5, 16);
        }
        const TwoDShadowReference::Frame reference(uniform, lighting.edges);
        const auto expectedAt = [&](unsigned px, unsigned py) {
          const double x = px + 0.5, y = py + 0.5;
          const auto grayLight = lit ? LightReference(uniform, styles[1], x, y, &reference)
                                     : std::array<double, 3>{1, 1, 1};
          const auto redLight = lit ? LightReference(uniform, styles[2], x, y, &reference)
                                    : std::array<double, 3>{1, 1, 1};
          std::array<double, 3> grayRadiance{}, redRadiance{};
          for (unsigned c = 0; c < 3; ++c) {
            grayRadiance[c] = base * grayLight[c] + styles[1].emission[c] * styles[1].emission[3] *
                                                        (128. / 255) * uniform.ambientStrength[3];
            redRadiance[c] = (c == 0 ? redLight[c] : 0) + styles[2].emission[c] *
                                                              styles[2].emission[3] * (128. / 255) *
                                                              uniform.ambientStrength[3];
          }
          const bool foreground = x > double(width) / 3 && x < double(width) * 2 / 3 &&
                                  y > double(height) / 3 && y < double(height) * 2 / 3;
          const double coverage = foreground ? alpha : 0;
          std::array<double, 3> bloom{};
          if (!bloomReference.pixels.empty())
            for (unsigned c = 0; c < 3; ++c)
              bloom[c] = bloomReference.Sample(x / width, y / height) * styles[1].emission[c] *
                         styles[1].emission[3] * uniform.bloom[0] * uniform.ambientStrength[3];
          return std::array<unsigned, 3>{
              Encode(coverage * redRadiance[0] + (1 - coverage) * grayRadiance[0] + bloom[0]),
              Encode(coverage * redRadiance[1] + (1 - coverage) * grayRadiance[1] + bloom[1]),
              Encode(coverage * redRadiance[2] + (1 - coverage) * grayRadiance[2] + bloom[2])};
        };
        const auto expected = expectedAt(width / 2, height / 2);
        TwoDLinearDrawData data{packet.prepared.View(),
                                packet.overlay,
                                {40, 0, width - 80, height},
                                lit ? &lighting : nullptr};
        auto bad = data;
        bad.viewport = {width, 0, 1, height};
        Require(surface->DrawTwoDLinear(bad) == SurfaceStatus::InvalidDescriptor,
                "invalid frame rejected");
        auto shortLighting = lighting;
        shortLighting.styles = std::span(styles).first(2);
        bad = data;
        bad.lighting = &shortLighting;
        Require(surface->DrawTwoDLinear(bad) == SurfaceStatus::InvalidDescriptor,
                "incomplete style batch rejected before rendering");
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
        if (lit) {
          for (const auto py : {height / 2 - 1, height / 2 + 1, height / 2 - 50, height / 2 + 50}) {
            const auto probe = expectedAt(width / 2, py);
            for (unsigned c = 0; c < 3; ++c)
              Require(std::abs(int(channel(width / 2, py, c)) - int(probe[c])) <= 2,
                      "CPU shadow/soft-edge/seam/HDR oracle");
          }
        }
        ++frames;
        if (frame == 8 || frame == 9) {
          for (const auto px :
               {width / 3 - 2, width / 3 + 2, width * 2 / 3 - 2, width * 2 / 3 + 2}) {
            const auto expectedEdge = expectedAt(px, height / 2);
            for (unsigned c = 0; c < 3; ++c)
              Require(std::abs(int(channel(px, height / 2, c)) - int(expectedEdge[c])) <= 2,
                      "selective bloom/opaque or transparent foreground edge oracle");
          }
        }
        if (shadowsEnabled && uniform.counts[1]) {
          ++shadowFrames;
          shadowSamples += uint64_t(uniform.counts[1]) * 1024;
          maximumShadowLights = std::max(maximumShadowLights, uniform.counts[1]);
        }
        if (lit) {
          ++litFrames;
          maximumLights = std::max(maximumLights, uniform.counts[0]);
        }
        if (frame == 2) {
          finalPixels = std::move(pixels);
          finalWidth = width;
          finalHeight = height;
        }
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
    Require(litFrames == 40 && maximumLights == 32, "moving/grouped/32-light native matrix");
    const auto state = surface->Diagnostics();
    Require(state.twoDLinearFrames == frames && state.twoDLinearTargetBytes > 0 &&
                state.twoDBloomPasses == 135 && state.twoDEmissionDraws == 10,
            "native frame/resource accounting");
    Require(state.twoDLinearTargetAllocations <= 18, "bounded completed-slot target reuse");
    Require(state.spriteGpuInstances > 0, "native instanced path exercised");
    Require(maximumShadowLights == 8 && shadowFrames == 30 &&
                state.twoDShadowDispatches == shadowFrames &&
                state.twoDShadowSamples == shadowSamples && state.twoDShadowBufferBytes > 0,
            "GPU shadow dispatch/capacity/sample/resource accounting");
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
    Require(surface->Diagnostics().twoDLinearTargetBytes == 0 &&
                surface->Diagnostics().twoDShadowBufferBytes == 0 &&
                surface->Diagnostics().twoDBloomTargetBytes == 0 && !surface->SupportsTwoDLinear(),
            "target teardown");
#if !defined(_WIN32) && !defined(__APPLE__)
    XCloseDisplay(display);
#endif
    std::cout << "Linear 2D native CPU oracle/HUD/black bars/instancing/1080p/21:9/odd "
                 "sizes/resize/zero restore/retirement PASS frames="
              << frames << " light_frames=" << litFrames << " maximum_lights=" << maximumLights
              << " shadow_frames=" << shadowFrames
              << " maximum_shadow_lights=" << maximumShadowLights << "\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
