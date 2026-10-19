#include "IntegratedStress.h"
#include "Nexora/Core/Memory.h"
#include "Nexora/Core/ProcessMemory.h"
#include "Nexora/TwoD/GameplayUi.h"
#include "Nexora/TwoD/Lifecycle.h"
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <stdexcept>
#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <dxgi1_4.h>
#include <psapi.h>
#include <wrl/client.h>
#endif

namespace nexora::twod::validation {
namespace {
using Clock = std::chrono::steady_clock;
using namespace Nexora::Presentation;
constexpr std::size_t particles = 256;
constexpr std::size_t loadingBytes = 8 * 1024 * 1024;
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
double Ms(Clock::time_point a, Clock::time_point b) {
  return std::chrono::duration<double, std::milli>(b - a).count();
}
struct MemorySample {
  std::optional<std::uint64_t> rss, privateBytes, handles, localUsage, localBudget, nonlocalUsage;
};
class MemoryProbe {
public:
  explicit MemoryProbe(const SurfaceDeviceInfo &device) {
#if defined(_WIN32)
    using Microsoft::WRL::ComPtr;
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
      return;
    unsigned matches = 0;
    for (unsigned i = 0;; ++i) {
      ComPtr<IDXGIAdapter1> candidate;
      if (factory->EnumAdapters1(i, &candidate) == DXGI_ERROR_NOT_FOUND)
        break;
      if (!candidate)
        break;
      DXGI_ADAPTER_DESC1 description{};
      if (SUCCEEDED(candidate->GetDesc1(&description)) && device.deviceIdsAvailable &&
          description.VendorId == device.vendorId && description.DeviceId == device.deviceId) {
        ++matches;
        candidate.As(&adapter_);
      }
    }
    // Do not silently attribute another identical board's usage to the renderer.
    if (matches != 1)
      adapter_.Reset();
#else
    (void)device;
#endif
  }
  MemorySample Read() const {
    MemorySample sample;
    sample.rss = core::CurrentProcessResidentBytes();
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX memory{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(),
                                reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),
                                sizeof(memory)))
      sample.privateBytes = memory.PrivateUsage;
    DWORD count{};
    if (GetProcessHandleCount(GetCurrentProcess(), &count))
      sample.handles = count;
    DXGI_QUERY_VIDEO_MEMORY_INFO info{};
    if (adapter_ &&
        SUCCEEDED(adapter_->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info))) {
      sample.localUsage = info.CurrentUsage;
      sample.localBudget = info.Budget;
    }
    if (adapter_ &&
        SUCCEEDED(adapter_->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &info)))
      sample.nonlocalUsage = info.CurrentUsage;
#endif
    return sample;
  }

private:
#if defined(_WIN32)
  Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter_;
