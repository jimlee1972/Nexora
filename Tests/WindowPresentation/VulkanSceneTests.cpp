#include "Nexora/Presentation/Surface.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef None

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace Nexora;
using Presentation::SurfaceStatus;
void Require(bool condition, const char *message) {
  if (!condition)
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
// Pixel readback belongs exclusively to this target-host test, never the render path.
void CheckPixels(Display *display, ::Window window, unsigned width, unsigned height,
                 bool translated, const std::filesystem::path &capture) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    XSync(display, False);
    auto *image = XGetImage(display, window, 0, 0, width, height, AllPlanes, ZPixmap);
    Require(image != nullptr, "X11 scene readback failed");
    const auto center = XGetPixel(image, width / 2, height / 2);
    const auto right = XGetPixel(image, width * 3 / 4, height / 2);
    const auto green = Channel(center, image->green_mask);
    const auto rightGreen = Channel(right, image->green_mask);
    const bool valid = translated ? green < 100 && rightGreen > 180 : green > 180;
    if (valid && !capture.empty()) {
      std::ofstream file(capture, std::ios::binary);
      file << "P6\n" << width << ' ' << height << "\n255\n";
      for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
          const auto pixel = XGetPixel(image, x, y);
          const char rgb[] = {static_cast<char>(Channel(pixel, image->red_mask)),
                              static_cast<char>(Channel(pixel, image->green_mask)),
                              static_cast<char>(Channel(pixel, image->blue_mask))};
          file.write(rgb, sizeof(rgb));
        }
      Require(file.good(), "scene capture write failed");
    }
    XDestroyImage(image);
    if (valid)
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  throw std::runtime_error("scene pixels failed depth/material/transform acceptance");
}
} // namespace

