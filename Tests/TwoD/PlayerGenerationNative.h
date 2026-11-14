#pragma once

#include "Nexora/TwoD/GameplayContent.h"
#include "Nexora/TwoD/GameplayGeneration.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
#if defined(NEXORA_PLAYER_RELOAD_NATIVE)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <chrono>
#include <cstring>
#include <thread>
#endif

namespace playerreload {
inline void Require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
class Native final {
public:
  using Pixels = std::vector<unsigned char>;
  Native(bool enabled, const std::filesystem::path &directory)
      : enabled_(enabled), root_(directory) {
    Require(std::filesystem::is_directory(root_), "Player reload native evidence directory");
    if (!enabled_)
      return;
#if defined(NEXORA_PLAYER_RELOAD_NATIVE)
    Nexora::Presentation::RenderSurfaceDescriptor descriptor;
    descriptor.title = "Nexora strict Player generations";
    descriptor.width = 640;
    descriptor.height = 480;
    descriptor.backend = Nexora::Presentation::SurfaceBackend::Vulkan;
    descriptor.srgbUiOutput = true;
    auto created = Nexora::Presentation::CreateRenderSurface(descriptor);
    Require(bool(created), "Player reload native surface");
    surface_ = std::move(created.surface);
    display_ = XOpenDisplay(nullptr);
    Require(display_ != nullptr, "Player reload native display");
    XSync(display_, False);
    ::Window root{}, parent{}, *children{};
    unsigned count{};
    Require(XQueryTree(display_, DefaultRootWindow(display_), &root, &parent, &children, &count),
            "Player reload native window tree");
    for (unsigned i = 0; i < count; ++i) {
      char *name{};
      if (XFetchName(display_, children[i], &name) && name) {
        if (std::strcmp(name, "Nexora strict Player generations") == 0)
          window_ = children[i];
        XFree(name);
      }
    }
    if (children)
      XFree(children);
    Require(window_ != 0, "Player reload native window identity");
#else
    throw std::runtime_error("Native reload acceptance was not built on this target");
#endif
  }
  ~Native() {
#if defined(NEXORA_PLAYER_RELOAD_NATIVE)
    if (surface_)
      static_cast<void>(surface_->DrainAndDestroy());
    if (display_)
      XCloseDisplay(display_);
#endif
  }
  Pixels Draw(nexora::twod::GameplayGeneration &values,
              const nexora::twod::GameplayContent &content) {
    if (!enabled_)
      return {};
#if defined(NEXORA_PLAYER_RELOAD_NATIVE)
    using namespace nexora;
    using Nexora::Presentation::SurfaceStatus;
    Require(surface_->BeginFrame() == SurfaceStatus::Ready, "Player reload acquire");
    std::vector<twod::SpriteInstance> instances;
    const auto poses = values.World().SceneWorldPoses(values.Sprites()->SceneId());
    Require(bool(poses), "Player reload native poses");
    for (const auto &pose : *poses)
      if (const auto *sprite = values.Sprites()->Find(pose.id))
        instances.push_back({pose.id, *sprite, pose.matrix});
    std::uint64_t identity = UINT64_MAX / 2;
    values.Pool().ForEach([&](const auto &instance) {
      instances.push_back({++identity, instance.sprite, runtime::ToMatrix(instance.transform)});
    });
    auto frame = twod::ExtractSprites(
        instances, values.Bridge().RenderCamera(640, 480),
        [&](auto id) { return content.ResolveAtlas(id); },
        [&](auto id) { return content.ResolveSprite(id); });
    Require(frame && frame->statistics.unavailable == 0 && frame->vertices.size() == 8,
            "Player reload source/pool extraction");
    // A distinct ten-bit red/blue marker acknowledges this exact present, so an old X11
    // front buffer cannot masquerade as a successful before/after image comparison.
    ++serial_;
    Require(serial_ < 1024, "Player reload native marker exhausted");
    const auto texture = frame->batches.front().textureId;
    const float u = (frame->vertices[0].uv[0] + frame->vertices[2].uv[0]) / 2;
    const float v = (frame->vertices[0].uv[1] + frame->vertices[2].uv[1]) / 2;
    for (unsigned bit = 0; bit < 10; ++bit) {
      const auto color = serial_ & (1U << bit) ? 0xFF0000FFU : 0xFFFF0000U;
      const auto base = static_cast<std::uint32_t>(frame->vertices.size());
      const auto first = static_cast<std::uint32_t>(frame->indices.size());
      const float x = float(bit * 4);
      for (const auto &point :
           {std::array{x, 0.F}, std::array{x + 4, 0.F}, std::array{x + 4, 4.F}, std::array{x, 4.F}})
        frame->vertices.push_back({{point[0], point[1]}, {u, v}, color});
      for (const auto index : {0U, 1U, 2U, 0U, 2U, 3U})
        frame->indices.push_back(base + index);
      Nexora::Presentation::UiDrawCommand command;
      command.clipWidth = 640;
      command.clipHeight = 480;
      command.textureId = texture;
      command.elementCount = 6;
      command.indexOffset = first;
      frame->batches.push_back(command);
    }
    Require(frame->Draw(*surface_) == SurfaceStatus::Ready &&
                surface_->EndFrame() == SurfaceStatus::Ready,
            "Player reload native draw/present");
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
      XSync(display_, False);
      auto *image = XGetImage(display_, window_, 0, 0, 640, 480, AllPlanes, ZPixmap);
      Require(image != nullptr, "Player reload native image");
      Pixels pixels(640 * 480 * 3);
      const auto channel = [](unsigned long value, unsigned long mask) {
        if (!mask)
          return 0UL;
        unsigned shift{};
        while (!(mask & 1)) {
          mask >>= 1;
          ++shift;
        }
        return ((value >> shift) & mask) * 255 / mask;
      };
      for (unsigned y = 0; y < 480; ++y)
        for (unsigned x = 0; x < 640; ++x) {
          const auto value = XGetPixel(image, x, y);
          const auto offset = (y * 640 + x) * 3;
          pixels[offset] = static_cast<unsigned char>(channel(value, image->red_mask));
          pixels[offset + 1] = static_cast<unsigned char>(channel(value, image->green_mask));
          pixels[offset + 2] = static_cast<unsigned char>(channel(value, image->blue_mask));
        }
      XDestroyImage(image);
      bool current = true;
      for (unsigned bit = 0; bit < 10; ++bit) {
        const auto offset = (2 * 640 + bit * 4 + 2) * 3;
        const bool red = (serial_ & (1U << bit)) != 0;
        current &= pixels[offset + (red ? 0 : 2)] > 240 && pixels[offset + (red ? 2 : 0)] < 10 &&
                   pixels[offset + 1] < 10;
      }
      if (current) {
        std::array<unsigned, 2> covered{};
        for (unsigned y = 8; y < 480; ++y)
          for (unsigned x = 0; x < 640; ++x) {
            const auto offset = (y * 640 + x) * 3;
            if (pixels[offset] > 240 && pixels[offset + 1] > 240 && pixels[offset + 2] > 240)
              ++covered[x < 320 ? 0 : 1];
          }
        Require(covered[0] > 500 && covered[1] > 500,
                "Player reload source or pooled Sprite is blank");
        std::ofstream file(root_ / ("frame-" + std::to_string(serial_) + ".ppm"), std::ios::binary);
        file << "P6\n640 480\n255\n";
        file.write(reinterpret_cast<const char *>(pixels.data()),
                   static_cast<std::streamsize>(pixels.size()));
        Require(file.good(), "Player reload pixel evidence write");
        // Exclude only the acknowledged marker rows from the complete image comparison.
        pixels.erase(pixels.begin(), pixels.begin() + 8 * 640 * 3);
        return pixels;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    throw std::runtime_error("Player reload present marker was not observed");
#else
    static_cast<void>(values);
    static_cast<void>(content);
    return {};
#endif
  }
  void Finish() {
#if defined(NEXORA_PLAYER_RELOAD_NATIVE)
    if (surface_) {
      Require(surface_->Diagnostics().presentedFrames == serial_,
              "Player reload native presentation count");
      Require(surface_->DrainAndDestroy() == Nexora::Presentation::SurfaceStatus::Ready,
              "Player reload native drain");
      surface_.reset();
    }
#endif
  }
  unsigned Frames() const noexcept { return serial_; }

private:
  bool enabled_;
  std::filesystem::path root_;
  unsigned serial_{};
#if defined(NEXORA_PLAYER_RELOAD_NATIVE)
  std::unique_ptr<Nexora::Presentation::RenderSurface> surface_;
  Display *display_{};
  ::Window window_{};
#endif
};
} // namespace playerreload