#endif
};
GameplayUiFont Font() {
  constexpr std::string_view text = " 10KUIVFX";
  constexpr std::array<std::array<unsigned, 7>, 9> bits{{{0, 0, 0, 0, 0, 0, 0},
                                                         {4, 12, 4, 4, 4, 4, 14},
                                                         {14, 17, 19, 21, 25, 17, 14},
                                                         {17, 18, 20, 24, 20, 18, 17},
                                                         {17, 17, 17, 17, 17, 17, 14},
                                                         {14, 4, 4, 4, 4, 4, 14},
                                                         {17, 17, 17, 17, 17, 10, 4},
                                                         {31, 16, 16, 30, 16, 16, 16},
                                                         {17, 17, 10, 4, 10, 17, 17}}};
  GameplayUiFont font;
  font.texture_id = std::uint64_t{1} << 60;
  font.width = 54;
  font.height = 8;
  font.pixel_size = 7;
  font.line_height = 8;
  font.rgba.resize(font.width * font.height * 4);
  for (unsigned i = 0; i < text.size(); ++i) {
    for (unsigned y = 0; y < 7; ++y)
      for (unsigned x = 0; x < 5; ++x)
        if (bits[i][y] & (1U << (4 - x)))
          for (unsigned c = 0; c < 4; ++c)
            font.rgba[((y * font.width + i * 6 + x) * 4) + c] = std::byte{255};
    font.glyphs.push_back({static_cast<std::uint32_t>(text[i]), 6, 0, 0, 5, 7,
                           float(i * 6) / float(font.width), 0,
                           float(i * 6 + 5) / float(font.width), 7.F / 8});
  }
  return font;
}
struct Sample {
  std::uint64_t frame{}, reloads{}, queryHits{}, sweepHits{}, gpuSubmission{}, activations{};
  bool warmup{}, sparse{}, reloaded{}, memorySampled{};
  std::size_t visible{}, culled{}, batches{}, uiQuads{}, uiCommands{};
  double cpu{}, full{}, cycle{}, elapsed{}, coverage{};
  std::array<double, 9> stages{};
  std::optional<double> gpu;
  SurfaceDiagnostics diagnostics;
  MemorySample memory;
  core::MemoryStatistics allocation;
};
template <class T> void Optional(std::ostream &out, const std::optional<T> &value) {
  if (value)
    out << *value;
}
void Write(std::ostream &out, const Sample &s, std::size_t objects) {
  out << s.frame << ',' << s.warmup << ',' << s.sparse << ',' << s.reloaded << ',' << s.elapsed
      << ',' << objects << ',' << particles << ',' << s.visible << ',' << s.culled << ','
      << s.batches << ',' << s.coverage << ',' << s.uiQuads << ',' << s.uiCommands << ','
      << s.queryHits << ',' << s.sweepHits << ',' << s.activations << ',' << s.cpu << ',' << s.full
      << ',' << s.cycle;
  for (const auto duration : s.stages)
    out << ',' << duration;
  out << ',' << s.gpuSubmission << ',';
  Optional(out, s.gpu);
  out << ',' << s.reloads << ',' << s.diagnostics.acquiredFrames << ','
      << s.diagnostics.presentedFrames << ',' << s.diagnostics.spriteInstancedDrawCalls << ','
      << s.diagnostics.spriteGpuInstances << ',' << s.diagnostics.nativeUiDrawCalls << ','
      << s.diagnostics.nativeUiTextureUploads << ',' << s.diagnostics.nativeUiBufferReallocations
      << ',' << s.diagnostics.nativeUiRejectedTextures << ',' << s.allocation.live_bytes << ','
      << s.allocation.peak_bytes << ',' << s.allocation.allocation_count << ','
      << s.allocation.free_count << ',' << s.memorySampled;
  for (const auto &value : {s.memory.rss, s.memory.privateBytes, s.memory.handles,
                            s.memory.localUsage, s.memory.localBudget, s.memory.nonlocalUsage}) {
    out << ',';
    Optional(out, value);
  }
  out << '\n';
  Check(bool(out), "integrated trace write");
}
} // namespace

