#include "Nexora/Core/Engine.h"
#include "Nexora/Game/GameplayHostBridge.h"
#include "Nexora/Runtime/GameplayAudio.h"
#include "Nexora/Runtime/GameplayGamepad.h"
#include "Nexora/Runtime/GameplayInput.h"
#include "Nexora/Runtime/GameplayModuleHost.h"
#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/Runtime/GameplayStorage.h"
#include "Nexora/TwoD/GameplayBridge.h"
#include "Nexora/TwoD/GameplayContent.h"
#include "Nexora/TwoD/GameplayUi.h"
#include "TwoDValidation.h"
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

extern "C" int32_t NexoraGameModuleLoad(uint32_t, NexoraGameModuleV3 *);
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
      if (std::string_view(name, length) != "Zig 2D")
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

int main(int argc, char **argv) {
  try {
    bool native{}, interactive{}, requireMovement{}, audioOutput{};
    unsigned frames = 60;
    std::string report, library, saveDirectory, validationPackage;
    for (int i = 1; i < argc; ++i) {
      const std::string_view arg(argv[i]);
      if (arg == "--native")
        native = true;
      else if (arg == "--audio")
        audioOutput = true;
      else if (arg == "--interactive") {
        native = interactive = true;
        frames = 0;
      } else if (arg == "--require-movement")
        requireMovement = true;
      else if ((arg == "--frames" || arg == "--report" || arg == "--module" ||
                arg == "--save-directory" || arg == "--validation-package") &&
               i + 1 < argc) {
        const std::string_view value(argv[++i]);
        if (arg == "--validation-package")
          validationPackage = value;
        else if (arg == "--save-directory")
          saveDirectory = value;
        else if (arg == "--report")
          report = value;
        else if (arg == "--module")
          library = value;
        else {
          const auto parsed = std::from_chars(value.data(), value.data() + value.size(), frames);
          Check(parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() &&
                    frames <= 100000,
                "invalid frame count");
        }
      } else
        throw std::runtime_error("unknown or incomplete argument");
    }
    Check(frames || interactive, "zero frames requires interactive mode");
    core::Engine engine;
    core::EngineConfiguration configuration;
    configuration.worker_count = 2;
    const bool validation = !validationPackage.empty();
    if (validation) {
      Check(library.empty() && !saveDirectory.empty(),
            "validation package requires the static sample and an explicit save directory");
      validationPackage = std::filesystem::canonical(validationPackage).string();
      configuration.content_root = validationPackage;
    }
    engine.Initialize(configuration);
    runtime::World world(runtime::WorldKind::Play);
    const auto sceneId = world.LoadScene("Zig 2D");
    twod::SpriteScene scene(world, sceneId);
    twod::Tilemap tiles;
    tiles.atlas = {1, 2};
    twod::InstancePool pool(1024);
    std::array<twod::Image, 2> images{
        {{1, 8, 8, twod::Bytes(256, std::byte{255})}, {2, 8, 8, twod::Bytes(256, std::byte{255})}}};
    for (std::size_t i = 0; i < images[0].rgba.size(); i += 4) {
      images[0].rgba[i + 1] = std::byte{64};
      images[0].rgba[i + 2] = std::byte{32};
      images[1].rgba[i] = std::byte{32};
      images[1].rgba[i + 2] = std::byte{64};
    }
    auto packed = twod::PackAtlas({1, 2}, images, 32, 32, true);
    Check(bool(packed), "sample atlas");
    auto atlas = std::make_shared<const twod::Atlas>(std::move(*packed));
    auto clip = std::make_shared<const twod::Clip>(
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
    twod::GameplayContent content;
    NexoraGameplayContentApiV1 contentApi{};
    contentApi.struct_size = sizeof(contentApi);
    contentApi.abi_version = 1;
    Check(content.GetApi(&contentApi) == 0, "content service negotiation");
    const auto atlasBytes = twod::EncodeAtlas(*atlas);
    const auto clipBytes = twod::EncodeClip(*clip);
    uint64_t assetHigh{}, assetLow{};
    Check(contentApi.register_atlas(contentApi.context, atlasBytes.data(), atlasBytes.size(),
                                    &assetHigh, &assetLow) == 0,
          "sample cooked Atlas registration");
    Check(contentApi.register_clip(contentApi.context, clipBytes.data(), clipBytes.size(),
                                   &assetHigh, &assetLow) == 0,
          "sample cooked Clip registration");
    twod::GameplayBridge bridge(
        world, scene, [&content](auto id) { return content.ResolveClip(id); }, &tiles, &pool);
    runtime::GameplayInput input;
    runtime::NativeGameplayGamepad gamepads;
    runtime::GameplayAudio audio;
    twod::GameplayUi ui;
    runtime::GameplayServices services;
    services.content = contentApi;
    services.input = input.Api();
    services.audio = audio.Api();
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
    std::unique_ptr<runtime::GameplayStorage> storage;
    if (!saveDirectory.empty()) {
      storage = std::make_unique<runtime::GameplayStorage>(saveDirectory, "nexora.zig.twod");
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
    if (audioOutput) {
      std::string reason;
      const bool opened = audio.OpenDefaultOutput(reason);
      Check(opened, reason.c_str());
      if (validation) {
        const NexoraAudioPlayDesc desc{1, NEXORA_AUDIO_BUS_SFX, 1, 1};
        std::uint64_t voice{};
        Check(services.audio.play(services.audio.context, &desc, &voice) == 0,
              "validation device audio play");
      }
    }
    Context context{world, bridge, sceneId, services};
    runtime::GameplayModuleHost module(Host(context));
    Check(library.empty() ? module.Load(NexoraGameModuleLoad) : module.Load(library),
          "Zig module load");
    Check(scene.Sprites().size() == 1 && pool.Size() == 32, "Zig authored content");
    const auto entity = scene.Sprites().begin()->first;
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
    Nexora::Presentation::RenderSurfaceResult surface;
    if (native) {
      Nexora::Presentation::RenderSurfaceDescriptor d;
      d.title = "Nexora Zig 2D";
      d.width = 640;
      d.height = 480;
      surface = Nexora::Presentation::CreateRenderSurface(d);
      Check(bool(surface), "native 2D surface");
    }
    std::array<bool, static_cast<std::size_t>(Nexora::Window::Key::F12) + 1> keys{};
    unsigned rendered{}, movementFrames{}, uiFrames{}, assetReloads{};
    std::uint64_t submittedAudioFrames{};
    unsigned uiSaves{}, uiOpens{};
    std::size_t visible{};
    twod::FixedStepper step;
    auto lastTime = std::chrono::steady_clock::now();
    bool reloaded{}, focused{true};
    double audioFrames{};
    twod::GameplayUiInput uiInput;
    for (unsigned frame = 0; !frames || frame < frames; ++frame) {
      Check(input.BeginFrame(), "input frame");
      (void)gamepads.Poll(input);
      uiInput.pointer_pressed = uiInput.pointer_released = false;
      if (native) {
        const auto status = surface.surface->BeginFrame();
        if (surface.surface->CloseRequested())
          break;
        for (const auto &event : surface.surface->Events()) {
          if (event.type == Nexora::Window::WindowEventType::FocusChanged) {
            focused = event.value0 != 0;
            Check(input.SetFocused(focused), "input focus");
            uiInput.focused = focused;
            if (!focused) {
              keys.fill(false);
              uiInput.pointer_down = false;
              uiInput.pointer_pressed = uiInput.pointer_released = false;
            }
          }
          if (event.type == Nexora::Window::WindowEventType::Pointer) {
            uiInput.pointer_x = static_cast<float>(event.value0);
            uiInput.pointer_y = static_cast<float>(event.value1);
            Check(input.SubmitPointer(0, uiInput.pointer_x, uiInput.pointer_y), "pointer input");
          }
          if (event.type == Nexora::Window::WindowEventType::Wheel)
            Check(input.SubmitPointer(0, uiInput.pointer_x, uiInput.pointer_y, event.value0,
                                      event.value1),
                  "wheel input");
          if (event.type == Nexora::Window::WindowEventType::PointerButton && focused &&
              event.value0 >= 0 && event.value0 < 5) {
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
              keys[static_cast<std::size_t>(event.value0)] = event.value1 != 0;
              Check(input.Submit(0, NEXORA_INPUT_DEVICE_KEYBOARD, 0,
                                 static_cast<uint32_t>(event.value0), event.value1 ? 1.F : 0.F),
                    "key input");
            }
        }
        if (status == Nexora::Presentation::SurfaceStatus::ZeroExtent ||
            status == Nexora::Presentation::SurfaceStatus::Occluded ||
            status == Nexora::Presentation::SurfaceStatus::OutOfDate) {
          lastTime = std::chrono::steady_clock::now();
          std::this_thread::sleep_for(std::chrono::milliseconds(8));
          --frame;
          continue;
        }
        Check(status == Nexora::Presentation::SurfaceStatus::Ready, "surface acquire");
      }
      const auto actionState = [&](uint32_t action) {
        NexoraGameplayActionStateV1 state{};
        state.struct_size = sizeof(state);
        state.abi_version = 1;
        Check(services.input.read_action(services.input.context, 0, action, &state) == 0,
              "UI action");
        return state;
      };
      const auto pressed = [&](uint32_t action) { return actionState(action).pressed != 0; };
      uiInput.navigate = pressed(253) ? 1 : pressed(254) ? -1 : 0;
      uiInput.activate = pressed(255);
      uiInput.cancel = pressed(256);
      ui.BeginInput(uiInput);
      ++context.input.sequence;
      const auto held = [&](Nexora::Window::Key key) {
        return keys[static_cast<std::size_t>(key)] ? 1.0 : 0.0;
      };
      context.input.move_x = interactive ? actionState(250).value : 1.0;
      context.input.move_y = interactive ? actionState(251).value : 0.0;
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
      for (uint64_t i = 0; i < ticks->steps; ++i)
        Check(module.FixedUpdate(step.StepSeconds()), "Zig fixed update");
      Check(module.Update(delta), "Zig variable update");
      if (frames && frame == frames / 2) {
        Check(library.empty() ? module.Reload(NexoraGameModuleLoad) : module.Reload(library),
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
      const auto width = native ? surface.surface->FrameInfo().width : 640U;
      const auto height = native ? surface.surface->FrameInfo().height : 480U;
      if (validation) {
        const auto buttons =
            twod_validation::Draw(ui, static_cast<float>(width), static_cast<float>(height));
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
      const auto camera = bridge.RenderCamera(width, height);
      // One extraction preserves transparent sorting across entities, transient instances and
      // tiles.
      std::vector<twod::SpriteInstance> instances;
      auto poses = world.SceneWorldPoses(sceneId);
      Check(bool(poses), "World poses");
      uint64_t identity{};
      for (const auto &pose : *poses)
        if (const auto *v = scene.Find(pose.id))
          instances.push_back({++identity, *v, pose.matrix});
      pool.ForEach([&](auto &v) {
        instances.push_back({++identity, v.sprite, runtime::ToMatrix(v.transform)});
      });
      auto tileInputs = tiles.Collect(camera);
      Check(bool(tileInputs), "Tilemap collection");
      for (auto &value : tileInputs->instances) {
        value.id = ++identity;
        instances.push_back(value);
      }
      auto extracted = twod::ExtractSprites(instances, camera, resolver);
      Check(bool(extracted), "2D extraction");
      visible = extracted->statistics.visible;
      Check(visible > 0 && extracted->statistics.unavailable == 0, "Zig Sprite asset resolution");
      if (native) {
        const auto overlay = ui.Frame();
        Check(extracted->Draw(*surface.surface, overlay.DrawData()) ==
                  Nexora::Presentation::SurfaceStatus::Ready,
              "native Sprite draw");
        Check(surface.surface->EndFrame() == Nexora::Presentation::SurfaceStatus::Ready,
              "native present");
        ++rendered;
      }
      if (audioOutput) {
        audioFrames += delta * 48000.0;
        while (audioFrames >= 1) {
          const auto count = static_cast<uint32_t>(std::min(audioFrames, 4096.0));
          Check(audio.Pump(count), "audio output");
          audioFrames -= count;
          submittedAudioFrames += count;
        }
      }
      world.EndFrame();
      if (interactive && held(Nexora::Window::Key::Escape))
        break;
      if (interactive && frames)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    const auto transform = world.WorldTransform(entity);
    Check(bool(transform), "Zig entity survives reload");
    if (requireMovement)
      Check(movementFrames > 0 && transform->x > 0, "native keyboard did not move Zig entity");
    if (!interactive)
      Check(std::abs(transform->x - initialX - double(frames) / 60) < 1e-8,
            "Zig Transform control");
    module.Unload();
    Check(context.allocated == context.freed, "Zig state allocation leak");
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
    const auto finalWidth = native ? surface.surface->FrameInfo().width : 640U;
    const auto finalHeight = native ? surface.surface->FrameInfo().height : 480U;
    const auto gpu =
        native ? surface.surface->Diagnostics() : Nexora::Presentation::SurfaceDiagnostics{};
    if (native)
      Check(gpu.spriteInstancedDrawCalls >= rendered && gpu.spriteGpuInstances >= rendered * 32,
            "Zig batches must reach native GPU instancing");
    if (native)
      Check(surface.surface->DrainAndDestroy() == Nexora::Presentation::SurfaceStatus::Ready,
            "surface shutdown");
    engine.Shutdown();
    const std::string evidence =
        "{\"status\":\"PASS\",\"language\":\"Zig 0.14\",\"api\":1,\"native_frames\":" +
        std::to_string(rendered) + ",\"movement_frames\":" + std::to_string(movementFrames) +
        ",\"entity_x\":" + std::to_string(transform->x) +
        ",\"final_width\":" + std::to_string(finalWidth) +
        ",\"final_height\":" + std::to_string(finalHeight) +
        ",\"native_ui_draws\":" + std::to_string(gpu.nativeUiDrawCalls) +
        ",\"native_ui_texture_uploads\":" + std::to_string(gpu.nativeUiTextureUploads) +
        ",\"resize_generations\":" + std::to_string(gpu.resizeGenerations) +
        ",\"sprite_instanced_draws\":" + std::to_string(gpu.spriteInstancedDrawCalls) +
        ",\"sprite_gpu_instances\":" + std::to_string(gpu.spriteGpuInstances) +
        ",\"sprite_instance_upload_bytes\":" + std::to_string(gpu.spriteInstanceUploadBytes) +
        ",\"visible\":" + std::to_string(visible) +
        ",\"reloaded\":" + (reloaded ? "true" : "false") +
        ",\"validation_package\":" + (validation ? "true" : "false") +
        ",\"resumed\":" + (resumed ? "true" : "false") +
        ",\"initial_x\":" + std::to_string(initialX) +
        ",\"ui_frames\":" + std::to_string(uiFrames) + ",\"ui_saves\":" + std::to_string(uiSaves) +
        ",\"ui_opens\":" + std::to_string(uiOpens) +
        ",\"cooked_reloads\":" + std::to_string(assetReloads) +
        ",\"audio_submitted_frames\":" + std::to_string(submittedAudioFrames) +
        ",\"save_reopened\":" + (saveReopened ? "true" : "false") +
        ",\"audible_acceptance\":\"NOT VERIFIED\"" +
        ",\"allocations_balanced\":true,\"physical_windows\":\"NOT RUN\"}\n";
    if (!report.empty()) {
      std::ofstream file(report);
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
