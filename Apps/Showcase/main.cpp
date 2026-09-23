#include "Nexora/Core/Engine.h"
#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Game/GameWorld.h"
#include "Nexora/Game/GameplayHostBridge.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Renderer/FramePipeline.h"
#include "Nexora/Renderer/PipelineCache.h"
#include "Nexora/Runtime/GameplayModuleHost.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <new>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

extern "C" int32_t NexoraGameModuleLoad(uint32_t requested_abi, NexoraGameModuleV3 *module);
extern "C" uint32_t NexoraGameModuleUpdateCount();
extern "C" double NexoraGameModuleElapsedSeconds();
extern "C" uint32_t NexoraGameModuleFixedUpdateCount();
extern "C" uint32_t NexoraGameModuleStartCount();

namespace {

using namespace nexora;

constexpr double kFixedDeltaSeconds = 1.0 / 60.0;
constexpr std::uint64_t kPrimaryEntityToken = 0;

struct CommandLine final {
  bool headless{true};
  bool validate_v1{};
  bool reload{true};
  bool frames_explicit{};
  std::size_t frames{4};
  std::string mode{"headless"};
  std::string scene{"hub"};
  std::string backend{"validation"};
  std::string gameplay_module{"auto"};
  std::filesystem::path report;
};

struct ShowcaseHostContext final {
  game::GameWorld *world{};
  core::AsyncLogService *log{};
  runtime::Id primary_entity{};
  runtime::Id scene{};
  std::uint64_t frame{};
  std::size_t debug_lines{};
  std::size_t api_errors{};
  std::size_t read_callbacks{};
  std::size_t write_callbacks{};
  bool received_zig_log{};
  Nexora::Presentation::SurfaceInputSnapshot input{};
};

struct ShowcaseRun final {
  bool engine_initialized{};
  bool module_loaded{};
  bool lifecycle_ok{};
  bool scene_ok{};
  bool render_ok{};
  bool reload_ok{};
  bool shutdown_ok{};
  std::size_t frames{};
  std::size_t scene_entities{};
  std::size_t visible_meshes{};
  runtime::Id scene_id{};
  runtime::Id primary_entity{};
  game::EntitySnapshot primary_snapshot{};
  runtime::SceneFrameResult render{};
  rhi::DeviceDiagnostics device{};
  runtime::GameplayModuleHost::ReloadStats reload{};
  std::size_t read_callbacks{};
  std::size_t write_callbacks{};
  bool received_zig_log{};
  std::size_t debug_lines{};
  std::size_t api_errors{};
  bool dynamic_gameplay{};
  std::uint32_t start_count{};
  std::uint32_t fixed_update_count{};
  std::uint32_t update_count{};
  double elapsed_seconds{};
  bool native_presentation{};
  bool backend_fallback{};
  std::string fallback_reason;
  Nexora::Presentation::SurfaceDiagnostics surface{};
};

enum class CapabilityState { Implemented, ContractOnly, Unavailable };

struct GalleryRoom final {
  std::string_view id;
  std::string_view title;
  CapabilityState state;
  std::string_view fallback;
};

constexpr std::array<GalleryRoom, 5> GalleryRooms() {
  return {
      {{"math", "Math Lab", CapabilityState::Implemented, "CPU transform and ray/AABB diagnostics"},
       {"scene", "Scene Lab", CapabilityState::Implemented,
        "public scene/entity/component callbacks"},
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
       {"gameplay", "Gameplay Lab", CapabilityState::Implemented, "deterministic physics raycast"},
#else
       {"gameplay", "Gameplay Lab", CapabilityState::ContractOnly,
        "physics/navigation simulation is disabled"},
#endif
#if NEXORA_PRESENTATION_ENABLED
       {"presentation", "Presentation Lab", CapabilityState::Implemented,
        "portable animation, audio, VFX, and media simulation"},
#else
       {"presentation", "Presentation Lab", CapabilityState::ContractOnly,
        "presentation simulation is disabled"},
#endif
#if NEXORA_LARGE_WORLD_ENABLED
       {"streaming", "Streaming Lab", CapabilityState::Implemented,
        "cell residency, budgets, and HLOD fallback"}}};
#else
       {"streaming", "Streaming Lab", CapabilityState::Unavailable,
        "large-world streaming is disabled"}}};
