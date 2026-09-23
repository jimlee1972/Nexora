#include "Nexora/Core/Engine.h"
#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Game/GameWorld.h"
#include "Nexora/Game/GameplayHostBridge.h"
#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Renderer/FramePipeline.h"
#include "Nexora/Renderer/PipelineCache.h"
#include "Nexora/Runtime/GameplayModuleHost.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
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
  std::size_t frames{4};
  std::string mode{"headless"};
  std::string scene{"hub"};
  std::string backend{"validation"};
  std::filesystem::path report;
};

struct ShowcaseHostContext final {
  game::GameWorld *world{};
  core::AsyncLogService *log{};
  runtime::Id primary_entity{};
  std::size_t read_callbacks{};
  std::size_t write_callbacks{};
  bool received_zig_log{};
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
};

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
    } else if (argument.starts_with("--report=")) {
      command.report = std::filesystem::path(argument.substr(9));
      if (command.report.empty()) {
        error = "--report requires a path";
        return false;
      }
    } else if (argument.starts_with("--mode=")) {
      command.mode = std::string(argument.substr(7));
      if (command.mode != "headless") {
        error = "only --mode=headless is implemented; windowed mode remains CONTRACT ONLY";
        return false;
      }
      command.headless = true;
    } else if (argument.starts_with("--scene=")) {
      command.scene = std::string(argument.substr(8));
      if (command.scene != "hub") {
        error = "only --scene=hub is implemented in the Zig bootstrap slice";
        return false;
      }
    } else if (argument.starts_with("--backend=")) {
      command.backend = std::string(argument.substr(10));
      if (command.backend != "auto" && command.backend != "validation") {
        error =
            "supported backends are auto and validation; native window backends are CONTRACT ONLY";
        return false;
      }
      if (command.backend == "auto")
        command.backend = "validation";
    } else {
      error = "unknown argument: " + std::string(argument);
      return false;
    }
  }
  return true;
}

