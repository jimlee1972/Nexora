#include "TwoDPlayer.h"
#include "Nexora/Core/Engine.h"
#include "Nexora/Game/GameplayHostBridge.h"
#include "Nexora/Runtime/GameplayAudio.h"
#include "Nexora/Runtime/GameplayGamepad.h"
#include "Nexora/Runtime/GameplayInput.h"
#include "Nexora/Runtime/GameplayLifecycle.h"
#include "Nexora/Runtime/GameplayModuleHost.h"
#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/Runtime/GameplayStorage.h"
#include "Nexora/TwoD/EffectsRendering.h"
#include "Nexora/TwoD/GameplayAssets.h"
#include "Nexora/TwoD/GameplayBridge.h"
#include "Nexora/TwoD/GameplayContent.h"
#include "Nexora/TwoD/GameplayDisplay.h"
#include "Nexora/TwoD/GameplayUi.h"
#include "Project.h"
#include "TwoDValidation.h"
#include "UiInputGate.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace {
using namespace nexora;
void Check(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
struct Context {
  runtime::World &world;
  twod::GameplayBridge &bridge;
  runtime::Id scene;
  runtime::GameplayServices &services;
  NexoraInputSnapshot input{};
  std::size_t allocated{}, freed{};
  std::string_view scene_name{"Zig 2D"};
};
template <class F> int32_t Call(void *p, F &&f) noexcept {
  if (!p)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  try {
    return f(*static_cast<Context *>(p));
  } catch (...) {
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  }
}
NexoraGameplayHostV3 Host(Context &c) {
  NexoraGameplayHostV3 h{};
  h.struct_size = sizeof(h);
  h.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  h.capabilities = NEXORA_GAMEPLAY_CAPABILITY_HOST_ALLOCATOR |
                   NEXORA_GAMEPLAY_CAPABILITY_SCENE_API | NEXORA_GAMEPLAY_CAPABILITY_2D_API |
                   NEXORA_GAMEPLAY_CAPABILITY_SERVICES_API |
                   NEXORA_GAMEPLAY_CAPABILITY_POINTER_INPUT;
  h.context = &c;
  h.log = [](void *, uint32_t, const char *text, uint32_t length) {
    if (text && length <= 65536)
      std::cerr.write(text, length) << '\n';
  };
  h.capture_pointer_input = [](void *p, uint32_t user, NexoraPointerInputSnapshotV1 *out) {
    return Call(p, [&](auto &s) -> int32_t {
      if (!out)
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      if (out->struct_size < sizeof(*out))
        return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
      NexoraGameplayPointerStateV1 pointer{};
      pointer.struct_size = sizeof(pointer);
      pointer.abi_version = 1;
      const auto &api = s.services.input;
      const auto result = api.read_pointer(api.context, user, &pointer);
      if (result != 0)
        return result;
      const NexoraPointerInputSnapshotV1 snapshot{out->struct_size,
                                                  1,
                                                  pointer.sequence,
                                                  pointer.delta_x,
                                                  pointer.delta_y,
                                                  pointer.wheel_x,
                                                  pointer.wheel_y,
                                                  pointer.buttons,
                                                  0};
      std::memcpy(out, &snapshot, sizeof(snapshot));
      return NEXORA_GAMEPLAY_OK;
    });
  };
  h.get_services_api = [](void *p, NexoraGameplayServicesApiV1 *api) {
    return Call(p, [&](auto &s) { return s.services.GetApi(api); });
  };
  h.get_twod_api = [](void *p, NexoraTwoDApiV1 *api) {
    return Call(p, [&](auto &s) { return s.bridge.GetApi(api); });
  };
  h.allocate = [](void *p, uint64_t owner, uint64_t size, uint64_t alignment) -> void * {
    if (!p || owner != NEXORA_ALLOCATION_OWNER_GAMEPLAY_STATE || !size || size > 16 * 1024 * 1024 ||
        !alignment || alignment > 4096 || (alignment & (alignment - 1)))
      return nullptr;
    auto *memory =
        ::operator new(static_cast<std::size_t>(size),
                       std::align_val_t{static_cast<std::size_t>(alignment)}, std::nothrow);
    if (memory)
      ++static_cast<Context *>(p)->allocated;
    return memory;
  };
  h.deallocate = [](void *p, uint64_t, void *memory, uint64_t, uint64_t alignment) {
    if (!memory)
      return;
    ++static_cast<Context *>(p)->freed;
    ::operator delete(memory, std::align_val_t{static_cast<std::size_t>(alignment)});
  };
  h.read_component = [](void *p, uint64_t id, uint64_t type, void *data, uint32_t size) {
    return Call(
        p, [&](auto &s) { return game::ReadGameplayComponent(s.world, id, type, data, size); });
  };
  h.write_component = [](void *p, uint64_t id, uint64_t type, const void *data, uint32_t size) {
    return Call(
        p, [&](auto &s) { return game::WriteGameplayComponent(s.world, id, type, data, size); });
  };
  h.load_scene = [](void *p, const char *name, uint32_t length, uint32_t persistent, uint64_t *id) {
    return Call(p, [&](auto &s) -> int32_t {
      if (!name || !id || length > 256 || persistent > 1)
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      if (std::string_view(name, length) != s.scene_name)
        return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
      *id = s.scene;
      return NEXORA_GAMEPLAY_OK;
    });
  };
  h.activate_scene = [](void *p, uint64_t id) {
    return Call(p, [&](auto &s) -> int32_t {
      return id == s.scene && s.world.Activate(id) ? NEXORA_GAMEPLAY_OK
                                                   : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    });
  };
  h.spawn_entity = [](void *p, uint64_t scene, const NexoraEntitySpawnDescriptor *d, uint64_t *id) {
    return Call(p, [&](auto &s) -> int32_t {
      if (!d || !id || scene != s.scene)
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      if (d->struct_size < sizeof(*d) || d->components)
        return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
      const runtime::Transform transform{d->position.x, d->position.y, d->position.z};
      if (!runtime::IsValidTransform(transform))
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      auto &entity = s.world.CreateEntity(scene);
      entity.transform = transform;
      *id = entity.id;
      return NEXORA_GAMEPLAY_OK;
    });
  };
  h.despawn_entity = [](void *p, uint64_t id) {
    return Call(p, [&](auto &s) -> int32_t {
      runtime::WorldCommandBuffer commands;
      commands.DestroyEntity(id);
      if (!commands.Apply(s.world))
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      for (const auto removed : commands.LastDestroyed())
        s.bridge.ForgetEntity(removed);
      return NEXORA_GAMEPLAY_OK;
    });
  };
  h.capture_input = [](void *p, uint32_t user, NexoraInputSnapshot *v) {
    return Call(p, [&](auto &s) -> int32_t {
      if (user || !v)
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      *v = s.input;
      return NEXORA_GAMEPLAY_OK;
    });
  };
  return h;
}
} // namespace