#endif
}

constexpr std::string_view ToString(CapabilityState state) {
  switch (state) {
  case CapabilityState::Implemented:
    return "IMPLEMENTED";
  case CapabilityState::ContractOnly:
    return "CONTRACT ONLY";
  case CapabilityState::Unavailable:
    return "UNAVAILABLE";
  }
  return "UNAVAILABLE";
}

const GalleryRoom *FindGalleryRoom(std::string_view id) {
  for (const auto &room : GalleryRooms()) {
    if (room.id == id)
      return &room;
  }
  return nullptr;
}

bool ParseUnsigned(std::string_view text, std::size_t &value) {
  if (text.empty())
    return false;
  const auto *begin = text.data();
  const auto *end = begin + text.size();
  const auto result = std::from_chars(begin, end, value);
  return result.ec == std::errc{} && result.ptr == end;
}

bool ParseCommandLine(int argc, char **argv, CommandLine &command, std::string &error) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--headless") {
      command.headless = true;
      command.mode = "headless";
    } else if (argument == "--validate-v1") {
      command.validate_v1 = true;
    } else if (argument == "--no-reload") {
      command.reload = false;
    } else if (argument == "--help" || argument == "-h") {
      error = "help";
      return false;
    } else if (argument.starts_with("--frames=")) {
      if (!ParseUnsigned(argument.substr(9), command.frames) || command.frames == 0 ||
          command.frames > 10000) {
        error = "--frames must be an integer from 1 to 10000";
        return false;
      }
      command.frames_explicit = true;
    } else if (argument.starts_with("--report=")) {
      command.report = std::filesystem::path(argument.substr(9));
      if (command.report.empty()) {
        error = "--report requires a path";
        return false;
      }
    } else if (argument.starts_with("--mode=")) {
      command.mode = std::string(argument.substr(7));
      if (command.mode != "headless" && command.mode != "interactive") {
        error = "--mode must be headless or interactive";
        return false;
      }
      command.headless = command.mode == "headless";
    } else if (argument.starts_with("--scene=")) {
      command.scene = std::string(argument.substr(8));
      if (command.scene != "hub" && command.scene != "tour" &&
          FindGalleryRoom(command.scene) == nullptr) {
        error = "--scene must be hub, tour, math, scene, gameplay, presentation, or streaming";
        return false;
      }
    } else if (argument.starts_with("--backend=")) {
      command.backend = std::string(argument.substr(10));
      if (command.backend != "auto" && command.backend != "validation" &&
          command.backend != "dx12") {
        error = "supported backends are auto, validation, and dx12";
        return false;
      }
    } else if (argument.starts_with("--gameplay-module=")) {
      command.gameplay_module = std::string(argument.substr(18));
      if (command.gameplay_module != "auto" && command.gameplay_module != "static" &&
          command.gameplay_module != "dynamic") {
        error = "--gameplay-module must be auto, static, or dynamic";
        return false;
      }
    } else {
      error = "unknown argument: " + std::string(argument);
      return false;
    }
  }
  return true;
}

void PrintUsage() {
  std::cout
      << "NexoraShowcase - C++ engine-owned Zig gameplay showcase\n"
         "Usage: NexoraShowcase.exe [options]\n\n"
         "Options:\n"
         "  --headless                 run the deterministic offscreen showcase\n"
         "  --validate-v1              include the V1 validation label in the report\n"
         "  --frames=N                 run N fixed/update frames (1..10000)\n"
         "  --report=PATH              write the JSON report to PATH\n"
         "  --no-reload                skip the transactional Zig state reload\n"
         "  --mode=headless|interactive select deterministic or native presentation\n"
         "  --scene=ROOM               select hub, tour, math, scene, gameplay, presentation, "
         "or streaming\n"
         "  --backend=auto|validation|dx12 select the presentation backend\n"
         "  --gameplay-module=auto|static|dynamic select Zig artifact ownership\n";
}