int main(int argc, char **argv) {
  try {
    auto windows = Window::CreateWindowSystem();
    Require(windows != nullptr, "X11 window system unavailable");
    const auto created = windows->Create({"Nexora Vulkan scene acceptance", 640, 480, true, false});
    Require(static_cast<bool>(created), "X11 window creation failed");
    Require(windows->Show(created.handle, true) == Window::WindowError::None, "show failed");
    auto *display = XOpenDisplay(nullptr);
    Require(display != nullptr, "X11 readback display unavailable");
    const auto native = reinterpret_cast<::Window>(windows->NativeHandle(created.handle));
    Presentation::SurfaceDescriptor descriptor{};
    descriptor.window = created.handle;
    descriptor.width = 640;
    descriptor.height = 480;
    descriptor.backend = Presentation::SurfaceBackend::Vulkan;
    auto surface = Presentation::CreateSurface(descriptor, *windows);
    Require(surface != nullptr, "Vulkan surface creation failed");
    std::array<Presentation::SceneVertex, 6> vertices{{
        {{-0.7F, -0.7F, 0.2F}, {0, 0, 1}},
        {{0.7F, -0.7F, 0.2F}, {0, 0, 1}},
        {{0, 0.7F, 0.2F}, {0, 0, 1}},
        {{-0.7F, -0.7F, 0.8F}, {1, 0, 0}},
        {{0.7F, -0.7F, 0.8F}, {1, 0, 0}},
        {{0, 0.7F, 0.8F}, {1, 0, 0}},
    }};
    std::array<std::uint16_t, 6> indices{0, 1, 2, 3, 4, 5};
    Presentation::SceneDrawData draw{};
    draw.vertices = vertices;
    draw.indices = indices;
    draw.light_direction[0] = draw.light_direction[1] = 0;
    draw.light_direction[2] = -1;
    draw.base_color[0] = draw.base_color[2] = 0.1F;
    draw.base_color[1] = 0.8F;
    Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
            "draw without acquire accepted");
    unsigned width = 640, height = 480;
    for (unsigned frame = 0; frame < 12; ++frame) {
      if (frame == 4 || frame == 8) {
        width = frame == 4 ? 480 : 640;
        height = frame == 4 ? 360 : 480;
        Require(windows->Resize(created.handle, width, height) == Window::WindowError::None,
                "resize failed");
        // The event belongs to the application; the public surface consumes its published extent.
        Require(surface->NotifyWindowExtent(width, height) == SurfaceStatus::Ready,
                "extent failed");
      }
      static_cast<void>(windows->PumpEvents());
      Require(surface->Acquire() == SurfaceStatus::Ready, "scene acquire failed");
      if (frame == 0) {
        auto invalid = draw;
        invalid.model_view_projection[0] = std::numeric_limits<float>::quiet_NaN();
        Require(surface->DrawScene(invalid) == SurfaceStatus::InvalidDescriptor,
                "nonfinite matrix accepted");
        indices[0] = 6;
        Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
                "invalid index accepted");
        indices[0] = 0;
        invalid = draw;
        invalid.indices = std::span(indices).first(2);
        Require(surface->DrawScene(invalid) == SurfaceStatus::InvalidDescriptor,
                "incomplete triangle accepted");
        SurfaceStatus wrongThread{};
        std::thread other([&] { wrongThread = surface->DrawScene(draw); });
        other.join();
        Require(wrongThread == SurfaceStatus::WrongThread, "wrong thread accepted");
        Require(surface->Diagnostics().sceneDrawCalls == 0, "invalid draws changed counters");
      }
      const bool translated = frame % 2 != 0;
      draw.model_view_projection[3] = translated ? 0.75F : 0.0F;
      // Reverse triangle order on alternate pairs; both orders must preserve the near lit face.
      indices = frame % 4 < 2 ? std::array<std::uint16_t, 6>{0, 1, 2, 3, 4, 5}
                              : std::array<std::uint16_t, 6>{3, 4, 5, 0, 1, 2};
      Require(surface->DrawScene(draw) == SurfaceStatus::Ready, "native indexed scene draw failed");
      Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
              "duplicate draw accepted");
      const std::vector<std::byte> composite(static_cast<std::size_t>(width) * height * 4);
      Require(surface->CompositeRgba8(composite, width, height) == SurfaceStatus::InvalidDescriptor,
              "scene/composite mixing accepted");
      Require(surface->RenderUi({}) == SurfaceStatus::InvalidDescriptor,
              "scene/UI mixing accepted");
      Require(surface->Present() == SurfaceStatus::Ready, "scene present failed");
      const auto capture =
          argc == 2 && frame == 10 ? std::filesystem::path(argv[1]) : std::filesystem::path{};
      CheckPixels(display, native, width, height, translated, capture);
    }
    // One triangle upload, two hardware instances, with independent transforms and tints.
    // Three uint16 indices also exercise the instance-buffer's four-byte alignment.
    std::array<Presentation::SceneInstance, 2> instances{
        {{{-0.5F, 0, 0}, {0.4F, 0.8F, 1}, {1, 0, 0, 1}},
         {{0.5F, 0, 0}, {0.4F, 0.8F, 1}, {0, 1, 0, 1}}}};
    draw.vertices = std::span(vertices).first(3);
    indices = {0, 1, 2, 3, 4, 5};
    draw.indices = std::span(indices).first(3);
    draw.instances = instances;
    draw.model_view_projection[3] = 0;
    draw.base_color[0] = draw.base_color[1] = draw.base_color[2] = 0.8F;
    Require(surface->Acquire() == SurfaceStatus::Ready, "instance acquire failed");
    instances[0].scale[0] = 0;
    Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
            "zero instance scale accepted");
    instances[0].scale[0] = 0.4F;
    instances[0].translation[0] = std::numeric_limits<float>::infinity();
    Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
            "nonfinite instance accepted");
    instances[0].translation[0] = -0.5F;
    const std::vector<Presentation::SceneInstance> excessive(4097);
    auto invalidInstances = draw;
    invalidInstances.instances = excessive;
    Require(surface->DrawScene(invalidInstances) == SurfaceStatus::InvalidDescriptor,
            "unbounded instances accepted");
    Require(surface->DrawScene(draw) == SurfaceStatus::Ready,
            "native hardware instance draw failed");
    Require(surface->Present() == SurfaceStatus::Ready, "instance present failed");
    bool instancePixels = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!instancePixels && std::chrono::steady_clock::now() < deadline) {
      XSync(display, False);
      auto *image = XGetImage(display, native, 0, 0, width, height, AllPlanes, ZPixmap);
      Require(image != nullptr, "instance readback failed");
      const auto left = XGetPixel(image, width / 4, height / 2);
      const auto right = XGetPixel(image, width * 3 / 4, height / 2);
      instancePixels =
          Channel(left, image->red_mask) > 150 && Channel(left, image->green_mask) < 20 &&
          Channel(right, image->green_mask) > 150 && Channel(right, image->red_mask) < 20;
      XDestroyImage(image);
      if (!instancePixels)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    Require(instancePixels, "independent instance transform/tint pixels failed");
    const std::array<std::byte, 8> texels{std::byte{255}, std::byte{0},  std::byte{0},
                                          std::byte{255}, std::byte{0},  std::byte{255},
                                          std::byte{0},   std::byte{255}};
    std::array<Presentation::UiTextureUpload, 1> uploads{{{7, 2, 1, 8, texels}}};
    draw.instances = {};
    draw.textureId = 7;
    draw.textureUploads = uploads;
    for (unsigned materialFrame = 0; materialFrame < 3; ++materialFrame) {
      const bool green = materialFrame == 2;
      if (green) {
        width = 480;
        height = 360;
        Require(windows->Resize(created.handle, width, height) == Window::WindowError::None,
                "material resize failed");
        Require(surface->NotifyWindowExtent(width, height) == SurfaceStatus::Ready,
                "material extent failed");
      }
      static_cast<void>(windows->PumpEvents());
      for (auto &vertex : vertices) {
        vertex.uv[0] = green ? 0.75F : 0.25F;
        vertex.uv[1] = 0.5F;
      }
      Require(surface->Acquire() == SurfaceStatus::Ready, "material acquire failed");
      if (materialFrame == 0) {
        auto invalidTexture = draw;
        invalidTexture.textureId = 99;
        Require(surface->DrawScene(invalidTexture) == SurfaceStatus::InvalidDescriptor,
                "unknown material texture accepted");
        uploads[0].rowPitch = 4;
        Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
                "malformed material row pitch accepted");
        uploads[0].rowPitch = 8;
        uploads[0].width = 1025;
        Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
                "unbounded material extent accepted");
        uploads[0].width = 2;
        vertices[0].uv[0] = std::numeric_limits<float>::quiet_NaN();
        Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
                "nonfinite UV accepted");
        vertices[0].uv[0] = 0.25F;
      }
      const auto beforeUploads = surface->Diagnostics().sceneTextureUploads;
      Require(surface->DrawScene(draw) == SurfaceStatus::Ready, "native material draw failed");
      if (materialFrame == 1)
        Require(surface->Diagnostics().sceneTextureUploads == beforeUploads,
                "immutable material texture was reuploaded");
      Require(surface->Present() == SurfaceStatus::Ready, "material present failed");
      bool materialPixels = false;
      const auto materialDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
      while (!materialPixels && std::chrono::steady_clock::now() < materialDeadline) {
        XSync(display, False);
        auto *image = XGetImage(display, native, 0, 0, width, height, AllPlanes, ZPixmap);
        Require(image != nullptr, "material readback failed");
        const auto center = XGetPixel(image, width / 2, height / 2);
        const auto redChannel = Channel(center, image->red_mask);
        const auto greenChannel = Channel(center, image->green_mask);
        materialPixels =
            green ? greenChannel > 150 && redChannel < 20 : redChannel > 150 && greenChannel < 20;
        XDestroyImage(image);
        if (!materialPixels)
          std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
      Require(materialPixels, "UV texture sampling/resize pixels failed");
    }
    const auto diagnostics = surface->Diagnostics();
    Require(diagnostics.sceneDrawCalls == 16 && diagnostics.sceneInstances == 17 &&
                diagnostics.acquiredFrames == 16 && diagnostics.presentedFrames == 16 &&
                diagnostics.resizeGenerations == 3 && diagnostics.sceneTextureUploads == 5,
            "native scene counters or resize evidence mismatch");
    Require(surface->Acquire() == SurfaceStatus::Ready, "abandoned frame acquire failed");
    Require(surface->DrainAndDestroy() == SurfaceStatus::Ready, "scene teardown failed");
    Require(surface->DrawScene(draw) == SurfaceStatus::InvalidDescriptor,
            "draw after abandoned-frame teardown accepted");
    Require(surface->Acquire() == SurfaceStatus::SurfaceLost, "acquire after teardown accepted");
    Require(surface->DrainAndDestroy() == SurfaceStatus::Ready, "scene teardown not idempotent");
    surface.reset();
    XCloseDisplay(display);
    Require(windows->Destroy(created.handle) == Window::WindowError::None,
            "window teardown failed");
    std::cout << "PASS: 16 indexed draws including native instance and UV texture pixels, "
                 "depth-order invariance, lighting, matrix translation, "
                 "3 resize generations, immutable texture reuse, invalid-input containment and "
                 "ordered teardown\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