int nexora::player::RunTwoDPlayer(int argc, char **argv, NexoraGameModuleLoadV3Fn builtin) {
  try {
    bool native{}, interactive{}, requireMovement{}, audioOutput{}, framesSpecified{}, gpuTiming{};
    unsigned audioOpenAttempts{}, audioOpenSuccesses{}, audioPumpFailures{};
    auto backend = Nexora::Presentation::SurfaceBackend::Automatic;
    unsigned frames = 60;
    std::set<std::string_view> options;
    std::string report, library, saveDirectory, validationPackage, projectFile;
    for (int i = 1; i < argc; ++i) {
      const std::string_view arg(argv[i]);
      Check(options.insert(arg == "--run-2d-project" ? "--project" : arg).second,
            "duplicate argument");
      if (arg == "--native")
        native = true;
      else if (arg == "--gpu-timing")
        gpuTiming = true;
      else if (arg == "--audio")
        audioOutput = true;
      else if (arg == "--backend" && i + 1 < argc) {
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
      } else if (arg == "--interactive") {
        native = interactive = true;
      } else if (arg == "--require-movement")
        requireMovement = true;
      else if ((arg == "--frames" || arg == "--report" || arg == "--module" ||
                arg == "--save-directory" || arg == "--validation-package" || arg == "--project" ||
                arg == "--run-2d-project") &&
               i + 1 < argc) {
        const std::string_view value(argv[++i]);
        if (arg == "--project" || arg == "--run-2d-project") {
          Check(projectFile.empty(), "duplicate project argument");
          projectFile = value;
        } else if (arg == "--validation-package")
          validationPackage = value;
        else if (arg == "--save-directory")
          saveDirectory = value;
        else if (arg == "--report")
          report = value;
        else if (arg == "--module")
          library = value;
        else {
          framesSpecified = true;
          const auto parsed = std::from_chars(value.data(), value.data() + value.size(), frames);
          Check(parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() &&
                    frames <= 100000,
                "invalid frame count");
        }
      } else
        throw std::runtime_error("unknown or incomplete argument");
    }
    if (interactive && !framesSpecified)
      frames = 0;
    std::optional<nexora::player::GameProject> project;
    if (!projectFile.empty()) {
      Check(library.empty() && validationPackage.empty() && !requireMovement,
            "project mode does not accept sample/module overrides");
      project = nexora::player::GameProject::Load(nexora::player::GamePath(projectFile));
      const auto path = project->module.u8string();
      library.assign(path.begin(), path.end());
      if (!framesSpecified) {
        native = interactive = true;
        frames = 0;
      }
    }
    Check(native || backend == Nexora::Presentation::SurfaceBackend::Automatic,
          "explicit backend requires native presentation");
    Check(builtin || project.has_value(), "standalone player requires --project MANIFEST");
    Check(frames || interactive, "zero frames requires interactive mode");
    core::Engine engine;
    core::EngineConfiguration configuration;
    configuration.worker_count = 2;
    const bool validation = !validationPackage.empty();
    if (project)
      configuration.content_root = project->root;
    if (validation) {
      Check(library.empty() && !saveDirectory.empty(),
            "validation package requires the static sample and an explicit save directory");
      validationPackage = std::filesystem::canonical(validationPackage).string();
      configuration.content_root = validationPackage;
    }
    engine.Initialize(configuration);
    runtime::World world(runtime::WorldKind::Play);
    const auto sceneId = world.LoadScene(project ? project->scene : "Zig 2D");
    twod::SpriteScene scene(world, sceneId);
    twod::Tilemap tiles;
    tiles.atlas = project ? project->tile_atlas : twod::AssetId{1, 2};
    const bool hasTiles = !project || tiles.atlas.high || tiles.atlas.low;
    twod::InstancePool pool(project ? project->pool_instances : 1024);
    std::shared_ptr<const twod::Atlas> atlas;
    std::shared_ptr<const twod::Clip> clip;
    if (!project) {
      std::array<twod::Image, 2> images{{{1, 8, 8, twod::Bytes(256, std::byte{255})},
                                         {2, 8, 8, twod::Bytes(256, std::byte{255})}}};
      for (std::size_t i = 0; i < images[0].rgba.size(); i += 4) {
        images[0].rgba[i + 1] = std::byte{64};
        images[0].rgba[i + 2] = std::byte{32};
        images[1].rgba[i] = std::byte{32};
        images[1].rgba[i + 2] = std::byte{64};
      }
      auto packed = twod::PackAtlas({1, 2}, images, 32, 32, true);
      Check(bool(packed), "sample atlas");
      atlas = std::make_shared<const twod::Atlas>(std::move(*packed));
      clip = std::make_shared<const twod::Clip>(
          twod::Clip{{3, 4}, atlas->id, true, {{1, 0.1, "step"}, {2, 0.1, "step"}}});
      if (validation) {
        const auto cookedAtlas = twod_validation::Cook(*engine.Services().vfs, validationPackage,
                                                       "assets/atlas.nxcook", "sprite_atlas");
        const auto cookedClip = twod_validation::Cook(*engine.Services().vfs, validationPackage,
                                                      "assets/clip.nxcook", "sprite_clip");
        auto decodedAtlas = twod::DecodeAtlas(cookedAtlas.payload);
        auto decodedClip = twod::DecodeClip(cookedClip.payload);
        Check(decodedAtlas && decodedClip && decodedAtlas->id == twod::AssetId{1, 2} &&
                  decodedClip->id == twod::AssetId{3, 4} && cookedAtlas.id == decodedAtlas->id &&
                  cookedClip.id == decodedClip->id && cookedAtlas.dependencies.empty() &&
                  cookedClip.dependencies == std::vector<twod::AssetId>{decodedClip->atlas},
              "validation cooked identity/dependency closure");
        atlas = std::make_shared<const twod::Atlas>(std::move(*decodedAtlas));
        clip = std::make_shared<const twod::Clip>(std::move(*decodedClip));
      }
    }
    twod::GameplayContent content(project ? project->content_limits
                                          : twod::GameplayContent::Limits{});
    NexoraGameplayContentApiV1 contentApi{};
    contentApi.struct_size = sizeof(contentApi);
    contentApi.abi_version = 1;
    Check(content.GetApi(&contentApi) == 0, "content service negotiation");
    uint64_t assetHigh{}, assetLow{};
    std::size_t projectBytes{};
    const auto readProjectAsset = [&](const nexora::player::ProjectAsset &asset) {
      auto bytes = twod_validation::Read(*engine.Services().vfs, project->root, asset.path.c_str());
      Check(bytes.size() <= 64 * 1024 * 1024 - projectBytes, "project asset payload budget");
      projectBytes += bytes.size();
      return bytes;
    };
    if (project) {
      std::set<std::pair<uint64_t, uint64_t>> identities;
      for (const auto kind : {std::string_view("atlas"), std::string_view("clip")})
        for (const auto &asset : project->assets) {
          if (asset.deferred || asset.kind != kind)
            continue;
          const auto cooked = runtime::AssetCooker::Deserialize(readProjectAsset(asset));
          Check(cooked && cooked->type == (kind == "atlas" ? "sprite_atlas" : "sprite_clip") &&
                    identities.emplace(cooked->id.high, cooked->id.low).second,
                "project cooked hash/type/identity");
          if (kind == "atlas") {
            const auto decoded = twod::DecodeAtlas(cooked->payload);
            Check(decoded && decoded->id == cooked->id && cooked->dependencies.empty(),
                  "project Atlas identity");
          } else {
            const auto decoded = twod::DecodeClip(cooked->payload);
            Check(decoded && decoded->id == cooked->id &&
                      cooked->dependencies == std::vector<twod::AssetId>{decoded->atlas},
                  "project Clip dependency");
          }
          const auto registered =
              kind == "atlas"
                  ? contentApi.register_atlas(contentApi.context, cooked->payload.data(),
                                              cooked->payload.size(), &assetHigh, &assetLow)
                  : contentApi.register_clip(contentApi.context, cooked->payload.data(),
                                             cooked->payload.size(), &assetHigh, &assetLow);
          Check(registered == 0, "project content registration");
        }
    } else {
      const auto atlasBytes = twod::EncodeAtlas(*atlas);
      const auto clipBytes = twod::EncodeClip(*clip);
      Check(contentApi.register_atlas(contentApi.context, atlasBytes.data(), atlasBytes.size(),
                                      &assetHigh, &assetLow) == 0,
            "sample cooked Atlas registration");
      Check(contentApi.register_clip(contentApi.context, clipBytes.data(), clipBytes.size(),
                                     &assetHigh, &assetLow) == 0,
            "sample cooked Clip registration");
    }
    Check(!hasTiles || !project || bool(content.ResolveAtlas(tiles.atlas)) ||
              std::any_of(project->assets.begin(), project->assets.end(),
                          [](const auto &a) { return a.deferred && a.kind == "atlas"; }),
          "project tile atlas is unavailable");
    twod::GameplayBridge bridge(
        world, scene, [&content](auto id) { return content.ResolveClip(id); },
        hasTiles ? &tiles : nullptr, &pool);
    runtime::GameplayInput input;
    runtime::NativeGameplayGamepad gamepads;
    runtime::GameplayLifecycle lifecycle(
        [](void *p, uint32_t d, float l, float h) {
          return static_cast<runtime::NativeGameplayGamepad *>(p)->SetRumble(d, l, h);
        },
        &gamepads);
    std::array<uint64_t, 4> disconnectSequences{};
    runtime::GameplayAudio audio(
        48000, 64, project ? project->audio_limits : runtime::GameplayAudio::Limits{});
    twod::GameplayUi ui;
    if (project)
      for (const auto &asset : project->assets) {
        if (asset.deferred)
          continue;
        if (asset.kind == "wave")
          Check(audio.RegisterWave(asset.id, readProjectAsset(asset)), "project WAV admission");
        else if (asset.kind == "font")
          Check(ui.RegisterFont(nexora::player::ReadGameFont(readProjectAsset(asset), asset.id)),
                "project font admission");
      }
    std::vector<twod::GameplayAssetFile> catalog;
    if (project)
      for (const auto &asset : project->assets)
        if (asset.deferred) {
          const auto size = std::filesystem::file_size(project->File(asset.path));
          Check(size && size <= 64ULL * 1024 * 1024, "deferred asset file budget");
          catalog.push_back({asset.kind == "atlas"  ? 1U
                             : asset.kind == "clip" ? 2U
                                                    : 3U,
                             size, asset.path});
        }
    twod::GameplayAssets assets(
        content, std::move(catalog),
        [&](const auto &file) {
          // Revalidate containment and size at request time; no game-selected native path crosses
          // ABI.
          Check(project.has_value() &&
                    std::filesystem::file_size(project->File(file.path)) == file.bytes,
                "deferred file changed size");
          auto result = engine.Services().vfs->ReadRange("content://" + file.path, 0, file.bytes);
          Check(result.status == core::ReadResult::Status::Completed &&
                    result.bytes.size() == file.bytes &&
                    std::filesystem::file_size(project->File(file.path)) == file.bytes,
                "deferred bounded local read");
          return std::move(result.bytes);
        },
        project ? project->staging_bytes : 64ULL * 1024 * 1024);
    runtime::GameplayServices services;
    services.assets.struct_size = sizeof(services.assets);
    services.assets.abi_version = 1;
    Check(assets.GetApi(&services.assets) == 0, "asset service negotiation");
    services.content = contentApi;
    services.input = input.Api();
    services.lifecycle = lifecycle.Api();
    services.audio = audio.Api();
    services.audio_state = audio.StateApi();
    const auto bind = [&](uint32_t action, uint32_t device, uint32_t control, float scale = 1.F,
                          float deadzone = 0.F) {
      NexoraGameplayInputBindingV1 binding{};
      binding.struct_size = sizeof(binding);
      binding.abi_version = 1;
      binding.device = device;
      binding.control = control;
      binding.scale = scale;
      binding.deadzone = deadzone;
      Check(services.input.bind(services.input.context, 0, action, &binding) == 0, "UI binding");
    };
    // Sample-host actions 250..256 can be rebound through the existing input service.
    bind(250, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_D);
    bind(250, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_A, -1);
    bind(251, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_W);
    bind(251, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_S, -1);
    bind(252, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_SPACE);
    bind(250, NEXORA_INPUT_DEVICE_GAMEPAD, 32, 1, .2F);
    bind(251, NEXORA_INPUT_DEVICE_GAMEPAD, 33, -1, .2F);
    bind(252, NEXORA_INPUT_DEVICE_GAMEPAD, 0);
    bind(253, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_DOWN_ARROW);
    bind(253, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_TAB);
    bind(254, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_UP_ARROW);
    bind(255, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_ENTER);
    bind(256, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_ESCAPE);
    // Raw joystick button defaults are user-rebindable, rather than assuming a controller model.
    bind(253, NEXORA_INPUT_DEVICE_GAMEPAD, 13);
    bind(254, NEXORA_INPUT_DEVICE_GAMEPAD, 12);
    bind(255, NEXORA_INPUT_DEVICE_GAMEPAD, 0);
    bind(256, NEXORA_INPUT_DEVICE_GAMEPAD, 1);
    services.ui.struct_size = sizeof(services.ui);
    services.ui.abi_version = 1;
    Check(ui.GetApi(&services.ui) == 0, "UI service negotiation");
    services.ui_draw.struct_size = sizeof(services.ui_draw);
    services.ui_draw.abi_version = 1;
    Check(ui.GetDrawApi(&services.ui_draw) == 0, "UI draw service negotiation");
    std::unique_ptr<runtime::GameplayStorage> storage;
    if (!saveDirectory.empty() || project) {
      const auto directory = saveDirectory.empty() ? runtime::UserGameplaySaveDirectory(project->id)
                                                   : nexora::player::GamePath(saveDirectory);
      storage = std::make_unique<runtime::GameplayStorage>(directory, project ? project->id
                                                                              : "nexora.zig.twod");
      services.storage.struct_size = sizeof(services.storage);
      services.storage.abi_version = 1;
      Check(storage->GetApi(services.storage) == 0, "storage service negotiation");
    }
    if (validation) {
      Check(ui.RegisterFont(twod_validation::Font()), "validation raster font");
      const auto wave =
          twod_validation::Read(*engine.Services().vfs, validationPackage, "assets/tone.wav");
      Check(audio.RegisterWave(1, wave), "validation WAV admission");
    }
    const bool wantsAudio = audioOutput || (project && project->audio != "off");
    const bool audioRequired = audioOutput || (project && project->audio == "required");
    if (wantsAudio) {
      std::string reason;
      ++audioOpenAttempts;
      const bool opened = audio.OpenDefaultOutput(reason);
      if (opened)
        ++audioOpenSuccesses;
      if (audioRequired)
        Check(opened, reason.c_str());
      else if (!opened)
        std::cerr << reason << '\n';
      audioOutput = opened;
      if (validation) {
        const NexoraAudioPlayDesc desc{1, NEXORA_AUDIO_BUS_SFX, 1, 1};
        std::uint64_t voice{};
        Check(services.audio.play(services.audio.context, &desc, &voice) == 0,
              "validation device audio play");
      }
    }
    Nexora::Presentation::RenderSurfaceResult surface;
    if (native) {
      Nexora::Presentation::RenderSurfaceDescriptor d;
      d.title = project ? std::string_view(project->title) : std::string_view("Nexora Zig 2D");
      d.width = project ? project->width : 640;
      d.height = project ? project->height : 480;
      d.resizable = project ? project->resizable : true;
      d.borderless = project && project->borderless;
      d.fitToWorkArea = true;
      d.srgbUiOutput = true;
      d.enableGpuTiming = gpuTiming;
      d.backend = backend;
      surface = Nexora::Presentation::CreateRenderSurface(d);
      if (!surface)
        throw std::runtime_error("native 2D surface: " + surface.reason);
    }
    twod::GameplayEffects effects(
        native && surface.surface->SupportsTwoDLinear()
            ? NEXORA_TWOD_EFFECTS_LIGHTING |
                  (surface.surface->SupportsTwoDShadows() ? NEXORA_TWOD_EFFECTS_SHADOWS : 0) |
                  (surface.surface->SupportsTwoDBloom() ? NEXORA_TWOD_EFFECTS_BLOOM : 0)
            : 0,
        [&](uint32_t kind, uint64_t handle) {
          return kind == NEXORA_TWOD_EFFECTS_ENTITY && world.FindEntity(handle) &&
                 scene.Find(handle);
        },
        [&](uint64_t high, uint64_t low, uint32_t region) {
          const auto binding = content.ResolveAtlas({high, low});
          return binding && !binding->atlas->nearest && binding->atlas->Find(region);
        },
        [&](twod::EffectsPoolIdentity h) {
          return pool.Find({h.owner, h.index, h.generation}) != nullptr;
        },
        [&](NexoraTwoDEffectsDiagnosticsV1 &out) {
          if (!native)
            return false;
          const auto stats = surface.surface->Diagnostics();
          const auto &frame = stats.twoDEffects;
          out.completed_frame = frame.completedFrame;
          out.effects_revision = frame.effectsRevision;
          out.timing_mask = frame.timingMask;
          out.timing_source = frame.timingSource;
          out.timing_status = frame.completedFrame ? frame.timingStatus : (gpuTiming ? 1U : 0U);
          out.shadow_dispatches = frame.work[0];
          out.world_draws = frame.work[1];
          out.bloom_passes = frame.work[2];
          out.composite_draws = frame.work[3];
          out.shadow_ms = frame.milliseconds[0];
          out.world_ms = frame.milliseconds[1];
          out.bloom_ms = frame.milliseconds[2];
          out.composite_ms = frame.milliseconds[3];
          out.scene_target_bytes = stats.twoDLinearTargetBytes;
          out.shadow_buffer_bytes = stats.twoDShadowBufferBytes;
          out.bloom_target_bytes = stats.twoDBloomTargetBytes;
          out.upload_capacity_bytes = stats.twoDUploadCapacityBytes;
          out.peak_effect_bytes = stats.twoDEffectsPeakBytes;
          out.texture_count = stats.twoDTextureCount;
          out.peak_texture_count = stats.twoDPeakTextureCount;
          return true;
        });
    services.twod_effects = effects.Api();
    twod::GameplayDisplay display([&](const NexoraDisplayWindowV1 &request) -> int32_t {
      if (!native)
        return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
      const auto result = surface.surface->ConfigureWindow(
          request.width, request.height, request.mode == 1, request.resizable != 0);
      if (result == Nexora::Window::WindowError::None)
        return NEXORA_GAMEPLAY_OK;
      if (result == Nexora::Window::WindowError::Unsupported)
        return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    });
    const auto updateDisplay = [&] {
      const auto info =
          native ? surface.surface->FrameInfo()
                 : Nexora::Presentation::SurfaceFrameInfo{project ? project->width : 640U,
                                                          project ? project->height : 480U, 1.F};
      display.BeginFrame(info.width, info.height, info.dpiScale,
                         native && surface.surface->Borderless() ? 1U : 0U);
    };
    updateDisplay();
    services.display = display.Api();
    Context context{world, bridge, sceneId, services};
    if (project)
      context.scene_name = project->scene;
    runtime::GameplayModuleHost module(Host(context));
    Check(library.empty() ? (builtin && module.Load(builtin))
                          : module.Load(nexora::player::GamePath(library)),
          "Zig module load");
    if (!project)
      Check(scene.Sprites().size() == 1 && pool.Size() == 32, "Zig authored content");
    const auto entity = scene.Sprites().empty() ? runtime::Id{} : scene.Sprites().begin()->first;
    const auto restorePosition = [&](const std::array<double, 2> &xy) {
      twod_validation::Restore(world, entity, xy);
      NexoraTwoDApiV1 api{};
      api.struct_size = sizeof(api);
      api.abi_version = 1;
      Check(bridge.GetApi(&api) == 0 && api.rebuild_queries(api.context, 0) == 0,
            "validation restored query snapshot");
    };
    double initialX{};
    bool resumed{};
    if (validation) {
      if (const auto saved = twod_validation::Load(services.storage)) {
        restorePosition(*saved);
        initialX = (*saved)[0];
        resumed = true;
      }
    }
    const twod::AtlasResolver resolver = [&content](auto id) { return content.ResolveAtlas(id); };
    std::array<bool, static_cast<std::size_t>(Nexora::Window::Key::F12) + 1> keys{};
    unsigned rendered{}, movementFrames{}, uiFrames{}, assetReloads{};
    std::uint64_t submittedAudioFrames{};
    unsigned uiSaves{}, uiOpens{};
    unsigned focusLost{}, focusGained{}, keyPresses{}, mousePresses{}, wheelEvents{};
    unsigned unfocusedMovementFrames{};
    float initialDpi{}, minimumDpi{}, maximumDpi{};
    std::size_t visible{};
    twod::FixedStepper step;
    auto lastTime = std::chrono::steady_clock::now();
    auto nextAudioAttempt = lastTime + std::chrono::seconds(1);
    bool reloaded{}, focused{true}, minimized{}, suspended{};
    double audioFrames{};
    twod::GameplayUiInput uiInput;
    nexora::player::UiInputGate uiGate;
    uint32_t previousGamepadMask{};
    for (unsigned frame = 0; !frames || frame < frames; ++frame) {
      const auto effectsState = effects.BeginFrame();
      assets.Tick();
      const auto retiredContent = content.CollectRetiredTextures();
      if (native && !retiredContent.empty())
        Check(surface.surface->ReleaseSpriteTextures(retiredContent) ==
                  Nexora::Presentation::SurfaceStatus::Ready,
              "content texture retirement");
      const auto retiredUi = ui.CollectRetiredTextures();
      if (native && !retiredUi.empty())
        Check(surface.surface->ReleaseSpriteTextures(retiredUi) ==
                  Nexora::Presentation::SurfaceStatus::Ready,
              "UI texture retirement");
      Check(input.BeginFrame(), "input frame");
      if (lifecycle.ExitRequested())
        break;
      bool canPresent = true;
      bool interruptedBatch = false;
      if (!lifecycle.Tick())
        std::cerr << "rumble stop pending retry\n";
      uiInput.pointer_pressed = uiInput.pointer_released = false;
      if (native) {
        const auto status = surface.surface->BeginFrame();
        if (surface.surface->CloseRequested()) {
          interruptedBatch = true;
          (void)lifecycle.RequestClose();
          if (lifecycle.ExitRequested() || !surface.surface->CancelCloseRequest())
            break;
          canPresent = false; // BeginFrame returned before acquiring; retry next frame.
        }
        const auto dpi = surface.surface->FrameInfo().dpiScale;
        if (dpi > 0) {
          if (initialDpi == 0)
            initialDpi = minimumDpi = maximumDpi = dpi;
          minimumDpi = std::min(minimumDpi, dpi);
          maximumDpi = std::max(maximumDpi, dpi);
        }
        for (const auto &event : surface.surface->Events()) {
          using Event = Nexora::Window::WindowEventType;
          if (event.type == Event::Minimized || event.type == Event::SuspendRequested ||
              (event.type == Event::Resized && (!event.width || !event.height))) {
            interruptedBatch = true;
            Check(input.SetFocused(false), "interrupted input reset");
            keys.fill(false);
            uiInput.pointer_down = uiInput.pointer_pressed = uiInput.pointer_released = false;
          }
          if (event.type == Event::Minimized || event.type == Event::Restored) {
            minimized = event.type == Event::Minimized;
            (void)lifecycle.Minimize(minimized);
          }
          if (event.type == Event::Resized) {
            minimized = !event.width || !event.height;
            (void)lifecycle.Minimize(minimized);
          }
          if (event.type == Event::SuspendRequested || event.type == Event::Resumed) {
            suspended = event.type == Event::SuspendRequested;
            (void)lifecycle.Suspend(suspended);
          }
          if (event.type == Nexora::Window::WindowEventType::FocusChanged) {
            focused = event.value0 != 0;
            if (focused)
              ++focusGained;
            else
              ++focusLost;
            interruptedBatch |= !focused;
            (void)lifecycle.Focus(focused);
            Check(input.SetFocused(focused && !minimized && !suspended), "input focus");
            uiInput.focused = focused;
            if (!focused) {
              keys.fill(false);
              uiInput.pointer_down = false;
              uiInput.pointer_pressed = uiInput.pointer_released = false;
            }
          }
          // Restore routing at the state transition, before any later key in this batch.
          Check(input.SetFocused(focused && !minimized && !suspended), "event input routing");
          if (event.type == Nexora::Window::WindowEventType::Pointer) {
            uiInput.pointer_x = static_cast<float>(event.value0);
            uiInput.pointer_y = static_cast<float>(event.value1);
            Check(input.SubmitPointer(0, uiInput.pointer_x, uiInput.pointer_y), "pointer input");
          }
          if (event.type == Nexora::Window::WindowEventType::Wheel) {
            ++wheelEvents;
            Check(input.SubmitPointer(0, uiInput.pointer_x, uiInput.pointer_y, event.value0,
                                      event.value1),
                  "wheel input");
          }
          if (event.type == Nexora::Window::WindowEventType::PointerButton && focused &&
              event.value0 >= 0 && event.value0 < 5) {
            if (event.value1)
              ++mousePresses;
            Check(input.Submit(0, NEXORA_INPUT_DEVICE_MOUSE, 0, static_cast<uint32_t>(event.value0),
                               event.value1 ? 1.F : 0.F),
                  "mouse input");
            if (event.value0 == 0) {
              uiInput.pointer_pressed |= event.value1 && !uiInput.pointer_down;
              uiInput.pointer_released |= !event.value1 && uiInput.pointer_down;
              uiInput.pointer_down = event.value1 != 0;
            }
          }
          if (event.type == Nexora::Window::WindowEventType::Key && event.value0 >= 0 &&
              static_cast<std::size_t>(event.value0) < keys.size())
            if (focused) {
              if (event.value1)
                ++keyPresses;
              keys[static_cast<std::size_t>(event.value0)] = event.value1 != 0;
              Check(input.Submit(0, NEXORA_INPUT_DEVICE_KEYBOARD, 0,
                                 static_cast<uint32_t>(event.value0), event.value1 ? 1.F : 0.F),
                    "key input");
            }
        }
        updateDisplay();
        if (status == Nexora::Presentation::SurfaceStatus::ZeroExtent ||
            status == Nexora::Presentation::SurfaceStatus::Occluded ||
            status == Nexora::Presentation::SurfaceStatus::OutOfDate) {
          canPresent = false;
          std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
        Check(!canPresent || status == Nexora::Presentation::SurfaceStatus::Ready,
              "surface acquire");
        (void)nexora::player::UiInputGate::PollGamepads(input, focused && !minimized && !suspended,
                                                        [&] { return gamepads.Poll(input); });
        const auto connected = gamepads.ConnectedMask();
        interruptedBatch |= connected != previousGamepadMask; // Reconnect also needs fresh release.
        previousGamepadMask = connected;
        (void)lifecycle.Devices(connected, gamepads.RumbleMask());
        for (uint32_t id = 0; id < 4; ++id) {
          const auto sequence = gamepads.DisconnectSequences()[id];
          if (sequence != disconnectSequences[id]) {
            interruptedBatch = true;
            (void)lifecycle.Disconnected(id);
            disconnectSequences[id] = sequence;
          }
        }
        const bool foreground = focused && !minimized && !suspended;
        Check(
            nexora::player::UiInputGate::FinishInput(input, uiInput, foreground, interruptedBatch),
            "lifecycle input routing");
        if (!foreground)
          keys.fill(false);
        // Apply() suppresses UI for this batch; keep fresh post-focus gameplay controls intact.
      }
      if (!native)
        updateDisplay();
      const auto &displayState = display.State();
      const auto actionState = [&](uint32_t action) {
        NexoraGameplayActionStateV1 state{};
        state.struct_size = sizeof(state);
        state.abi_version = 1;
        Check(services.input.read_action(services.input.context, 0, action, &state) == 0,
              "UI action");
        return state;
      };
      const auto pressed = [&](uint32_t action) { return actionState(action).pressed != 0; };
      const bool neutral = !uiInput.pointer_down && !actionState(253).held &&
                           !actionState(254).held && !actionState(255).held &&
                           !actionState(256).held;
      uiInput.navigate = pressed(253) ? 1 : pressed(254) ? -1 : 0;
      uiInput.activate = pressed(255);
      uiInput.cancel = pressed(256);
      uiGate.Apply(uiInput, interruptedBatch, neutral);
      auto logicalInput = uiInput;
      logicalInput.pointer_x /= displayState.dpi_scale;
      logicalInput.pointer_y /= displayState.dpi_scale;
      ui.BeginInput(logicalInput);
      ++context.input.sequence;
      const auto held = [&](Nexora::Window::Key key) {
        return keys[static_cast<std::size_t>(key)] ? 1.0 : 0.0;
      };
      context.input.move_x = interactive || project ? actionState(250).value : 1.0;
      context.input.move_y = interactive || project ? actionState(251).value : 0.0;
      if (!focused && (context.input.move_x || context.input.move_y))
        ++unfocusedMovementFrames;
      context.input.buttons = actionState(252).held ? 1U : 0U;
      if (context.input.move_x || context.input.move_y)
        ++movementFrames;
      const auto now = std::chrono::steady_clock::now();
      const double delta =
          interactive && !frames
              ? std::min(0.1, std::chrono::duration<double>(now - lastTime).count())
              : 1.0 / 60;
      lastTime = now;
      auto ticks = step.Advance(delta);
      Check(bool(ticks), "fixed step");
      if (module.SupportsFixedUpdate())
        for (uint64_t i = 0; i < ticks->steps; ++i)
          Check(module.FixedUpdate(step.StepSeconds()), "gameplay fixed update");
      Check(module.Update(delta), "Zig variable update");
      if (!project && frames && frame == frames / 2) {
        Check(library.empty() ? (builtin && module.Reload(builtin)) : module.Reload(library),
              "Zig reload");
        Check(scene.Sprites().size() == 1 && pool.Size() == 32, "reload duplicated content");
        reloaded = true;
        if (validation) {
          const auto before = content.ResolveAtlas(atlas->id);
          const auto reloadAtlas = twod_validation::Cook(*engine.Services().vfs, validationPackage,
                                                         "assets/atlas.nxcook", "sprite_atlas");
          Check(reloadAtlas.id == atlas->id && reloadAtlas.dependencies.empty(),
                "validation cooked reload identity");
          Check(contentApi.register_atlas(contentApi.context, reloadAtlas.payload.data(),
                                          reloadAtlas.payload.size(), &assetHigh, &assetLow) == 0,
                "validation cooked reload");
          const auto after = content.ResolveAtlas(atlas->id);
          Check(before && after && before->texture == after->texture &&
                    before->atlas == after->atlas,
                "identical cooked reload changed immutable generation");
          ++assetReloads;
        }
      }
      if (lifecycle.ExitRequested())
        break;
      if (native && !canPresent) {
        world.EndFrame();
        continue;
      }
      const auto width = displayState.pixel_width;
      const auto height = displayState.pixel_height;
      const auto &viewport = displayState.viewport_pixels;
      if (validation) {
        const auto buttons =
            twod_validation::Draw(ui, displayState.logical_width, displayState.logical_height);
        ++uiFrames;
        if (buttons.save) {
          const auto value = world.WorldTransform(entity);
          Check(bool(value), "validation UI save entity");
          twod_validation::Save(services.storage, value->x, value->y);
          ++uiSaves;
        }
        if (buttons.open) {
          if (const auto saved = twod_validation::Load(services.storage)) {
            restorePosition(*saved);
            ++uiOpens;
          }
        }
      }
      const auto camera = bridge.RenderCamera(uint32_t(viewport.width), uint32_t(viewport.height));
      // One extraction preserves transparent sorting across entities, transient instances and
      // tiles.
      std::vector<twod::SpriteInstance> instances;
      auto poses = world.SceneWorldPoses(sceneId);
      Check(bool(poses), "World poses");
      std::unordered_map<const twod::Sprite *, const NexoraTwoDEffectsSpriteV1 *> effectStyles;
      if (effectsState->enabled) {
        effectStyles.reserve(effectsState->sprites.size());
        for (const auto &style : effectsState->sprites) {
          const twod::Sprite *sprite{};
          if (style.kind == NEXORA_TWOD_EFFECTS_ENTITY) {
            if (world.FindEntity(style.handle))
              sprite = scene.Find(style.handle);
          } else if (const auto handle = effects.ResolvePoolIdentity(style.handle)) {
            if (const auto *instance =
                    pool.Find({handle->owner, handle->index, handle->generation}))
              sprite = &instance->sprite;
          }
          if (sprite)
            effectStyles.emplace(sprite, &style);
        }
      }
      const auto lightingStyle = [&](const twod::Sprite *sprite) {
        const auto found = effectStyles.find(sprite);
        if (found == effectStyles.end())
          return Nexora::Presentation::TwoDLightingStyle{};
        const auto &s = *found->second;
        return Nexora::Presentation::TwoDLightingStyle{s.groups, s.receive_light, s.receive_shadow,
                                                       0};
      };
      const auto emissionStyle =
          [&](const twod::Sprite *sprite) -> std::optional<twod::EmissionSource> {
        const auto found = effectStyles.find(sprite);
        if (found == effectStyles.end())
          return {};
        const auto &s = *found->second;
        if (!s.emission_atlas_high && !s.emission_atlas_low)
          return {};
        return twod::EmissionSource{{s.emission_atlas_high, s.emission_atlas_low},
                                    s.emission_region,
                                    {s.emission_color[0], s.emission_color[1], s.emission_color[2]},
                                    s.emission_intensity};
      };
      uint64_t identity{};
      for (const auto &pose : *poses)
        if (const auto *v = scene.Find(pose.id))
          instances.push_back(
              {++identity, *v, pose.matrix, {}, lightingStyle(v), emissionStyle(v)});
      pool.ForEach([&](auto &v) {
        instances.push_back({++identity,
                             v.sprite,
                             runtime::ToMatrix(v.transform),
                             {},
                             lightingStyle(&v.sprite),
                             emissionStyle(&v.sprite)});
      });
      if (hasTiles) {
        auto tileInputs = tiles.Collect(camera);
        Check(bool(tileInputs), "Tilemap collection");
        for (auto &value : tileInputs->instances) {
          value.id = ++identity;
          instances.push_back(value);
        }
      }
      auto extracted = twod::ExtractSprites(instances, camera, resolver);
      Check(bool(extracted), "2D extraction");
      Check(!project || extracted->statistics.unavailable == 0,
            "project sprite asset is unavailable");
      visible = extracted->statistics.visible;
      if (!project)
        Check(visible > 0 && extracted->statistics.unavailable == 0, "Zig Sprite asset resolution");
      extracted->OffsetViewport(uint32_t(viewport.x), uint32_t(viewport.y));
      auto overlay = ui.Frame();
      overlay.ToPixels(displayState.dpi_scale, width, height);
      if (project && !overlay.commands.empty())
        ++uiFrames;
      if (native) {
        const auto background =
            viewport.width != width || viewport.height != height ? 0xFF000000 : 0xFF29140A;
        auto drawStatus = Nexora::Presentation::SurfaceStatus::Unsupported;
        if (effectsState->enabled) {
          const auto lighting = twod::PrepareLighting(*effectsState, camera, uint32_t(viewport.x),
                                                      uint32_t(viewport.y));
          Check(bool(lighting), "frozen lighting preparation");
          drawStatus =
              extracted->DrawLinear(*surface.surface, lighting->uniforms, overlay.DrawData(),
                                    {uint32_t(viewport.x), uint32_t(viewport.y),
                                     uint32_t(viewport.width), uint32_t(viewport.height)},
                                    background, lighting->edges, effectsState->revision);
        } else {
          drawStatus = extracted->Draw(*surface.surface, overlay.DrawData(), background);
        }
        Check(drawStatus == Nexora::Presentation::SurfaceStatus::Ready, "native Sprite draw");
        const auto presented = surface.surface->EndFrame();
        // A window request during module update can invalidate the acquired image. The next
        // BeginFrame consumes its new extent; never count an unpresented frame as delivered.
        Check(presented == Nexora::Presentation::SurfaceStatus::Ready ||
                  presented == Nexora::Presentation::SurfaceStatus::OutOfDate ||
                  presented == Nexora::Presentation::SurfaceStatus::ZeroExtent ||
                  presented == Nexora::Presentation::SurfaceStatus::Occluded,
              "native present");
        if (presented == Nexora::Presentation::SurfaceStatus::Ready)
          ++rendered;
      }
      if (project && wantsAudio && !audioOutput && now >= nextAudioAttempt) {
        std::string reason;
        ++audioOpenAttempts;
        audioOutput = audio.OpenDefaultOutput(reason);
        if (audioOutput)
          ++audioOpenSuccesses;
        nextAudioAttempt = now + std::chrono::seconds(1);
        audioFrames = 0;
      }
      if (audioOutput) {
        audioFrames += delta * 48000.0;
        while (audioFrames >= 1) {
          const auto count = static_cast<uint32_t>(std::min(audioFrames, 4096.0));
          if (!audio.Pump(count)) {
            ++audioPumpFailures;
            Check(project.has_value(), "audio output");
            audio.CloseOutput();
            audioOutput = false;
            audioFrames = 0;
            nextAudioAttempt = now + std::chrono::seconds(1);
            break;
          }
          audioFrames -= count;
          submittedAudioFrames += count;
        }
      }
      world.EndFrame();
      if (!project && interactive && held(Nexora::Window::Key::Escape))
        break;
      if (interactive && frames)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    auto transform = world.WorldTransform(entity);
    if (project && !transform)
      transform = runtime::Transform{};
    Check(bool(transform), "Zig entity survives reload");
    if (requireMovement)
      Check(movementFrames > 0 && transform->x > 0, "native keyboard did not move Zig entity");
    if (!interactive && !project)
      Check(std::abs(transform->x - initialX - double(frames) / 60) < 1e-8,
            "Zig Transform control");
    (void)lifecycle.Stop();
    module.Unload();
    NexoraAssetUsageV1 assetUsage{};
    assetUsage.struct_size = sizeof(assetUsage);
    Check(services.assets.get_usage(services.assets.context, &assetUsage) == 0, "asset usage");
    assets.Shutdown();
    Check(context.allocated == context.freed, "Zig state allocation leak");
    NexoraAudioUsageV1 audioUsage{};
    audioUsage.struct_size = sizeof(audioUsage);
    Check(services.audio_state.get_usage(services.audio_state.context, &audioUsage) == 0,
          "audio residency");
    Check(services.audio.stop_all(services.audio.context) == 0, "audio voice shutdown");
    audio.CloseOutput();
    bool saveReopened{};
    if (validation) {
      twod_validation::Save(services.storage, transform->x, transform->y);
      // Stop every module callback before invalidating its borrowed storage table.
      services.storage = {};
      storage.reset();
      runtime::GameplayStorage reopened(saveDirectory, "nexora.zig.twod");
      const auto saved = twod_validation::Load(twod_validation::Storage(reopened));
      Check(saved && (*saved)[0] == transform->x && (*saved)[1] == transform->y,
            "validation save/reopen after owner destruction");
      saveReopened = true;
    }
    const auto finalWidth = display.State().pixel_width;
    const auto finalHeight = display.State().pixel_height;
    const auto gpu =
        native ? surface.surface->Diagnostics() : Nexora::Presentation::SurfaceDiagnostics{};
    if (native && !project)
      Check(gpu.spriteInstancedDrawCalls >= rendered && gpu.spriteGpuInstances >= rendered * 32,
            "Zig batches must reach native GPU instancing");
    if (native)
      Check(surface.surface->DrainAndDestroy() == Nexora::Presentation::SurfaceStatus::Ready,
            "surface shutdown");
    engine.Shutdown();
    const auto effectMilliseconds = [&](unsigned pass) {
      return (gpu.twoDEffects.timingMask & (1U << pass))
                 ? std::to_string(gpu.twoDEffects.milliseconds[pass])
                 : std::string("null");
    };
    const std::string evidence =
        "{\"status\":\"PASS\",\"module_abi\":3,\"project_mode\":" +
        std::string(project ? "true" : "false") +
        ",\"api\":1,\"native_frames\":" + std::to_string(rendered) +
        ",\"movement_frames\":" + std::to_string(movementFrames) +
        ",\"entity_x\":" + std::to_string(transform->x) +
        ",\"final_width\":" + std::to_string(finalWidth) +
        ",\"final_height\":" + std::to_string(finalHeight) +
        ",\"viewport_x\":" + std::to_string(display.State().viewport_pixels.x) +
        ",\"viewport_y\":" + std::to_string(display.State().viewport_pixels.y) +
        ",\"viewport_width\":" + std::to_string(display.State().viewport_pixels.width) +
        ",\"viewport_height\":" + std::to_string(display.State().viewport_pixels.height) +
        ",\"native_ui_draws\":" + std::to_string(gpu.nativeUiDrawCalls) +
        ",\"effects_completed_frame\":" + std::to_string(gpu.twoDEffects.completedFrame) +
        ",\"effects_revision\":" + std::to_string(gpu.twoDEffects.effectsRevision) +
        ",\"effects_timing_mask\":" + std::to_string(gpu.twoDEffects.timingMask) +
        ",\"effects_timing_source\":" + std::to_string(gpu.twoDEffects.timingSource) +
        ",\"effects_timing_status\":" + std::to_string(gpu.twoDEffects.timingStatus) +
        ",\"effects_shadow_ms\":" + effectMilliseconds(0) +
        ",\"effects_world_ms\":" + effectMilliseconds(1) +
        ",\"effects_bloom_ms\":" + effectMilliseconds(2) +
        ",\"effects_composite_ms\":" + effectMilliseconds(3) +
        ",\"effects_scene_bytes\":" + std::to_string(gpu.twoDLinearTargetBytes) +
        ",\"effects_shadow_bytes\":" + std::to_string(gpu.twoDShadowBufferBytes) +
        ",\"effects_bloom_bytes\":" + std::to_string(gpu.twoDBloomTargetBytes) +
        ",\"effects_upload_capacity_bytes\":" + std::to_string(gpu.twoDUploadCapacityBytes) +
        ",\"effects_peak_bytes\":" + std::to_string(gpu.twoDEffectsPeakBytes) +
        ",\"effects_peak_textures\":" + std::to_string(gpu.twoDPeakTextureCount) +
        ",\"native_ui_texture_uploads\":" + std::to_string(gpu.nativeUiTextureUploads) +
        ",\"resize_generations\":" + std::to_string(gpu.resizeGenerations) +
        ",\"surface_backend_requested\":\"" +
        std::string(native ? Nexora::Presentation::ToString(backend) : "none") + "\"" +
        ",\"render_device_ids_available\":" + (gpu.device.deviceIdsAvailable ? "true" : "false") +
        ",\"render_vendor_id\":" + std::to_string(gpu.device.vendorId) +
        ",\"render_device_id\":" + std::to_string(gpu.device.deviceId) +
        ",\"initial_dpi_scale\":" + std::to_string(initialDpi) +
        ",\"minimum_dpi_scale\":" + std::to_string(minimumDpi) +
        ",\"maximum_dpi_scale\":" + std::to_string(maximumDpi) +
        ",\"focus_lost_events\":" + std::to_string(focusLost) +
        ",\"focus_gained_events\":" + std::to_string(focusGained) +
        ",\"key_press_events\":" + std::to_string(keyPresses) +
        ",\"mouse_press_events\":" + std::to_string(mousePresses) +
        ",\"wheel_events\":" + std::to_string(wheelEvents) +
        ",\"unfocused_movement_frames\":" + std::to_string(unfocusedMovementFrames) +
        ",\"sprite_instanced_draws\":" + std::to_string(gpu.spriteInstancedDrawCalls) +
        ",\"sprite_gpu_instances\":" + std::to_string(gpu.spriteGpuInstances) +
        ",\"sprite_instance_upload_bytes\":" + std::to_string(gpu.spriteInstanceUploadBytes) +
        ",\"asset_active_bytes\":" + std::to_string(assetUsage.active_bytes) +
        ",\"asset_retired_bytes\":" + std::to_string(assetUsage.retired_bytes) +
        ",\"asset_staging_bytes\":" + std::to_string(assetUsage.staging_bytes) +
        ",\"asset_peak_content_bytes\":" + std::to_string(assetUsage.peak_content_bytes) +
        ",\"audio_active_bytes\":" + std::to_string(audioUsage.active_bytes) +
        ",\"audio_retired_bytes\":" + std::to_string(audioUsage.retired_bytes) +
        ",\"audio_peak_live_bytes\":" + std::to_string(audioUsage.peak_live_bytes) +
        ",\"pool_capacity\":" + std::to_string(project ? project->pool_instances : 1024) +
        ",\"visible\":" + std::to_string(visible) +
        ",\"reloaded\":" + (reloaded ? "true" : "false") +
        ",\"validation_package\":" + (validation ? "true" : "false") +
        ",\"resumed\":" + (resumed ? "true" : "false") +
        ",\"initial_x\":" + std::to_string(initialX) +
        ",\"ui_frames\":" + std::to_string(uiFrames) + ",\"ui_saves\":" + std::to_string(uiSaves) +
        ",\"ui_opens\":" + std::to_string(uiOpens) +
        ",\"cooked_reloads\":" + std::to_string(assetReloads) +
        ",\"audio_submitted_frames\":" + std::to_string(submittedAudioFrames) +
        ",\"audio_open_attempts\":" + std::to_string(audioOpenAttempts) +
        ",\"audio_open_successes\":" + std::to_string(audioOpenSuccesses) +
        ",\"audio_pump_failures\":" + std::to_string(audioPumpFailures) +
        ",\"save_reopened\":" + (saveReopened ? "true" : "false") +
        ",\"audible_acceptance\":\"NOT VERIFIED\"" +
        ",\"allocations_balanced\":true,\"physical_windows\":\"NOT RUN\"}\n";
    if (!report.empty()) {
      std::ofstream file(nexora::player::GamePath(report));
      file << evidence;
      Check(bool(file), "report output");
    }
    std::cout << evidence;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
