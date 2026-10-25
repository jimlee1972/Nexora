#pragma once
#include "NativePlayer.h"
#include "Nexora/Runtime/GameSceneSession.h"

namespace nexora::player {
// CPU palette spans are borrowed only by DrawScene. The surface copies uploads into its own
// fence-owned frame resources, which remain alive across palette swaps and drain on teardown.
struct PreparedGameView final : runtime::GameScenePresentation {
  std::shared_ptr<StaticView> candidate;
  std::shared_ptr<StaticView> &published;
  PreparedGameView(std::shared_ptr<StaticView> view, std::shared_ptr<StaticView> &active)
      : candidate(std::move(view)), published(active) {}
  void Commit() noexcept override { published.swap(candidate); }
};

inline int RunNativeGame(std::shared_ptr<const runtime::LoadedGameProject> project,
                         NativeOptions options) {
  using namespace Nexora::Presentation;
  PlayerGameplay gameplay;
  if (!gameplay.InitializeServices(project->ProjectId(), options.save_directory)) {
    std::cerr << "Game services initialization failed\n";
    return 1;
  }
  runtime::GameSceneSession session(gameplay.world, project);
  gameplay.services.scene = session.Api();
  runtime::GameplayModuleHost module(gameplay.Host());
  std::uint64_t startup{};
  if (session.RequestLoad(project->StartupScene(), runtime::GameSceneLoadMode::Replace, false,
                          startup) != runtime::GameSceneError::None) {
    std::cerr << "Configured game startup scene could not be queued\n";
    return 1;
  }
  const auto created = CreateRenderSurface(
      {.title = "Nexora Game", .width = 960, .height = 640, .backend = options.backend});
  if (!created) {
    std::cerr << "Native game unavailable: " << created.reason << '\n';
    return 1;
  }
  struct Shutdown final {
    runtime::GameSceneSession &session;
    runtime::GameplayModuleHost &module;
    ~Shutdown() {
      session.Stop();
      module.Unload();
    }
  } shutdown{session, module};
  auto &surface = *created.surface;
  std::shared_ptr<StaticView> published;
  bool started{}, failed{}, module_loaded{};
  std::uint64_t frames{}, transitions{}, rejected{};
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  auto previous_tick = std::chrono::steady_clock::now();
  double accumulator{}, audio_accumulator{};
  constexpr double fixed_delta = 1.0 / 60.0;
  while (!options.maximum_frames || frames < options.maximum_frames) {
    if (options.maximum_frames && std::chrono::steady_clock::now() > deadline) {
      std::cerr << "Native game stalled beyond its bounded run deadline\n";
      failed = true;
      break;
    }
    const auto acquired = surface.BeginFrame();
    if (surface.CloseRequested())
      break;
    if (module_loaded)
      gameplay.CaptureNativeInput(surface);
    const auto action = RecoveryAction(acquired);
    if (action == SurfaceAction::Suspend || action == SurfaceAction::RecreateSurface) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    if (action != SurfaceAction::Render) {
      std::cerr << "Native game acquire failed: " << ToString(acquired) << '\n';
      failed = true;
      break;
    }
    if (module_loaded) {
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
        std::cerr << "Game module callback failed\n";
        failed = true;
        break;
      }
      ++gameplay.updates;
      audio_accumulator += delta * 48000.;
      while (audio_accumulator >= 1) {
        const auto count = static_cast<std::uint32_t>(std::min(audio_accumulator, 4096.));
        if (!gameplay.audio.Pump(count)) {
          audio_accumulator = 0;
          break;
        }
        audio_accumulator -= count;
      }
    }
    gameplay.world.EndFrame();
    const auto extent = surface.FrameInfo();
    const auto aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    bool drawn{}, native_failed{};
    const auto processed =
        session.Process([&](const runtime::GameSceneView &view,
                            std::string &error) -> std::unique_ptr<runtime::GameScenePresentation> {
          const auto items = runtime::CaptureGameSceneRenderItems(view, &error);
          if (!items)
            return {};
          auto palette = PrepareSceneView(*items, view.world, view.primary, &error, true);
          if (!palette)
            return {};
          auto staged = std::make_shared<StaticView>(std::move(*palette));
          auto prepared = std::make_unique<PreparedGameView>(staged, published);
          auto data = staged->DrawData(view.world, aspect);
          if (!data) {
            error = "Candidate camera projection failed.";
            return {};
          }
          data->retainMeshGeometry = surface.SupportsSceneGeometryResidency();
          const auto result =
              staged->instances.empty() ? SurfaceStatus::Ready : surface.DrawScene(*data);
          if (result != SurfaceStatus::Ready) {
            error = "Candidate native draw preparation failed: ";
            error.append(ToString(result));
            native_failed = true;
            return {};
          }
          drawn = true;
          return prepared;
        });
    if (!processed) {
      ++rejected;
      std::cerr << "Game scene request failed; live World retained: " << session.ErrorText()
                << '\n';
      if (!published || native_failed) {
        failed = true;
        break;
      }
    }
    if (processed && drawn) {
      ++transitions;
      const auto instance = std::ranges::find(session.View().instances, session.View().primary,
                                              &runtime::GameSceneInstance::scene);
      std::cerr << "game scene committed: " << instance->asset.ToString() << ' '
                << session.View().primary << ' ' << session.View().camera << '\n';
    }
    if (!drawn) {
      std::string error;
      const auto items = runtime::CaptureGameSceneRenderItems(session.View(), &error);
      auto refreshed = items ? PrepareSceneView(*items, gameplay.world.InternalWorld(),
                                                session.View().primary, &error, true)
                             : std::nullopt;
      if (!refreshed) {
        std::cerr << "Live game render admission failed: " << error << '\n';
        failed = true;
        break;
      }
      auto data = refreshed->DrawData(gameplay.world.InternalWorld(), aspect);
      if (data)
        data->retainMeshGeometry = surface.SupportsSceneGeometryResidency();
      if (!data ||
          (!refreshed->instances.empty() && surface.DrawScene(*data) != SurfaceStatus::Ready)) {
        std::cerr << "Live game draw failed\n";
        failed = true;
        break;
      }
    }
    const auto presented = surface.EndFrame();
    if (presented == SurfaceStatus::Ready) {
      ++frames;
      deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
      if (!started) {
        started = true;
        if (!options.gameplay_module.empty()) {
          if (!module.Load(options.gameplay_module)) {
            std::cerr << "Game module initialization failed\n";
            failed = true;
            break;
          }
          module_loaded = true;
          previous_tick = std::chrono::steady_clock::now();
        }
      }
    } else if (RecoveryAction(presented) != SurfaceAction::RecreateSurface &&
               RecoveryAction(presented) != SurfaceAction::Suspend) {
      std::cerr << "Native game presentation failed: " << ToString(presented) << '\n';
      failed = true;
      break;
    }
  }
  session.Stop();
  module.Unload();
  const auto observed = surface.Diagnostics();
  const auto drained = surface.DrainAndDestroy();
  if (failed || !frames || drained != SurfaceStatus::Ready)
    return 1;
  const auto stats = session.Stats();
  const auto current = session.View();
  const auto primary =
      std::ranges::find(current.instances, current.primary, &runtime::GameSceneInstance::scene);
  std::cout << "{\"status\":\"RENDERED_GAME_PACKAGE\",\"native_rendering\":true,"
               "\"start_scene\":\""
            << project->StartupScene().ToString() << "\",\"active_scene\":\""
            << primary->asset.ToString() << "\",\"camera\":\"" << current.camera
            << "\",\"gameplay_loaded\":" << (module_loaded ? "true" : "false")
            << ",\"gameplay_updates\":" << gameplay.updates << ",\"scene_commits\":" << transitions
            << ",\"rejected_requests\":" << rejected << ",\"scene_records\":" << stats.scene_records
            << ",\"instances\":" << stats.instances
            << ",\"stored_entities\":" << stats.stored_entities
            << ",\"presented_frames\":" << observed.presentedFrames
            << ",\"scene_draw_calls\":" << observed.sceneDrawCalls
            << ",\"geometry_residency_available\":"
            << (observed.sceneGeometryResidency.available ? "true" : "false")
            << ",\"geometry_uploads\":" << observed.sceneGeometryResidency.uploads
            << ",\"geometry_hits\":" << observed.sceneGeometryResidency.hits
            << ",\"geometry_native_bytes\":"
            << observed.sceneGeometryResidency.nativeAllocationBytes
            << ",\"geometry_cpu_bytes\":" << observed.sceneGeometryResidency.retainedCpuBytes
            << ",\"scene_texture_uploads\":" << observed.sceneTextureUploads
            << ",\"software_rasterizer\":" << (observed.softwareRasterizer ? "true" : "false")
            << ",\"drained\":true}\n";
  std::cout.flush();
  if (!std::cout) {
    std::cerr << "Native game report output failed\n";
    return 1;
  }
  return 0;
}
} // namespace nexora::player