void PrintUsage() {
  std::cout << "NexoraShowcase - C++ engine-owned Zig gameplay showcase\n"
               "Usage: NexoraShowcase.exe [options]\n\n"
               "Options:\n"
               "  --headless                 run the deterministic offscreen showcase\n"
               "  --validate-v1              include the V1 validation label in the report\n"
               "  --frames=N                 run N fixed/update frames (1..10000)\n"
               "  --report=PATH              write the JSON report to PATH\n"
               "  --no-reload                skip the transactional Zig state reload\n"
               "  --mode=headless            explicit headless mode\n"
               "  --scene=hub                run the Zig Showcase Hub scene\n"
               "  --backend=auto|validation  use the validation offscreen backend\n";
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

void *Allocate(void *, std::uint64_t size, std::uint64_t alignment) {
  if (size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0)
    return nullptr;
  try {
    return ::operator new(static_cast<std::size_t>(size),
                          std::align_val_t{static_cast<std::size_t>(alignment)});
  } catch (const std::bad_alloc &) {
    return nullptr;
  }
}

void Deallocate(void *, void *allocation, std::uint64_t, std::uint64_t alignment) {
  if (allocation != nullptr)
    ::operator delete(allocation, std::align_val_t{static_cast<std::size_t>(alignment)});
}

NexoraGameplayHostV3 MakeHost(ShowcaseHostContext &context) {
  return {sizeof(NexoraGameplayHostV3),
          NEXORA_GAMEPLAY_ABI_VERSION,
          NEXORA_GAMEPLAY_CAPABILITY_HOST_ALLOCATOR,
          &context,
          &Log,
          &ReadComponent,
          &WriteComponent,
          &Allocate,
          &Deallocate};
}

bool NearlyEqual(double left, double right) { return std::abs(left - right) < 0.000001; }

bool RunShowcase(const CommandLine &command, core::Engine &engine, ShowcaseRun &result,
                 std::string &error) {
  game::GameWorld world;
  const auto scene = world.LoadScene("Zig Showcase Hub", true);
  if (!world.ActivateScene(scene)) {
    error = "failed to activate the showcase scene";
    return false;
  }

  game::EntitySpawnDescriptor camera_descriptor;
  camera_descriptor.transform = {0.0, 2.0, 6.0};
  camera_descriptor.camera = runtime::CameraComponent{60.0, 0.1, 100.0};
  (void)world.SpawnEntity(scene, camera_descriptor);

  game::EntitySpawnDescriptor light_descriptor;
  light_descriptor.transform = {2.0, 4.0, 2.0};
  light_descriptor.light = runtime::LightComponent{2.0F};
  (void)world.SpawnEntity(scene, light_descriptor);

  const auto spawn_cube = [&world, scene](runtime::Transform transform, runtime::Id mesh) {
    game::EntitySpawnDescriptor descriptor;
    descriptor.transform = transform;
    descriptor.mesh_renderer = runtime::MeshComponent{mesh, runtime::MaterialComponent{0x1001}};
    return world.SpawnEntity(scene, descriptor);
  };
  const auto primary_entity = spawn_cube({0.0, 0.0, 0.0}, 0x2001);
  (void)spawn_cube({-2.0, 0.0, 0.0}, 0x2002);
  (void)spawn_cube({2.0, 0.0, 0.0}, 0x2003);

  ShowcaseHostContext context{&world, engine.Services().log, primary_entity};
  runtime::GameplayModuleHost module(MakeHost(context));
  if (!module.Load(NexoraGameModuleLoad)) {
    error = "Zig gameplay module failed to load or start";
    return false;
  }
  result.module_loaded = module.IsLoaded();

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

  for (std::size_t frame = 0; frame < command.frames; ++frame) {
    engine.BeginFrame();
    if (!module.FixedUpdate(kFixedDeltaSeconds) || !module.Update(kFixedDeltaSeconds)) {
      error = "Zig gameplay callback failed during the fixed/update loop";
      device->DestroyTexture(output);
      return false;
    }
    world.EndFrame();
  }

  const auto snapshot_before_reload = world.GetEntity(primary_entity);
  if (!snapshot_before_reload) {
    error = "the Zig-controlled entity disappeared from the showcase world";
    device->DestroyTexture(output);
    return false;
  }

  bool reload_ok = true;
  if (command.reload)
    reload_ok = module.Reload(NexoraGameModuleLoad);
  const auto reload = module.GetReloadStats();
  const auto render = runtime::RenderSceneFrame(world.InternalWorld(), *device, output,
                                                output_descriptor, pipeline);
  const auto diagnostics = device->Diagnostics();
  device->WaitIdle();
  device->DestroyTexture(output);

  const auto snapshot = world.GetEntity(primary_entity);
  const auto visible_meshes = world.Query(scene, game::GameWorld::kQueryMeshRenderer).size();
  const bool lifecycle_ok = context.received_zig_log && module.IsLoaded() &&
                            NexoraGameModuleStartCount() >= 1 &&
                            NexoraGameModuleFixedUpdateCount() == command.frames &&
                            NexoraGameModuleUpdateCount() == command.frames;
  const bool scene_ok = snapshot.has_value() && visible_meshes == 3 &&
                        NearlyEqual(snapshot->transform.x, command.frames * kFixedDeltaSeconds);
  const bool render_ok = render.has_value() && render->visible_meshes == visible_meshes &&
                         render->passes == 5 && render->barriers == 6 &&
                         diagnostics.validation_errors == 0;
  const bool migration_ok =
      !command.reload || (reload_ok && reload.successful_reloads == 1 && reload.migrated_bytes > 0);
  module.Unload();
  const bool shutdown_ok = !module.IsLoaded();

  result.engine_initialized = engine.IsInitialized();
  result.lifecycle_ok = lifecycle_ok;
  result.scene_ok = scene_ok;
  result.render_ok = render_ok;
  result.reload_ok = migration_ok;
  result.shutdown_ok = shutdown_ok;
  result.frames = command.frames;
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
         << "    \"headless_validation\": \"IMPLEMENTED\",\n"
         << "    \"transactional_reload\": \"" << (run.reload_ok ? "IMPLEMENTED" : "FAIL")
         << "\",\n"
         << "    \"windowed_native_backend\": \"CONTRACT ONLY\"\n"
         << "  },\n"
         << "  \"lifecycle\": {\n"
         << "    \"engine_initialized\": " << run.engine_initialized << ",\n"
         << "    \"module_callbacks\": " << run.lifecycle_ok << ",\n"
         << "    \"module_unloaded_before_engine_shutdown\": " << run.shutdown_ok << ",\n"
         << "    \"frames\": " << run.frames << ",\n"
         << "    \"start_count\": " << NexoraGameModuleStartCount() << ",\n"
         << "    \"fixed_update_count\": " << NexoraGameModuleFixedUpdateCount() << ",\n"
         << "    \"update_count\": " << NexoraGameModuleUpdateCount() << ",\n"
         << "    \"elapsed_seconds\": " << NexoraGameModuleElapsedSeconds() << "\n"
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
         << "    \"zig_log_received\": " << run.received_zig_log << "\n"
         << "  },\n"
         << "  \"render_evidence\": {\n"
         << "    \"backend\": \"Validation\",\n"
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