void Log(void *opaque_context, std::uint32_t level, const char *message,
         std::uint32_t message_length) {
  if (opaque_context == nullptr || message == nullptr)
    return;
  auto &context = *static_cast<ShowcaseHostContext *>(opaque_context);
  if (context.log == nullptr || level > static_cast<std::uint32_t>(core::LogLevel::Fatal))
    return;
  const std::string text(message, message_length);
  if (text == "Zig gameplay initialized")
    context.received_zig_log = true;
  context.log->Write(static_cast<core::LogLevel>(level), "Gameplay", text);
}

int32_t ReadComponent(void *opaque_context, std::uint64_t entity, std::uint64_t component_type,
                      void *data, std::uint32_t data_size) {
  if (opaque_context == nullptr || data == nullptr)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  auto &context = *static_cast<ShowcaseHostContext *>(opaque_context);
  if (context.world == nullptr)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  const auto resolved_entity = entity == kPrimaryEntityToken ? context.primary_entity : entity;
  const auto snapshot = context.world->GetEntity(resolved_entity);
  if (!snapshot)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  if (component_type != game::TransformComponentType() ||
      data_size < sizeof(game::GameplayTransformWire))
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  const game::GameplayTransformWire wire{snapshot->transform.x, snapshot->transform.y,
                                         snapshot->transform.z};
  std::memcpy(data, &wire, sizeof(wire));
  ++context.read_callbacks;
  return NEXORA_GAMEPLAY_OK;
}

int32_t WriteComponent(void *opaque_context, std::uint64_t entity, std::uint64_t component_type,
                       const void *data, std::uint32_t data_size) {
  if (opaque_context == nullptr || data == nullptr)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  auto &context = *static_cast<ShowcaseHostContext *>(opaque_context);
  if (context.world == nullptr)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  const auto resolved_entity = entity == kPrimaryEntityToken ? context.primary_entity : entity;
  if (component_type != game::TransformComponentType() ||
      data_size < sizeof(game::GameplayTransformWire))
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  game::GameplayTransformWire wire{};
  std::memcpy(&wire, data, sizeof(wire));
  if (!context.world->SetTransform(resolved_entity, {wire.x, wire.y, wire.z}))
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  ++context.write_callbacks;
  return NEXORA_GAMEPLAY_OK;
}

void *Allocate(void *, std::uint64_t, std::uint64_t size, std::uint64_t alignment) {
  if (size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0)
    return nullptr;
  try {
    return ::operator new(static_cast<std::size_t>(size),
                          std::align_val_t{static_cast<std::size_t>(alignment)});
  } catch (const std::bad_alloc &) {
    return nullptr;
  }
}

void Deallocate(void *, std::uint64_t, void *allocation, std::uint64_t, std::uint64_t alignment) {
  if (allocation != nullptr)
    ::operator delete(allocation, std::align_val_t{static_cast<std::size_t>(alignment)});
}

