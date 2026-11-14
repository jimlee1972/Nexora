#include "SceneUiFixture.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef None
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
unsigned Channel(unsigned long value, unsigned long mask) {
  if (!mask)
    return 0;
  while (!(mask & 1UL)) {
    value >>= 1;
    mask >>= 1;
  }
  return static_cast<unsigned>((value & mask) * 255 / mask);
}
void Pixels(Display *display, ::Window window, unsigned width, unsigned height,
            const std::filesystem::path &capture, bool srgb) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    XSync(display, False);
    auto *image = XGetImage(display, window, 0, 0, width, height, AllPlanes, ZPixmap);
    Require(image != nullptr, "Scene/UI native readback failed");
    const auto at = [&](unsigned x, unsigned y) {
      const auto pixel = XGetPixel(image, x, y);
      return std::array{Channel(pixel, image->red_mask), Channel(pixel, image->green_mask),
                        Channel(pixel, image->blue_mask)};
    };
    const bool passed =
        SceneUiFixture::Pixels(at(width / 4, height / 2), at(width / 8, height / 2),
                               at(width / 4, height / 8), at(width * 3 / 4, height / 2),
                               at(width * 3 / 4, height / 8)) &&
        SceneUiFixture::Orientation(
            {at(width * 5 / 16, height / 4), at(width * 5 / 32, height / 4),
             at(width * 5 / 32, height * 3 / 4), at(width * 5 / 16, height * 3 / 4)},
            {at(width * 13 / 16, height * 3 / 8), at(width * 21 / 32, height * 3 / 8),
             at(width * 21 / 32, height * 5 / 8), at(width * 13 / 16, height * 5 / 8)},
            srgb);
    const bool finalObservation =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(10) >= deadline;
    if (!passed && finalObservation) {
      std::cerr << "Scene orientation samples (image/direct):";
      for (const auto point :
           {std::array{width * 5 / 16, height / 4}, std::array{width * 5 / 32, height / 4},
            std::array{width * 5 / 32, height * 3 / 4}, std::array{width * 5 / 16, height * 3 / 4},
            std::array{width * 13 / 16, height * 3 / 8},
            std::array{width * 21 / 32, height * 3 / 8},
            std::array{width * 21 / 32, height * 5 / 8},
            std::array{width * 13 / 16, height * 5 / 8}}) {
        const auto color = at(point[0], point[1]);
        std::cerr << ' ' << color[0] << ',' << color[1] << ',' << color[2];
      }
      std::cerr << '\n';
    }
    if (!capture.empty() && (passed || finalObservation)) {
      std::ofstream file(capture, std::ios::binary);
      file << "P6\n" << width << ' ' << height << "\n255\n";
      for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
          const auto rgb = at(x, y);
          const std::array<char, 3> bytes{static_cast<char>(rgb[0]), static_cast<char>(rgb[1]),
                                          static_cast<char>(rgb[2])};
          file.write(bytes.data(), bytes.size());
        }
      Require(file.good(), "Scene/UI capture write failed");
    }
    XDestroyImage(image);
    if (passed)
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  throw std::runtime_error(
      "native Scene/UI/source-style/occlusion or disjoint-view pixels changed");
}
} // namespace
int main(int argc, char **argv) {
  using namespace Nexora;
  using namespace Nexora::Presentation;
  try {
    auto windows = Window::CreateWindowSystem();
    Require(bool(windows), "window system unavailable");
    const auto created = windows->Create({"Nexora Scene UI composition", 640, 480, true, false});
    Require(bool(created), "native Scene/UI window failed");
    Require(windows->Show(created.handle, true) == Window::WindowError::None, "show failed");
    auto *display = XOpenDisplay(nullptr);
    Require(display != nullptr, "X11 display failed");
    const auto native = reinterpret_cast<::Window>(windows->NativeHandle(created.handle));
    SurfaceDescriptor descriptor{};
    descriptor.window = created.handle;
    descriptor.width = 640;
    descriptor.height = 480;
    descriptor.backend = SurfaceBackend::Vulkan;
    auto surface = CreateSurface(descriptor, *windows);
    Require(bool(surface), "Vulkan surface failed");
    SceneUiFixture fixture;
#if defined(NEXORA_SCENE_UI_DUAL)
    constexpr bool dual = true;
#else
    constexpr bool dual = false;
#endif
    unsigned width = 640, height = 480;
    for (unsigned frame = 0; frame < 12; ++frame) {
      if (frame == 4 || frame == 8) {
        width = frame == 4 ? 480 : 640;
        height = frame == 4 ? 360 : 480;
        Require(windows->Resize(created.handle, width, height) == Window::WindowError::None &&
                    surface->NotifyWindowExtent(width, height) == SurfaceStatus::Ready,
                "Scene/UI resize failed");
      }
      const bool drawn = fixture.Draw(*surface, width, height, dual);
      if (!drawn)
        std::cerr << fixture.failure << '\n';
      Require(drawn, "native Scene/UI fixture rejected");
      Pixels(display, native, width, height,
             argc == 2 ? std::filesystem::path(argv[1]) : std::filesystem::path{},
             surface->Diagnostics().uiTargetEncodesSrgb);
    }
    const auto stats = surface->Diagnostics();
    Require(stats.presentedFrames == 12 && stats.sceneDrawCalls == 24 &&
                stats.sceneComposites == 12 && stats.nativeUiDrawCalls == (dual ? 60 : 48) &&
                stats.nativeUiRejectedTextures == 0,
            "Scene/UI actual submission counts changed");
    Require(surface->DrainAndDestroy() == SurfaceStatus::Ready && !surface->SceneTexture() &&
                surface->DrainAndDestroy() == SurfaceStatus::Ready,
            "Scene/UI drain or reference invalidation failed");
    XCloseDisplay(display);
    Require(windows->Destroy(created.handle) == Window::WindowError::None, "window drain failed");
    std::cout << "PASS12native Scene/UI compositions,24scenes," << (dual ? 60 : 48)
              << "UI commands,2resizes;oneUI pass/frame;dual=" << dual << '\n';
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