void IntegratedStress(std::ostream &report, std::ostream &trace, const IntegratedOptions &options,
                      const std::optional<Atlas> &sourceAtlas,
                      const std::optional<Clip> &sourceClip) {
  const auto objects = options.objects;
  Check(objects == 1000 || objects == 5000 || objects == 10000,
        "integrated objects must be 1000, 5000 or 10000");
  const auto loading = Clock::now();
  Image white{1, 8, 8, Bytes(256, std::byte{255})};
  Image alternate = white;
  alternate.id = 2;
  for (std::size_t i = 0; i < alternate.rgba.size(); i += 4)
    alternate.rgba[i] = std::byte{128};
  const std::array images{white, alternate};
  auto atlas = sourceAtlas ? sourceAtlas : PackAtlas({9, 17}, images, 32, 32, true);
  auto effect = PackAtlas({29, 31}, std::span(&white, 1), 32, 32, true);
  Check(atlas && effect, "integrated atlases");
  std::array<std::shared_ptr<const Atlas>, 2> owners{std::make_shared<const Atlas>(*atlas),
                                                     std::make_shared<const Atlas>(*effect)};
  std::array<std::uint64_t, 2> textures{201, 202};
  Check(atlas->id != effect->id, "integrated atlas identities must differ");
  // A real atlas codec round trip occurs at each loading spike (not disk IO).
  const auto encodedAtlas = EncodeAtlas(*atlas);
  Check(!encodedAtlas.empty(), "integrated atlas encode");
  InstancePool pool(objects);
  std::vector<Instance> initial(objects);
  const auto populateInitial = [&](bool sparse) {
    for (std::size_t i = 0; i < objects; ++i) {
      auto &instance = initial[i];
      instance.sprite.atlas = atlas->id;
      instance.sprite.region = atlas->regions.front().id;
      instance.sprite.size = {0.15F, 0.15F};
      instance.sprite.color = {0.2F, 0.7F, 1.F, 1.F};
      instance.transform.x = double(i % 100) * (sparse ? 2.0 : 0.25) - 12;
      instance.transform.y = double(i / 100) * (sparse ? 2.0 : 0.25) - 12;
    }
  };
  populateInitial(false);
  auto handles = pool.Create(initial);
  Check(bool(handles), "integrated initial pool");
  RenderSurfaceResult surface;
  if (options.native) {
    RenderSurfaceDescriptor descriptor;
    descriptor.title = "Nexora 2D integrated soak";
    descriptor.width = 640;
    descriptor.height = 480;
    descriptor.enableGpuTiming = true;
    descriptor.backend = options.backend;
    surface = CreateRenderSurface(descriptor);
    Check(bool(surface), "integrated native surface");
  }
  const auto device = options.native ? surface.surface->Diagnostics().device : SurfaceDeviceInfo{};
  Check(!options.native || !surface.surface->Diagnostics().softwareRasterizer,
        "integrated physical run rejects software rasterizer");
  MemoryProbe memory(device);
  GameplayUi ui;
  Check(ui.RegisterFont(Font()), "integrated fixture font");
  NexoraGameplayUiApiV1 api{};
  api.struct_size = sizeof(api);
  Check(ui.GetApi(&api) == 0, "integrated UI negotiation");
  std::uint64_t font{};
  Check(api.get_font(api.context, 0, &font) == 0, "integrated UI font");
  Animator animation;
  Check(animation.Play(sourceClip ? *sourceClip
                                  : Clip{{18, 19},
                                         atlas->id,
                                         true,
                                         {{atlas->regions.front().id, 0.1, "tick"},
                                          {atlas->regions.back().id, 0.1, "tick"}}}),
        "integrated animation");
  FixedStepper step;
  SpatialIndex index(2);
  core::TrackingAllocator allocator;
  Camera camera;
  camera.width = 640;
  camera.height = 480;
  camera.verticalSize = 24;
  const unsigned warmup = options.seconds ? 1200 : 4;
  const auto loopStart = Clock::now();
  auto measuredStart = loopStart;
  auto previousStart = loopStart;
  std::optional<Sample> pending;
  std::uint64_t frames{}, reloads{}, steps{}, events{}, activations{}, totalInstances{};
  std::uint64_t lastGpu{}, gpuSamples{};
  bool sparse = false;
  trace << "frame,warmup,sparse,reloaded,elapsed_seconds,objects,vfx,visible,culled,sprite_batches,"
           "covered_aabb_ratio,ui_quads,ui_commands,query_hits,sweep_hits,activations,cpu_ms,"
           "frame_ms,cycle_ms,reload_ms,begin_frame_ms,populate_ms,index_ms,query_ms,"
           "extract_ms,ui_ms,draw_ms,end_frame_ms,gpu_submission,gpu_ms,reloads,acquired,"
           "presented,instanced_draws,gpu_instances,ui_draws,texture_uploads,buffer_reallocations,"
           "rejected_textures,fixture_alloc_live,fixture_alloc_peak,fixture_allocations,fixture_"
           "frees,"
           "memory_sampled,rss_bytes,private_bytes,process_handles,gpu_local_usage_bytes,"
           "gpu_local_budget_bytes,gpu_nonlocal_usage_bytes\n";
  trace << std::setprecision(17);
  for (;;) {
    const auto begin = Clock::now();
    if (pending) {
      pending->cycle = Ms(previousStart, begin);
      Write(trace, *pending, objects); // This IO is included in the new frame/cycle interval.
      pending.reset();
    }
    if (frames == warmup)
      measuredStart = begin;
    if (frames >= std::uint64_t(warmup) + options.frames &&
        (!options.seconds || Ms(measuredStart, begin) >= options.seconds * 1000.0))
      break;
    Sample sample;
    sample.frame = frames;
    sample.warmup = frames < warmup;
    sample.elapsed = Ms(loopStart, begin) / 1000;
    std::array<Clock::time_point, 10> points;
    points[0] = Clock::now();
    if (frames && frames % options.reloadEvery == 0) {
      auto *scratch = allocator.Allocate(loadingBytes, 64, 6);
      // Touch the declared allocation, and balance it even if validation throws.
      const auto freeScratch = [&](void *p) { allocator.Deallocate(p); };
      std::unique_ptr<void, decltype(freeScratch)> scratchOwner(scratch, freeScratch);
      // Volatile page touches cannot disappear as dead stores in monolithic LTCG builds.
      auto *pages = static_cast<volatile std::byte *>(scratch);
      for (std::size_t offset = 0; offset < loadingBytes; offset += 4096)
        pages[offset] = std::byte{0x5a};
      pages[loadingBytes - 1] = std::byte{0x5a};
      auto decoded = DecodeAtlas(encodedAtlas);
      Check(decoded && decoded->id == atlas->id, "integrated loading decode");
      Check(pool.Destroy(*handles) && !pool.Find(handles->front()), "integrated stale handles");
      sparse = !sparse;
      populateInitial(sparse);
      handles = pool.Create(initial);
      Check(bool(handles), "integrated pool recreate");
      if (options.native)
        Check(surface.surface->ReleaseSpriteTextures(textures) == SurfaceStatus::Ready,
              "integrated retire atlases");
      textures[0] += 2;
      textures[1] += 2;
      owners = {std::make_shared<const Atlas>(std::move(*decoded)),
                std::make_shared<const Atlas>(*effect)};
      ++reloads;
      sample.reloaded = true;
    }
    points[1] = Clock::now();
    if (options.native) {
      Check(surface.surface->BeginFrame() == SurfaceStatus::Ready, "integrated acquire");
      Check(!surface.surface->CloseRequested(), "integrated interrupted by window close");
      const auto size = surface.surface->FrameInfo();
      Check(size.width == 640 && size.height == 480, "integrated viewport changed");
    }
    points[2] = Clock::now();
    {
      const auto tick = step.Advance(1.0 / 60);
      Check(tick && tick->steps == 1, "integrated fixed step");
      steps += tick->steps;
      const auto animated = animation.Advance(step.StepSeconds());
      Check(bool(animated), "integrated animation advance");
      events += animated->events.size();
      std::vector<SpriteInstance> sprites;
      std::vector<Shape> shapes;
      sprites.reserve(objects + particles);
      shapes.reserve(objects);
      std::uint64_t id = 0;
      pool.ForEach([&](auto &instance) {
        instance.transform.x += 0.0001;
        instance.sprite.region = animated->region;
        const auto identity = ++id;
        sprites.push_back({identity, instance.sprite, runtime::ToMatrix(instance.transform)});
        Shape shape;
        shape.id = identity;
        shape.center = {float(instance.transform.x), float(instance.transform.y)};
        shape.radius = 0.05F;
        shapes.push_back(shape);
      });
      Check(id == objects && shapes.size() == objects, "integrated full updates");
      for (std::size_t i = 0; i < particles; ++i) {
        Sprite particle;
        particle.atlas = effect->id;
        particle.region = 1;
        particle.size = {0.6F, 0.6F};
        particle.color = {1.F, 0.2F, 0.05F, 0.125F};
        particle.layer = 1;
        runtime::Transform transform;
        const double phase = double(frames % 600) / 60 + double(i) * 0.02454369260617026;
        transform.x = double(i % 16) * 0.4 - 3 + std::sin(phase) * 0.2;
        transform.y = double(i / 16) * 0.4 - 3 + std::cos(phase) * 0.2;
        sprites.push_back({objects + i + 1, particle, runtime::ToMatrix(transform)});
      }
      points[3] = Clock::now();
      Check(index.Build(shapes), "integrated full query rebuild");
      points[4] = Clock::now();
      Shape query;
      query.radius = 2;
      sample.queryHits = index.Query(query).size();
      sample.sweepHits = index.Sweep({-1, 0}, {1, 0}, 0.05F).size();
      points[5] = Clock::now();
      auto extracted =
          ExtractSprites(sprites, camera, [&](AssetId asset) -> std::optional<AtlasBinding> {
            if (asset == atlas->id)
              return AtlasBinding{owners[0], textures[0]};
            if (asset == effect->id)
              return AtlasBinding{owners[1], textures[1]};
            return std::nullopt;
          });
      Check(extracted && extracted->statistics.input == objects + particles &&
                extracted->statistics.unavailable == 0 && extracted->statistics.batches == 2,
            "integrated extraction counts");
      sample.visible = extracted->statistics.visible;
      sample.culled = extracted->statistics.culled;
      sample.batches = extracted->statistics.batches;
      sample.coverage = extracted->statistics.coveredPixels / (640 * 480);
      points[6] = Clock::now();
      GameplayUiInput input;
      input.pointer_x = input.pointer_y = -1;
      input.navigate = frames % 60 == 0 ? 1 : 0;
      input.activate = frames % 90 == 0;
      ui.BeginInput(input);
      Check(api.begin_frame(api.context, 640, 480) == 0, "integrated UI begin");
      for (unsigned i = 0; i < 16; ++i)
        Check(api.panel(api.context, {float(i % 8) * 80, float(i / 8) * 28, 76, 24},
                        0x90403020u + static_cast<unsigned>(frames % 32)) == 0,
              "integrated panels");
      for (unsigned i = 0; i < 8; ++i) {
        std::uint32_t state{};
        Check(api.button(api.context, i + 1, {float(i) * 80, 60, 76, 24}, "UI", 2, font, 14, 1,
                         &state) == 0,
              "integrated buttons");
        activations += (state & NEXORA_UI_ACTIVATED) != 0;
        Check(api.text(api.context, font, 7, {float(i) * 80, 90, 76, 20}, "10K UI VFX", 10,
                       0xffffffffu) == 0,
              "integrated labels");
      }
      Check(api.end_frame(api.context) == 0, "integrated UI end");
      const auto overlay = ui.Frame();
      sample.uiQuads = overlay.vertices.size() / 4;
      sample.uiCommands = overlay.commands.size();
      Check(sample.uiQuads == 120 && sample.uiCommands == 120, "integrated UI counts");
      points[7] = Clock::now();
      sample.cpu = Ms(points[2], points[7]);
      if (options.native)
        Check(extracted->Draw(*surface.surface, overlay.DrawData()) == SurfaceStatus::Ready,
              "integrated draw");
      points[8] = Clock::now();
      if (options.native) {
        Check(surface.surface->EndFrame() == SurfaceStatus::Ready, "integrated present");
        sample.diagnostics = surface.surface->Diagnostics();
        const auto timing = sample.diagnostics.gpuTiming;
        if (timing.milliseconds && timing.completedSubmission != lastGpu) {
          lastGpu = timing.completedSubmission;
          sample.gpuSubmission = lastGpu;
          sample.gpu = timing.milliseconds;
          if (!sample.warmup && lastGpu > warmup)
            ++gpuSamples;
        }
        totalInstances += sample.visible;
        Check(sample.diagnostics.acquiredFrames == frames + 1 &&
                  sample.diagnostics.presentedFrames == frames + 1 &&
                  sample.diagnostics.spriteGpuInstances == totalInstances &&
                  sample.diagnostics.spriteInstancedDrawCalls == (frames + 1) * 2 &&
                  sample.diagnostics.nativeUiRejectedTextures == 0,
              "integrated native submission balance");
      }
      points[9] = Clock::now();
    } // Include frame-owned vectors/owners' destruction in full/cycle time.
    sample.memorySampled = frames % 60 == 0;
    if (sample.memorySampled)
      sample.memory = memory.Read();
    sample.allocation = allocator.Statistics();
    Check(sample.allocation.live_bytes == 0 &&
              sample.allocation.allocation_count == sample.allocation.free_count,
          "integrated fixture allocation balance");
    for (unsigned i = 0; i < sample.stages.size(); ++i)
      sample.stages[i] = Ms(points[i], points[i + 1]);
    sample.full = Ms(begin, Clock::now());
    sample.sparse = sparse;
    sample.reloads = reloads;
    sample.activations = activations;
    previousStart = begin;
    pending = sample;
    ++frames;
  }
  const auto measuredSeconds = Ms(measuredStart, Clock::now()) / 1000;
  Check(!pending && steps == frames && pool.Size() == objects, "integrated final state");
  SurfaceDiagnostics final;
  if (options.native) {
    final = surface.surface->Diagnostics();
    Check(surface.surface->DrainAndDestroy() == SurfaceStatus::Ready, "integrated drain");
  }
  Check(pool.Destroy(*handles) && pool.Size() == 0 && !pool.Find(handles->front()),
        "integrated final pool release");
  trace.flush();
  Check(bool(trace), "integrated trace flush");
  report << std::setprecision(17)
         << "objects,vfx,frames,warmup_frames,measured_seconds,loading_ms,reload_every,reloads,"
            "steps,animation_events,activations,gpu_samples,acquired,presented,texture_uploads,"
            "buffer_reallocations,fixture_alloc_live,fixture_alloc_peak,fixture_allocations,"
            "fixture_frees,device_name,vendor_id,device_id,driver_version,backend_requested\n"
         << objects << ',' << particles << ',' << frames - warmup << ',' << warmup << ','
         << measuredSeconds << ',' << Ms(loading, loopStart) << ',' << options.reloadEvery << ','
         << reloads << ',' << steps << ',' << events << ',' << activations << ',' << gpuSamples
         << ',' << final.acquiredFrames << ',' << final.presentedFrames << ','
         << final.nativeUiTextureUploads << ',' << final.nativeUiBufferReallocations << ','
         << allocator.Statistics().live_bytes << ',' << allocator.Statistics().peak_bytes << ','
         << allocator.Statistics().allocation_count << ',' << allocator.Statistics().free_count
         << ',' << std::quoted(std::string(device.name.data())) << ',' << device.vendorId << ','
         << device.deviceId << ',' << device.driverVersion << ','
         << (options.native ? ToString(options.backend) : "none") << '\n';
  report.flush();
  Check(bool(report), "integrated report write");
}
} // namespace nexora::twod::validation
