#pragma once

#include "Nexora/Presentation/RenderSurface.h"
#include "StaticView.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace nexora::player {

struct NativeOptions final {
  Nexora::Presentation::SurfaceBackend backend{Nexora::Presentation::SurfaceBackend::Automatic};
  std::uint32_t maximum_frames{}; // Zero runs until a real close request.
};

inline int RunNative(const runtime::LoadedStaticProject &project, NativeOptions options) {
  using namespace Nexora::Presentation;
  std::string error;
  const auto view = PrepareStaticView(project, &error);
  if (!view) {
    std::cerr << "Native StaticView admission failed: " << error << '\n';
    return 1;
  }
  const auto created = CreateRenderSurface(
      {.title = "Nexora Project Player", .width = 960, .height = 640, .backend = options.backend});
  if (!created) {
    std::cerr << "Native ProjectPlayer unavailable: " << created.reason << '\n';
    return 1;
  }
  auto &surface = *created.surface;
  std::uint64_t frames{};
  bool failed{};
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  std::uint64_t reported_resize = UINT64_MAX;
  while (!options.maximum_frames || frames < options.maximum_frames) {
    if (options.maximum_frames && std::chrono::steady_clock::now() > deadline) {
      std::cerr << "Native ProjectPlayer stalled beyond its bounded run deadline\n";
      failed = true;
      break;
    }
    const auto acquired = surface.BeginFrame();
    if (surface.CloseRequested())
      break;
    const auto action = RecoveryAction(acquired);
    if (action == SurfaceAction::Suspend || action == SurfaceAction::RecreateSurface) {
      // Acquisition owns surface recreation. Device loss never retries a dead device.
      if (options.maximum_frames && std::chrono::steady_clock::now() > deadline) {
        std::cerr << "Native ProjectPlayer did not present within its bounded run deadline\n";
        failed = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    if (action != SurfaceAction::Render) {
      std::cerr << "Native ProjectPlayer acquire failed: " << ToString(acquired) << '\n';
      failed = true;
      break;
    }
    const auto extent = surface.FrameInfo();
    const auto draw = view->DrawData(project, static_cast<float>(extent.width) /
                                                  static_cast<float>(extent.height));
    if (!draw) {
      std::cerr << "Native ProjectPlayer camera projection failed after resize\n";
      failed = true;
      break;
    }
    const auto drawn = surface.DrawScene(*draw);
    if (drawn != SurfaceStatus::Ready) {
      std::cerr << "Native ProjectPlayer draw failed: " << ToString(drawn) << '\n';
      failed = true;
      break;
    }
    const auto presented = surface.EndFrame();
    if (presented == SurfaceStatus::Ready) {
      ++frames;
      deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
      const auto diagnostics = surface.Diagnostics();
      if (reported_resize != diagnostics.resizeGenerations) {
        reported_resize = diagnostics.resizeGenerations;
        std::cerr << "native project frame: " << extent.width << ' ' << extent.height << ' '
                  << diagnostics.sceneDrawCalls << ' ' << diagnostics.presentedFrames << ' '
                  << diagnostics.resizeGenerations << '\n';
      }
    } else if (RecoveryAction(presented) != SurfaceAction::RecreateSurface &&
               RecoveryAction(presented) != SurfaceAction::Suspend) {
      std::cerr << "Native ProjectPlayer present failed: " << ToString(presented) << '\n';
      failed = true;
      break;
    }
  }
  const auto observed = surface.Diagnostics();
  const auto drained = surface.DrainAndDestroy();
  if (drained != SurfaceStatus::Ready || failed || !observed.presentedFrames ||
      !observed.sceneDrawCalls) {
    std::cerr
        << "Native ProjectPlayer did not complete a drawn presentation and successful drain\n";
    return 1;
  }
  std::cout << "{\"status\":\"RENDERED_STATIC_VIEW\",\"native_rendering\":true,"
               "\"gameplay_loaded\":false,\"requested_backend\":\""
            << ToString(options.backend) << "\",\"render_instances\":" << view->instances.size()
            << ",\"camera\":\"" << view->camera << "\",\"light\":\"" << view->light
            << "\",\"presented_frames\":" << observed.presentedFrames
            << ",\"scene_draw_calls\":" << observed.sceneDrawCalls
            << ",\"software_rasterizer\":" << (observed.softwareRasterizer ? "true" : "false")
            << ",\"resize_generations\":" << observed.resizeGenerations
            << ",\"inactive_components\":" << project.InactiveComponentCount()
            << ",\"drained\":true}\n";
  return 0;
}
} // namespace nexora::player
