#pragma once

#include "GameplayPlayer.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "StaticView.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace nexora::player {

struct NativeOptions final {
  Nexora::Presentation::SurfaceBackend backend{Nexora::Presentation::SurfaceBackend::Automatic};
  std::filesystem::path save_directory;
  std::filesystem::path
      gameplay_module;            // Explicit trusted executable code; never package discovery.
  std::uint32_t maximum_frames{}; // Zero runs until a real close request.
};

inline int RunNative(const runtime::LoadedStaticProject &project, NativeOptions options) {
  using namespace Nexora::Presentation;
  std::string error;
  auto view = PrepareStaticView(project, &error);
  if (!view) {
    std::cerr << "Native StaticView admission failed: " << error << '\n';
    return 1;
  }
  PlayerGameplay gameplay;
  runtime::GameplayModuleHost module(gameplay.Host());
  const bool has_gameplay = !options.gameplay_module.empty();
  if (has_gameplay && (!gameplay.Initialize(project, options.save_directory) ||
                       !module.Load(options.gameplay_module))) {
    std::cerr << "ProjectPlayer gameplay initialization failed\n";
    return 1;
  }
  const auto created = CreateRenderSurface(
      {.title = "Nexora Project Player", .width = 960, .height = 640, .backend = options.backend});
  if (!created) {
    std::cerr << "Native ProjectPlayer unavailable: " << created.reason << '\n';
    return 1;
  }
  struct UnloadBeforeSurface final {
    runtime::GameplayModuleHost &module;
    ~UnloadBeforeSurface() { module.Unload(); }
  } unload_before_surface{module};
  auto &surface = *created.surface;
  std::uint64_t frames{};
  bool failed{};
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  std::uint64_t reported_resize = UINT64_MAX;
  auto previous_tick = std::chrono::steady_clock::now();
  double accumulator{}, audio_accumulator{};
  constexpr double fixed_delta = 1.0 / 60.0;
  while (!options.maximum_frames || frames < options.maximum_frames) {
    if (options.maximum_frames && std::chrono::steady_clock::now() > deadline) {
      std::cerr << "Native ProjectPlayer stalled beyond its bounded run deadline\n";
      failed = true;
      break;
    }
    const auto acquired = surface.BeginFrame();
    if (surface.CloseRequested())
      break;
    // BeginFrame owns a fresh event batch even when presentation is suspended.
    // Consume focus/release events before a retry can replace that batch.
    if (has_gameplay)
      gameplay.CaptureNativeInput(surface);
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
    if (has_gameplay) {
      const auto now = std::chrono::steady_clock::now();
      const auto delta = std::min(std::chrono::duration<double>(now - previous_tick).count(), 0.25);
      previous_tick = now;
      accumulator += delta;
      while (accumulator >= fixed_delta) {
        if (module.SupportsFixedUpdate()) {
          if (!module.FixedUpdate(fixed_delta)) {
            failed = true;
            break;
          }
          ++gameplay.fixed_updates;
        }
        accumulator -= fixed_delta;
      }
      if (failed || !module.Update(delta)) {
        std::cerr << "ProjectPlayer gameplay callback failed\n";
        failed = true;
        break;
      }
      ++gameplay.updates;
      // Output is optional; the service reports unsupported when a device cannot be opened.
      audio_accumulator += delta * 48000.0;
      while (audio_accumulator >= 1) {
        const auto samples = static_cast<std::uint32_t>(std::min(audio_accumulator, 4096.0));
        if (!gameplay.audio.Pump(samples)) {
          audio_accumulator = 0;
          break;
        }
        audio_accumulator -= samples;
      }
      gameplay.world.EndFrame();
      if (!view->RefreshWorld(project, gameplay.world.InternalWorld())) {
        std::cerr << "ProjectPlayer gameplay invalidated admitted StaticView objects\n";
        failed = true;
        break;
      }
    }
    const auto extent = surface.FrameInfo();
    const auto &draw_world = has_gameplay ? gameplay.world.InternalWorld() : project.WorldView();
    const auto draw = view->DrawData(draw_world, static_cast<float>(extent.width) /
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
  module.Unload(); // Stop/destroy code before draining or destroying native resources.
  const auto observed = surface.Diagnostics();
  const auto drained = surface.DrainAndDestroy();
  if (drained != SurfaceStatus::Ready || failed || !observed.presentedFrames ||
      !observed.sceneDrawCalls) {
    std::cerr
        << "Native ProjectPlayer did not complete a drawn presentation and successful drain\n";
    return 1;
  }
  std::cout << "{\"status\":\"RENDERED_STATIC_VIEW\",\"native_rendering\":true,"
               "\"gameplay_loaded\":"
            << (has_gameplay ? "true" : "false") << ",\"gameplay_updates\":" << gameplay.updates
            << ",\"gameplay_fixed_updates\":" << gameplay.fixed_updates
            << ",\"requested_backend\":\"" << ToString(options.backend)
            << "\",\"render_instances\":" << view->instances.size() << ",\"camera\":\""
            << view->camera << "\",\"light\":\"" << view->light
            << "\",\"presented_frames\":" << observed.presentedFrames
            << ",\"scene_draw_calls\":" << observed.sceneDrawCalls
            << ",\"software_rasterizer\":" << (observed.softwareRasterizer ? "true" : "false")
            << ",\"resize_generations\":" << observed.resizeGenerations
            << ",\"inactive_components\":" << project.InactiveComponentCount()
            << ",\"drained\":true}\n";
  return 0;
}
} // namespace nexora::player