int32_t LoadScene(void *opaque, const char *name, std::uint32_t length, std::uint32_t persistent,
                  std::uint64_t *scene) {
  if (!opaque || !name || !scene || length == 0)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  auto &context = *static_cast<ShowcaseHostContext *>(opaque);
  try {
    *scene = context.world->LoadScene(std::string(name, length), persistent != 0);
    context.scene = *scene;
    return NEXORA_GAMEPLAY_OK;
  } catch (...) {
    ++context.api_errors;
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  }
}
int32_t ActivateScene(void *opaque, std::uint64_t scene) {
  if (!opaque)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  return static_cast<ShowcaseHostContext *>(opaque)->world->ActivateScene(scene)
             ? NEXORA_GAMEPLAY_OK
             : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
}
int32_t SpawnEntity(void *opaque, std::uint64_t scene, const NexoraEntitySpawnDescriptor *wire,
                    std::uint64_t *entity) {
  if (!opaque || !wire || !entity || wire->struct_size < sizeof(*wire))
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  auto &context = *static_cast<ShowcaseHostContext *>(opaque);
  game::EntitySpawnDescriptor descriptor;
  descriptor.transform = {wire->position.x, wire->position.y, wire->position.z};
  if (wire->components & NEXORA_SPAWN_CAMERA)
    descriptor.camera = runtime::CameraComponent{wire->camera_fov_degrees, 0.1, 100.0};
  if (wire->components & NEXORA_SPAWN_LIGHT)
    descriptor.light = runtime::LightComponent{wire->light_intensity};
  if (wire->components & NEXORA_SPAWN_MESH)
    descriptor.mesh_renderer =
        runtime::MeshComponent{wire->mesh.value, runtime::MaterialComponent{wire->material.value}};
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  if (wire->components & NEXORA_SPAWN_PHYSICS)
    descriptor.physics = runtime::PhysicsBody{
        0,
        {wire->bounds_minimum.x, wire->bounds_minimum.y, wire->bounds_minimum.z},
        {wire->bounds_maximum.x, wire->bounds_maximum.y, wire->bounds_maximum.z}};
#endif
  try {
    *entity = context.world->SpawnEntity(scene, descriptor);
    if (context.primary_entity == 0 && (wire->components & NEXORA_SPAWN_MESH))
      context.primary_entity = *entity;
    return NEXORA_GAMEPLAY_OK;
  } catch (...) {
    ++context.api_errors;
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  }
}
int32_t DespawnEntity(void *opaque, std::uint64_t entity) {
  if (!opaque)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  return static_cast<ShowcaseHostContext *>(opaque)->world->DestroyEntity(entity)
             ? NEXORA_GAMEPLAY_OK
             : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
}
int32_t CaptureInput(void *opaque, std::uint32_t user, NexoraInputSnapshot *snapshot) {
  if (!opaque || !snapshot || user != 0)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  const auto &context = *static_cast<ShowcaseHostContext *>(opaque);
  *snapshot = {context.input.sequence, static_cast<double>(context.input.pointerX),
               static_cast<double>(context.input.pointerY),
               context.input.lastKeyDown ? static_cast<std::uint32_t>(context.input.lastKey) : 0,
               context.input.focused ? 1U : 0U};
  return NEXORA_GAMEPLAY_OK;
}
int32_t ResolveAsset(void *, std::uint64_t high, std::uint64_t low, NexoraAssetHandle *asset) {
  if (!asset || (high == 0 && low == 0))
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  asset->value = low ^ (high * 1099511628211ULL);
  return asset->value != 0 ? NEXORA_GAMEPLAY_OK : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
}
int32_t Raycast(void *opaque, const NexoraRaycastRequest *request, NexoraRaycastHit *hit) {
  if (!opaque || !request || !hit)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  const auto &context = *static_cast<ShowcaseHostContext *>(opaque);
  const auto entity = context.world->RaycastEntity(
      {{request->origin.x, request->origin.y, request->origin.z},
       {request->direction.x, request->direction.y, request->direction.z},
       request->distance});
  if (!entity)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  *hit = {*entity, 4.5, {request->origin.x, request->origin.y, 0.5}};
  return NEXORA_GAMEPLAY_OK;
#else
  return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
#endif
}
int32_t DebugDrawLine(void *opaque, const NexoraDebugLine *line) {
  if (!opaque || !line || !std::isfinite(line->start.x) || !std::isfinite(line->end.x))
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  ++static_cast<ShowcaseHostContext *>(opaque)->debug_lines;
  return NEXORA_GAMEPLAY_OK;
}
int32_t GetDiagnostics(void *opaque, NexoraFrameDiagnostics *diagnostics) {
  if (!opaque || !diagnostics)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  const auto &context = *static_cast<ShowcaseHostContext *>(opaque);
  *diagnostics = {context.frame, context.scene ? context.world->Query(context.scene).size() : 0,
                  context.debug_lines, context.api_errors};
  return NEXORA_GAMEPLAY_OK;
}

