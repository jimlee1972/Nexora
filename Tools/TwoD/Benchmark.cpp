#include "Nexora/Core/Engine.h"
#include "Nexora/Core/JobSystem.h"
#include "Nexora/Core/Vfs.h"
#include "Nexora/TwoD/Lifecycle.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <set>
#include <stdexcept>
#include <thread>
#if defined(__linux__)
#include <unistd.h>
#endif
using namespace nexora;
namespace {
void Check(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
using Clock = std::chrono::steady_clock;
double Milliseconds(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
double Percentile(std::vector<double> values, double p) {
  if (values.empty())
    return 0;
  std::sort(values.begin(), values.end());
  return values[std::size_t(std::ceil(p * double(values.size()))) - 1];
}
std::size_t ResidentBytes() {
#if defined(__linux__)
  std::ifstream f("/proc/self/statm");
  std::size_t total{}, resident{};
  if (f >> total >> resident)
    return resident * std::size_t(sysconf(_SC_PAGESIZE));
#endif
  return 0;
}
runtime::RuntimeBlob LoadCooked(const std::filesystem::path &path) {
  const auto absolute = std::filesystem::absolute(path);
  core::Engine engine;
  core::EngineConfiguration config;
  config.worker_count = 2;
  config.content_root = absolute.parent_path();
  engine.Initialize(config);
  const auto n = std::filesystem::file_size(absolute);
  Check(n <= 64 * 1024 * 1024, "cooked file budget");
  core::AsyncReadOptions options;
  options.size = n;
  const auto handle = engine.Services().vfs->ReadAsync(
      "content://" + absolute.filename().generic_string(), options);
  const auto deadline = Clock::now() + std::chrono::seconds(10);
  core::ReadResult result;
  do {
    result = handle.Get();
    if (result.status != core::ReadResult::Status::Pending)
      break;
    Check(Clock::now() < deadline, "async load deadline");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (true);
  Check(result.status == core::ReadResult::Status::Completed, "async cooked read");
  auto blob = runtime::AssetCooker::Deserialize(result.bytes);
  Check(bool(blob), "invalid cooked asset");
  engine.Shutdown();
  return std::move(*blob);
}

void Measure(std::ostream &report, std::size_t count, bool overlap, bool native, unsigned frames,
             const std::optional<twod::Atlas> &sourceAtlas,
             const std::optional<twod::Clip> &sourceClip, bool sparse,
             Nexora::Presentation::SurfaceBackend backend, std::ostream *frameTrace) {
  const auto loading = Clock::now();
  twod::Image image{1, 8, 8, twod::Bytes(256, std::byte{255})};
  auto atlas = twod::PackAtlas({9, 17}, std::span(&image, 1), 32, 32, true);
  if (sourceAtlas)
    atlas = sourceAtlas;
  Check(bool(atlas), "atlas");
  auto shared = std::make_shared<const twod::Atlas>(*atlas);
  twod::Sprite sprite;
  sprite.atlas = atlas->id;
  sprite.region = atlas->regions.front().id;
  sprite.size = overlap ? math::Vector2{2, 2} : math::Vector2{0.15F, 0.15F};
  sprite.color = {0.2F, 0.7F, 1.0F, overlap ? 0.05F : 1.0F};
  twod::InstancePool pool(count);
  std::vector<twod::Instance> instances(count);
  for (std::size_t i = 0; i < count; ++i) {
    instances[i].sprite = sprite;
    instances[i].transform.x = overlap ? 0 : double(i % 100) * (sparse ? 2.0 : 0.25) - 12;
    instances[i].transform.y = overlap ? 0 : double(i / 100) * (sparse ? 2.0 : 0.25) - 12;
  }
  auto handles = pool.Create(instances);
  Check(bool(handles), "bulk create");
  Nexora::Presentation::RenderSurfaceResult surface;
  if (native) {
    Nexora::Presentation::RenderSurfaceDescriptor d;
    d.title = "Nexora 2D mixed stress";
    d.width = 640;
    d.height = 480;
    d.enableGpuTiming = true;
    d.backend = backend;
    surface = Nexora::Presentation::CreateRenderSurface(d);
    if (!surface)
      throw std::runtime_error("native stress surface: " + surface.reason);
  }
  const auto device =
      native ? surface.surface->Diagnostics().device : Nexora::Presentation::SurfaceDeviceInfo{};
  const auto loadMs = Milliseconds(loading);
  twod::FixedStepper step;
  twod::SpatialIndex index(2);
  twod::Animator animation;
  Check(animation.Play(sourceClip
                           ? *sourceClip
                           : twod::Clip{{18, 19}, atlas->id, true, {{sprite.region, 0.1, "tick"}}}),
        "animation");
  twod::Camera camera;
  camera.width = 640;
  camera.height = 480;
  camera.verticalSize = 24;
  std::vector<double> cpu, gpu, whole;
  std::uint64_t lastGpu{}, steps{};
  std::size_t queries{}, visible{}, culled{}, draws{}, peak = ResidentBytes();
  double coverage{}, reloadMs{};
  std::uint64_t texture = 201;
  constexpr unsigned warmup = 4;
  struct FrameSample {
    unsigned frame;
    double cpu{}, whole{};
    std::array<Clock::time_point, 9> points{};
  };
  std::vector<FrameSample> samples;
  if (frameTrace)
    samples.reserve(frames + warmup);
  for (unsigned frame = 0; frame < frames + warmup; ++frame) {
    const auto begin = Clock::now();
    FrameSample sample{frame};
    sample.points[0] = begin;
    const auto stamp = [&](unsigned point) {
      if (frameTrace)
        sample.points[point] = Clock::now();
    };
    if (frame == warmup + frames / 2) {
      const auto reloadStart = Clock::now();
      Check(pool.Destroy(*handles), "bulk recycle");
      Check(!pool.Find(handles->front()), "stale recycled handle");
      handles = pool.Create(instances);
      Check(bool(handles), "bulk recreate");
      if (native)
        Check(surface.surface->ReleaseSpriteTextures(std::span(&texture, 1)) ==
                  Nexora::Presentation::SurfaceStatus::Ready,
              "retire old atlas generation");
      ++texture;
      shared = std::make_shared<const twod::Atlas>(*atlas);
      reloadMs = Milliseconds(reloadStart);
    }
    stamp(1);
    if (native)
      Check(surface.surface->BeginFrame() == Nexora::Presentation::SurfaceStatus::Ready,
            "stress acquire");
    const auto update = Clock::now();
    if (frameTrace)
      sample.points[2] = update;
    auto tick = step.Advance(1.0 / 60);
    Check(tick && tick->steps == 1, "fixed tick");
    steps += tick->steps;
    const auto animationUpdate = animation.Advance(step.StepSeconds());
    Check(bool(animationUpdate), "animation tick");
    std::vector<twod::SpriteInstance> sprites;
    std::vector<twod::Shape> shapes;
    sprites.reserve(count);
    shapes.reserve(count);
    std::uint64_t id = 0;
    pool.ForEach([&](auto &instance) {
      instance.transform.x += 0.0001;
      instance.sprite.region = animationUpdate->region;
      const auto identity = ++id;
      sprites.push_back({identity, instance.sprite, runtime::ToMatrix(instance.transform)});
      twod::Shape s;
      s.id = identity;
      s.center = {float(instance.transform.x), float(instance.transform.y)};
      s.radius = 0.05F;
      shapes.push_back(s);
    });
    stamp(3);
    Check(index.Build(shapes), "mixed query build");
    stamp(4);
    twod::Shape query;
    query.radius = overlap ? 0.1F : 2.0F;
    queries += index.Query(query).size();
    static_cast<void>(index.Sweep({-1, 0}, {1, 0}, 0.05F));
    stamp(5);
    auto extracted =
        twod::ExtractSprites(sprites, camera, [&](auto) -> std::optional<twod::AtlasBinding> {
          return twod::AtlasBinding{shared, texture};
        });
    Check(bool(extracted), "extract mixed sprites");
    visible = extracted->statistics.visible;
    culled = extracted->statistics.culled;
    coverage = extracted->statistics.coveredPixels / (camera.width * camera.height);
    draws = extracted->statistics.batches;
    if (frame >= warmup || frameTrace) {
      sample.cpu = Milliseconds(update);
      if (frame >= warmup)
        cpu.push_back(sample.cpu);
    }
    stamp(6);
    if (native)
      Check(extracted->Draw(*surface.surface) == Nexora::Presentation::SurfaceStatus::Ready,
            "stress draw");
    stamp(7);
    if (native) {
      Check(surface.surface->EndFrame() == Nexora::Presentation::SurfaceStatus::Ready,
            "stress present");
      const auto timing = surface.surface->Diagnostics().gpuTiming;
      if (timing.milliseconds && timing.completedSubmission > warmup &&
          timing.completedSubmission != lastGpu) {
        lastGpu = timing.completedSubmission;
        gpu.push_back(*timing.milliseconds);
      }
    }
    if (frame >= warmup || frameTrace) {
      sample.points[8] = Clock::now();
      sample.whole = std::chrono::duration<double, std::milli>(sample.points[8] - begin).count();
      if (frame >= warmup)
        whole.push_back(sample.whole);
      if (frameTrace)
        samples.push_back(sample);
    }
    peak = std::max(peak, ResidentBytes());
  }
  Check(steps == frames + warmup && pool.Size() == count, "mixed lifetime state");
  if (native) {
    Check(surface.surface->Diagnostics().nativeUiTextureUploads == 3, "stress reuploads atlas");
    Check(surface.surface->DrainAndDestroy() == Nexora::Presentation::SurfaceStatus::Ready,
          "stress drain");
  }
  // Buffer every sample; no file output is part of a measured frame.
  if (frameTrace) {
    *frameTrace << std::setprecision(17);
    for (const auto &sample : samples) {
      *frameTrace << count << ',' << sparse << ',' << overlap << ',' << native << ','
                  << sample.frame << ',' << (sample.frame < warmup) << ',' << sample.cpu << ','
                  << sample.whole;
      for (unsigned point = 0; point < 8; ++point)
        *frameTrace << ','
                    << std::chrono::duration<double, std::milli>(sample.points[point + 1] -
                                                                 sample.points[point])
                           .count();
      *frameTrace << '\n';
    }
  }
  report << count << ',' << sparse << ',' << reloadMs << ',' << overlap << ',' << native << ','
         << frames << ',' << warmup << ',' << loadMs << ',' << Percentile(cpu, 0.5) << ','
         << Percentile(cpu, 0.95) << ',' << Percentile(cpu, 0.99) << ','
         << *std::max_element(cpu.begin(), cpu.end()) << ',' << Percentile(whole, 0.95) << ','
         << gpu.size() << ',';
  if (!gpu.empty())
    report << Percentile(gpu, 0.95);
  report << ',' << peak << ',' << visible << ',' << culled << ',' << draws << ',' << coverage << ','
         << queries << ','
         << std::accumulate(whole.begin(), whole.end(), 0.0) / double(whole.size()) << ','
         << Percentile(whole, 0.99) << ',' << *std::max_element(whole.begin(), whole.end()) << ','
         << std::quoted(std::string(device.name.data()), '"', '"') << ',' << device.vendorId << ','
         << device.deviceId << ',' << device.driverVersion << ','
         << static_cast<unsigned>(device.driverVersionFormat) << ','
         << (native ? Nexora::Presentation::ToString(backend) : "none") << '\n';
}
} // namespace
int main(int argc, char **argv) {
  try {
    std::string reportPath, atlasPath, clipPath, frameTracePath;
    bool native = false;
    auto backend = Nexora::Presentation::SurfaceBackend::Automatic;
    unsigned frames = 48;
    for (int i = 1; i < argc; ++i) {
      std::string_view a = argv[i];
      if (a == "--native")
        native = true;
      else if (a == "--backend" && i + 1 < argc) {
        const std::string_view value(argv[++i]);
        if (value == "auto")
          backend = Nexora::Presentation::SurfaceBackend::Automatic;
        else if (value == "dx12")
          backend = Nexora::Presentation::SurfaceBackend::Dx12;
        else if (value == "vulkan")
          backend = Nexora::Presentation::SurfaceBackend::Vulkan;
        else if (value == "metal")
          backend = Nexora::Presentation::SurfaceBackend::Metal;
        else
          throw std::runtime_error("backend must be auto, dx12, vulkan or metal");
      } else if (a == "--atlas" && i + 1 < argc)
        atlasPath = argv[++i];
      else if (a == "--clip" && i + 1 < argc)
        clipPath = argv[++i];
      else if (a == "--report" && i + 1 < argc)
        reportPath = argv[++i];
      else if (a == "--frame-trace" && i + 1 < argc)
        frameTracePath = argv[++i];
      else if (a == "--frames" && i + 1 < argc) {
        std::string_view n = argv[++i];
        auto r = std::from_chars(n.data(), n.data() + n.size(), frames);
        Check(r.ec == std::errc{} && r.ptr == n.data() + n.size() && frames >= 4 && frames <= 600,
              "frames 4..600 required");
      } else
        throw std::runtime_error(
            "usage: NexoraTwoDBenchmark [--native --backend auto|dx12|vulkan|metal --frames 48] "
            "--report FILE.csv [--frame-trace FRAMES.csv]");
    }
    Check(native || backend == Nexora::Presentation::SurfaceBackend::Automatic,
          "explicit backend requires native presentation");
    Check(!reportPath.empty(), "report required");
    std::optional<twod::Atlas> loadedAtlas;
    std::optional<twod::Clip> loadedClip;
    if (!atlasPath.empty()) {
      auto atlasBlob = LoadCooked(atlasPath);
      Check(atlasBlob.type == "sprite_atlas", "atlas type");
      loadedAtlas = twod::DecodeAtlas(atlasBlob.payload);
      Check(loadedAtlas && loadedAtlas->id == atlasBlob.id, "cooked atlas identity");
      std::vector<runtime::RuntimeBlob> blobs{atlasBlob};
      if (!clipPath.empty()) {
        auto clipBlob = LoadCooked(clipPath);
        Check(clipBlob.type == "sprite_clip", "clip type");
        loadedClip = twod::DecodeClip(clipBlob.payload);
        Check(loadedClip && loadedClip->id == clipBlob.id && loadedClip->atlas == loadedAtlas->id,
              "cooked clip identity");
        for (const auto &f : loadedClip->frames)
          Check(loadedAtlas->Find(f.region) != nullptr, "cooked clip region");
        blobs.push_back(std::move(clipBlob));
      }
      auto bundle = runtime::BundleBuilder::Build("2D packaged validation", 1, {}, blobs);
      Check(bundle && runtime::BundleBuilder::Verify(*bundle), "cooked bundle");
      runtime::AssetGenerationStore store;
      Check(store.Stage({*bundle}) && store.ActivateStaged(), "generation publication");
      Check(store.Load(loadedAtlas->id) != nullptr, "resident cooked atlas");
      store.Release(loadedAtlas->id);
      Check(store.ResidentBytes() == 0, "cooked ownership release");

    } else
      Check(clipPath.empty(), "clip requires atlas");
    std::ofstream report(reportPath);
    Check(bool(report), "report output");
    std::ofstream frameTrace;
    if (!frameTracePath.empty()) {
      std::error_code error;
      Check(!std::filesystem::equivalent(reportPath, frameTracePath, error),
            "frame trace must differ from report");
      frameTrace.open(frameTracePath);
      Check(bool(frameTrace), "frame trace output");
      frameTrace
          << "objects,sparse,overlap,native,frame,warmup,cpu_ms,frame_ms,reload_ms,"
             "begin_frame_ms,populate_ms,index_ms,query_ms,extract_ms,draw_ms,end_frame_ms\n";
    }
    auto *trace = frameTrace.is_open() ? &frameTrace : nullptr;
    report << "objects,sparse,reload_ms,overlap,native,frames,warmup_frames,loading_ms,cpu_p50_ms,"
              "cpu_p95_ms,cpu_"
              "p99_ms,cpu_max_"
              "ms,frame_p95_ms,gpu_samples,gpu_p95_ms,peak_process_rss_bytes,visible,culled,sprite_"
              "batches,covered_aabb_ratio,query_hits,frame_average_ms,frame_p99_ms,frame_max_ms,"
              "device_name,vendor_id,device_id,driver_version,driver_version_format,backend_"
              "requested\n";
    for (auto count : {100U, 1000U, 5000U, 10000U})
      Measure(report, count, false, native, frames, loadedAtlas, loadedClip, false, backend, trace);
    Measure(report, 1000, true, native, frames, loadedAtlas, loadedClip, false, backend, trace);
    Measure(report, 10000, false, native, frames, loadedAtlas, loadedClip, true, backend, trace);
    if (trace) {
      frameTrace.flush();
      Check(bool(frameTrace), "frame trace write");
    }
    Check(bool(report), "report write");
    std::cout << "Reproducible mixed 2D stress completed. Physical minimum-PC 60 FPS acceptance: "
                 "NOT RUN.\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