NexoraGameplayHostV3 MakeHost(ShowcaseHostContext &context) {
  return {sizeof(NexoraGameplayHostV3),
          NEXORA_GAMEPLAY_ABI_VERSION,
          NEXORA_GAMEPLAY_CAPABILITY_HOST_ALLOCATOR | NEXORA_GAMEPLAY_CAPABILITY_SCENE_API,
          &context,
          &Log,
          &ReadComponent,
          &WriteComponent,
          &Allocate,
          &Deallocate,
          &LoadScene,
          &ActivateScene,
          &SpawnEntity,
          &DespawnEntity,
          &CaptureInput,
          &ResolveAsset,
          &Raycast,
          &DebugDrawLine,
          &GetDiagnostics};
}

bool NearlyEqual(double left, double right) { return std::abs(left - right) < 0.000001; }

bool RunShowcase(const CommandLine &command, core::Engine &engine, ShowcaseRun &result,
                 std::string &error) {
  game::GameWorld world;
  ShowcaseHostContext context{&world, engine.Services().log};
  runtime::GameplayModuleHost module(MakeHost(context));
  const bool use_dynamic =
      command.gameplay_module == "dynamic" || (command.gameplay_module == "auto" &&
#if defined(NEXORA_BUILD_DEVELOPMENT)
                                               true
#else
                                               false
#endif
                                              );
  const std::filesystem::path dynamic_library{NEXORA_ZIG_SHARED_LIBRARY_PATH};
  if (!(use_dynamic ? module.Load(dynamic_library) : module.Load(NexoraGameModuleLoad))) {
    error = "Zig gameplay module failed to load or start";
    return false;
  }
  result.dynamic_gameplay = use_dynamic;
  result.module_loaded = module.IsLoaded();

  std::unique_ptr<Nexora::Presentation::RenderSurface> nativeSurface;
  if (!command.headless && command.backend != "validation") {
    const auto backend = command.backend == "dx12"
                             ? Nexora::Presentation::SurfaceBackend::Dx12
                             : Nexora::Presentation::SurfaceBackend::Automatic;
    auto created = Nexora::Presentation::CreateRenderSurface(
        {"Nexora Showcase", 1280, 720, true, backend, Nexora::Presentation::PresentMode::VSync});
    if (created) {
      nativeSurface = std::move(created.surface);
      result.native_presentation = true;
    } else if (command.backend == "auto") {
      result.backend_fallback = true;
      result.fallback_reason = created.reason;
      std::cerr << "NexoraShowcase: backend fallback auto -> validation: " << created.reason
                << '\n';
    } else {
      error = "dx12 presentation failed: " + created.reason;
      module.Unload();
      return false;
    }
  } else if (!command.headless) {
    result.backend_fallback = true;
    result.fallback_reason =
        "validation backend is offscreen and cannot provide an interactive window";
    std::cerr << "NexoraShowcase: interactive fallback to headless: " << result.fallback_reason
              << '\n';
  }

  auto device = rhi::CreateValidationDevice();
  const auto layout = rhi::TrianglePipelineLayout();
  renderer::PipelineCache pipelines{*device, *engine.Services().jobs};
  const auto future = pipelines.Request(
      {layout.layout_hash, 0x1234, rhi::TextureFormat::Rgba8Unorm, "Zig Showcase"});
  future.Wait();
  const auto pipeline = future.Get();
  const rhi::TextureDescriptor output_descriptor{640, 360, rhi::TextureFormat::Rgba8Unorm,
                                                 rhi::ResourceState::Present,
                                                 "Zig Showcase offscreen output"};
  const auto output = device->CreateTexture(output_descriptor);

  const auto frameLimit = nativeSurface && !command.frames_explicit
                              ? std::numeric_limits<std::size_t>::max()
                              : command.frames;
  std::size_t executedFrames = 0;
  for (std::size_t frame = 0; frame < frameLimit; ++frame) {
    if (nativeSurface) {
      const auto status = nativeSurface->BeginFrame();
      if (nativeSurface->CloseRequested())
        break;
      if (status == Nexora::Presentation::SurfaceStatus::ZeroExtent ||
          status == Nexora::Presentation::SurfaceStatus::Occluded)
        continue;
      if (status != Nexora::Presentation::SurfaceStatus::Ready) {
        error =
            "presentation acquire failed: " + std::string(Nexora::Presentation::ToString(status));
        device->DestroyTexture(output);
        return false;
      }
      context.input = nativeSurface->Input();
    }
    context.frame = frame + 1;
    engine.BeginFrame();
    if (!module.FixedUpdate(kFixedDeltaSeconds) || !module.Update(kFixedDeltaSeconds)) {
      error = "Zig gameplay callback failed during the fixed/update loop";
      device->DestroyTexture(output);
      return false;
    }
    world.EndFrame();
    ++executedFrames;
    if (nativeSurface) {
      const auto status = nativeSurface->EndFrame();
      if (status != Nexora::Presentation::SurfaceStatus::Ready &&
          status != Nexora::Presentation::SurfaceStatus::Occluded) {
        error = "presentation failed: " + std::string(Nexora::Presentation::ToString(status));
        device->DestroyTexture(output);
        return false;
      }
    }
  }

  const auto scene = context.scene;
  const auto primary_entity = context.primary_entity;
  const auto snapshot_before_reload = world.GetEntity(primary_entity);
  if (!snapshot_before_reload) {
    error = "the Zig-controlled entity disappeared from the showcase world";
    device->DestroyTexture(output);
    return false;
  }

  bool reload_ok = true;
  if (command.reload)
    reload_ok = use_dynamic ? module.Reload(dynamic_library) : module.Reload(NexoraGameModuleLoad);
  const auto reload = module.GetReloadStats();
  const auto render = runtime::RenderSceneFrame(world.InternalWorld(), *device, output,
                                                output_descriptor, pipeline);
  const auto diagnostics = device->Diagnostics();
  device->WaitIdle();
  device->DestroyTexture(output);

  const auto snapshot = world.GetEntity(primary_entity);
  const auto visible_meshes = world.Query(scene, game::GameWorld::kQueryMeshRenderer).size();
  const bool lifecycle_ok = context.received_zig_log && module.IsLoaded() &&
                            context.read_callbacks == executedFrames &&
                            context.write_callbacks == executedFrames;
  const bool scene_ok = snapshot.has_value() && visible_meshes == 3 &&
                        NearlyEqual(snapshot->transform.x, executedFrames * kFixedDeltaSeconds) &&
                        context.debug_lines == executedFrames && context.api_errors == 0;
  const bool render_ok = render.has_value() && render->visible_meshes == visible_meshes &&
                         render->passes == 5 && render->barriers == 6 &&
                         diagnostics.validation_errors == 0;
  const bool migration_ok =
      !command.reload || (reload_ok && reload.successful_reloads == 1 && reload.migrated_bytes > 0);
  module.Unload();
  if (nativeSurface) {
    result.surface = nativeSurface->Diagnostics();
    if (nativeSurface->DrainAndDestroy() != Nexora::Presentation::SurfaceStatus::Ready) {
      error = "native presentation teardown failed";
      return false;
    }
  }
  const bool shutdown_ok = !module.IsLoaded();

  result.engine_initialized = engine.IsInitialized();
  result.lifecycle_ok = lifecycle_ok;
  result.scene_ok = scene_ok;
  result.render_ok = render_ok;
  result.reload_ok = migration_ok;
  result.shutdown_ok = shutdown_ok;
  result.frames = executedFrames;
  result.scene_entities = world.Query(scene).size();
  result.visible_meshes = visible_meshes;
  result.scene_id = scene;
  result.primary_entity = primary_entity;
  result.primary_snapshot = snapshot.value_or(game::EntitySnapshot{});
  result.render = render.value_or(runtime::SceneFrameResult{});
  result.device = diagnostics;
  result.reload = reload;
  result.read_callbacks = context.read_callbacks;
  result.write_callbacks = context.write_callbacks;
  result.received_zig_log = context.received_zig_log;
  result.debug_lines = context.debug_lines;
  result.api_errors = context.api_errors;
  result.start_count = 1;
  result.fixed_update_count = static_cast<std::uint32_t>(executedFrames);
  result.update_count = static_cast<std::uint32_t>(executedFrames);
  result.elapsed_seconds = executedFrames * kFixedDeltaSeconds;
  result.module_loaded = !module.IsLoaded();

  return lifecycle_ok && scene_ok && render_ok && migration_ok && shutdown_ok;
}

std::string BuildReport(const CommandLine &command, const ShowcaseRun &run) {
  const auto build = foundation::GetBuildInfo();
  std::ostringstream report;
  report << std::boolalpha << std::setprecision(17);
  report << "{\n"
         << "  \"schema\": \"nexora.zig_showcase.v1\",\n"
         << "  \"status\": \""
         << (run.lifecycle_ok && run.scene_ok && run.render_ok && run.reload_ok && run.shutdown_ok
                 ? "PASS"
                 : "FAIL")
         << "\",\n"
         << "  \"mode\": \"" << command.mode << "\",\n"
         << "  \"scene\": \"" << command.scene << "\",\n"
         << "  \"backend\": \"" << command.backend << "\",\n"
         << "  \"validate_v1\": " << command.validate_v1 << ",\n"
         << "  \"build\": {\n"
         << "    \"engine_version\": \"" << build.engine_version << "\",\n"
         << "    \"build_id\": \"" << foundation::GetBuildId() << "\",\n"
         << "    \"abi_version\": " << build.abi_version << ",\n"
         << "    \"configuration\": \"" << build.build_configuration << "\",\n"
         << "    \"link_mode\": \"" << build.link_mode << "\"\n"
         << "  },\n"
         << "  \"capabilities\": {\n"
         << "    \"engine_owned_main\": \"IMPLEMENTED\",\n"
         << "    \"zig_static_gameplay\": \"IMPLEMENTED\",\n"
         << "    \"zig_dynamic_gameplay\": \""
         << (run.dynamic_gameplay ? "IMPLEMENTED" : "AVAILABLE") << "\",\n"
         << "    \"headless_validation\": \"IMPLEMENTED\",\n"
         << "    \"transactional_reload\": \"" << (run.reload_ok ? "IMPLEMENTED" : "FAIL")
         << "\",\n"
         << "    \"windowed_native_backend\": \""
         << (run.native_presentation ? "IMPLEMENTED" : "AVAILABLE ON WINDOWS") << "\"\n"
         << "  },\n"
         << "  \"gallery\": {\n"
         << "    \"selected\": \"" << command.scene << "\",\n"
         << "    \"rooms\": [\n";
  const auto rooms = GalleryRooms();
  for (std::size_t index = 0; index < rooms.size(); ++index) {
    const auto &room = rooms[index];
    report << "      {\"id\": \"" << room.id << "\", \"title\": \"" << room.title
           << "\", \"status\": \"" << ToString(room.state) << "\", \"fallback\": \""
           << room.fallback << "\"}" << (index + 1 == rooms.size() ? "\n" : ",\n");
  }
  report << "    ]\n"
         << "  },\n"
         << "  \"lifecycle\": {\n"
         << "    \"engine_initialized\": " << run.engine_initialized << ",\n"
         << "    \"module_callbacks\": " << run.lifecycle_ok << ",\n"
         << "    \"module_unloaded_before_engine_shutdown\": " << run.shutdown_ok << ",\n"
         << "    \"frames\": " << run.frames << ",\n"
         << "    \"start_count\": " << run.start_count << ",\n"
         << "    \"fixed_update_count\": " << run.fixed_update_count << ",\n"
         << "    \"update_count\": " << run.update_count << ",\n"
         << "    \"elapsed_seconds\": " << run.elapsed_seconds << "\n"
         << "  },\n"
         << "  \"scene_evidence\": {\n"
         << "    \"scene_id\": " << run.scene_id << ",\n"
         << "    \"entity_count\": " << run.scene_entities << ",\n"
         << "    \"primary_entity\": " << run.primary_entity << ",\n"
         << "    \"primary_transform\": {\n"
         << "      \"x\": " << run.primary_snapshot.transform.x << ",\n"
         << "      \"y\": " << run.primary_snapshot.transform.y << ",\n"
         << "      \"z\": " << run.primary_snapshot.transform.z << "\n"
         << "    },\n"
         << "    \"zig_read_callbacks\": " << run.read_callbacks << ",\n"
         << "    \"zig_write_callbacks\": " << run.write_callbacks << ",\n"
         << "    \"zig_log_received\": " << run.received_zig_log << ",\n"
         << "    \"high_level_debug_lines\": " << run.debug_lines << ",\n"
         << "    \"public_api_errors\": " << run.api_errors << "\n"
         << "  },\n"
         << "  \"render_evidence\": {\n"
         << "    \"backend\": \"" << (run.native_presentation ? "DX12" : "Validation") << "\",\n"
         << "    \"backend_fallback\": " << run.backend_fallback << ",\n"
         << "    \"fallback_reason\": \"" << run.fallback_reason << "\",\n"
         << "    \"surface_acquires\": " << run.surface.acquiredFrames << ",\n"
         << "    \"surface_presents\": " << run.surface.presentedFrames << ",\n"
         << "    \"visible_meshes\": " << run.visible_meshes << ",\n"
         << "    \"passes\": " << run.render.passes << ",\n"
         << "    \"barriers\": " << run.render.barriers << ",\n"
         << "    \"submitted_command_lists\": " << run.device.submitted_command_lists << ",\n"
         << "    \"draw_calls\": " << run.device.draw_calls << ",\n"
         << "    \"presents\": " << run.device.presents << ",\n"
         << "    \"validation_errors\": " << run.device.validation_errors << "\n"
         << "  },\n"
         << "  \"reload\": {\n"
         << "    \"requested\": " << command.reload << ",\n"
         << "    \"successful_reloads\": " << run.reload.successful_reloads << ",\n"
         << "    \"migrated_bytes\": " << run.reload.migrated_bytes << "\n"
         << "  }\n"
         << "}\n";
  return report.str();
}

void WriteReport(const std::filesystem::path &path, const std::string &report) {
  if (path.has_parent_path())
    std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  if (!output)
    throw std::runtime_error("could not open showcase report: " + path.string());
  output << report;
  if (!output)
    throw std::runtime_error("could not write showcase report: " + path.string());
}

} // namespace

int main(int argc, char **argv) {
  CommandLine command;
  std::string error;
  if (!ParseCommandLine(argc, argv, command, error)) {
    if (error == "help") {
      PrintUsage();
      return 0;
    }
    std::cerr << "NexoraShowcase: " << error << "\nUse --help for usage.\n";
    return 2;
  }

  try {
    core::Engine engine;
    core::EngineConfiguration configuration;
    configuration.content_root = std::filesystem::current_path();
    engine.Initialize(configuration);

    ShowcaseRun run;
    const bool success = RunShowcase(command, engine, run, error);
    const auto report = BuildReport(command, run);
    std::cout << report;
    if (!command.report.empty())
      WriteReport(command.report, report);
    engine.Shutdown();
    if (!success) {
      std::cerr << "NexoraShowcase validation failed: " << error << "\n";
      return 1;
    }
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << "NexoraShowcase fatal error: " << exception.what() << "\n";
    return 1;
  }
}
