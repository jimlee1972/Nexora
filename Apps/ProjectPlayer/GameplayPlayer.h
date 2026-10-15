#pragma once

#include "Nexora/Game/GameplayHostBridge.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/Runtime/GameplayAudio.h"
#include "Nexora/Runtime/GameplayGamepad.h"
#include "Nexora/Runtime/GameplayInput.h"
#include "Nexora/Runtime/GameplayModuleHost.h"
#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/Runtime/GameplayStorage.h"
#include "Nexora/Runtime/ProjectPackage.h"

#include <iostream>
#include <limits>
#include <new>

namespace nexora::player {

// Owner-thread host. The module is destroyed before this context and its copied World.
// StaticView's immutable asset palette is not a dynamic mesh/2D renderer.
struct PlayerGameplay final {
  game::GameWorld world{runtime::WorldKind::Play};
  NexoraInputSnapshot input{};
  std::array<bool, 256> held_keys{};
  bool focused{};
  double pointer_x{}, pointer_y{};
  runtime::GameplayServices services;
  runtime::GameplayInput actions;
  runtime::NativeGameplayGamepad gamepad;
  runtime::GameplayAudio audio;
  std::unique_ptr<runtime::GameplayStorage> storage;
  runtime::Id scene{};
  std::uint64_t updates{}, fixed_updates{};

  bool Initialize(const runtime::LoadedStaticProject &project,
                  const std::filesystem::path &save_directory) {
    services.input = actions.Api();
    services.audio = audio.Api();
    std::string audio_error;
    if (!audio.OpenDefaultOutput(audio_error))
      std::cerr << "ProjectPlayer audio output unavailable: " << audio_error << '\n';
    if (!save_directory.empty()) {
      storage = std::make_unique<runtime::GameplayStorage>(save_directory,
                                                           project.ProjectId().ToString());
      services.storage.struct_size = sizeof(services.storage);
      services.storage.abi_version = 1;
      if (storage->GetApi(services.storage) != NEXORA_GAMEPLAY_OK)
        return false;
    }
    const auto snapshot = project.WorldView().SaveScene(project.SceneId());
    if (!snapshot)
      return false;
    const auto loaded = world.InternalWorld().LoadSceneSnapshot(*snapshot);
    if (!loaded || !world.ActivateScene(*loaded))
      return false;
    scene = *loaded;
    return true;
  }

  // Legacy V3 input retains its WASD/Space meaning; pointer coordinates belong to extensions.
  void CaptureEvents(std::span<const Nexora::Window::WindowEvent> events, bool native_focused,
                     std::uint64_t sequence) {
    focused = native_focused;
    if (!focused)
      held_keys.fill(false);
    (void)actions.BeginFrame();
    (void)actions.SetFocused(focused);
    for (const auto &event : events) {
      using Nexora::Window::WindowEventType;
      if (event.type == WindowEventType::FocusChanged) {
        focused = event.value0 != 0;
        if (!focused)
          held_keys.fill(false);
        (void)actions.SetFocused(focused);
      } else if (event.type == WindowEventType::Key) {
        if (focused && event.value0 >= 0 &&
            static_cast<std::size_t>(event.value0) < held_keys.size()) {
          held_keys[static_cast<std::size_t>(event.value0)] = event.value1 != 0;
          (void)actions.Submit(0, NEXORA_INPUT_DEVICE_KEYBOARD, 0,
                               static_cast<std::uint32_t>(event.value0),
                               event.value1 ? 1.0F : 0.0F);
        }
      } else if (event.type == WindowEventType::Pointer) {
        pointer_x = event.value0;
        pointer_y = event.value1;
        (void)actions.SubmitPointer(0, pointer_x, pointer_y);
      } else if (event.type == WindowEventType::Wheel) {
        (void)actions.SubmitPointer(0, pointer_x, pointer_y, event.value0, event.value1);
      } else if (event.type == WindowEventType::PointerButton) {
        (void)actions.Submit(0, NEXORA_INPUT_DEVICE_MOUSE, 0,
                             static_cast<std::uint32_t>(event.value0), event.value1 ? 1.0F : 0.0F);
      }
    }
    (void)gamepad.Poll(actions);
    const auto held = [&](Nexora::Window::Key key) {
      return held_keys[static_cast<std::size_t>(key)] ? 1.0 : 0.0;
    };
    using Nexora::Window::Key;
    input = {sequence, held(Key::D) - held(Key::A), held(Key::W) - held(Key::S),
             held(Key::Space) != 0 ? 1U : 0U, 0};
  }

  void CaptureNativeInput(const Nexora::Presentation::RenderSurface &surface) {
    CaptureEvents(surface.Events(), surface.Input().focused, surface.Input().sequence);
  }

  NexoraGameplayHostV3 Host() {
    NexoraGameplayHostV3 host{};
    host.struct_size = sizeof(host);
    host.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
    host.capabilities =
        NEXORA_GAMEPLAY_CAPABILITY_HOST_ALLOCATOR | NEXORA_GAMEPLAY_CAPABILITY_SCENE_API;
    host.capabilities |= NEXORA_GAMEPLAY_CAPABILITY_SERVICES_API;
    host.context = this;
    host.get_services_api = [](void *context, NexoraGameplayServicesApiV1 *api) -> int32_t {
      return static_cast<PlayerGameplay *>(context)->services.GetApi(api);
    };
    host.log = [](void *, std::uint32_t, const char *message, std::uint32_t length) {
      if (message)
        std::cerr.write(message, length) << '\n';
    };
    host.read_component = [](void *context, std::uint64_t entity, std::uint64_t type, void *data,
                             std::uint32_t size) -> int32_t {
      try {
        return game::ReadGameplayComponent(static_cast<PlayerGameplay *>(context)->world, entity,
                                           type, data, size);
      } catch (...) {
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      }
    };
    host.write_component = [](void *context, std::uint64_t entity, std::uint64_t type,
                              const void *data, std::uint32_t size) -> int32_t {
      // Changing meshes/materials needs palette/resource admission beyond this static adapter.
      if (type == game::MeshRendererComponentType())
        return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
      try {
        return game::WriteGameplayComponent(static_cast<PlayerGameplay *>(context)->world, entity,
                                            type, data, size);
      } catch (...) {
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      }
    };
    host.allocate = [](void *, std::uint64_t, std::uint64_t size,
                       std::uint64_t alignment) -> void * {
      if (!size || size > std::numeric_limits<std::size_t>::max() || !alignment ||
          alignment > std::numeric_limits<std::size_t>::max() || (alignment & (alignment - 1)))
        return nullptr;
      try {
        return ::operator new(static_cast<std::size_t>(size), std::align_val_t(alignment));
      } catch (...) {
        return nullptr;
      }
    };
    host.deallocate = [](void *, std::uint64_t, void *allocation, std::uint64_t,
                         std::uint64_t alignment) {
      if (allocation && alignment && !(alignment & (alignment - 1)))
        ::operator delete(allocation, std::align_val_t(alignment));
    };
    host.load_scene = [](void *context, const char *name, std::uint32_t length,
                         std::uint32_t persistent, std::uint64_t *scene) -> int32_t {
      if (!name || !length || length > 4096 || !scene || persistent > 1)
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      try {
        *scene = static_cast<PlayerGameplay *>(context)->world.LoadScene(std::string(name, length),
                                                                         persistent != 0);
        return NEXORA_GAMEPLAY_OK;
      } catch (...) {
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      }
    };
    host.activate_scene = [](void *context, std::uint64_t scene) -> int32_t {
      return static_cast<PlayerGameplay *>(context)->world.ActivateScene(scene)
                 ? NEXORA_GAMEPLAY_OK
                 : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    };
    host.spawn_entity = [](void *context, std::uint64_t scene,
                           const NexoraEntitySpawnDescriptor *wire,
                           std::uint64_t *entity) -> int32_t {
      if (!wire || !entity || wire->struct_size < sizeof(*wire))
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      // Non-rendered logical entities are supported. New rendering/physics bindings are not.
      if (wire->components)
        return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
      try {
        game::EntitySpawnDescriptor descriptor;
        descriptor.transform = {wire->position.x, wire->position.y, wire->position.z};
        *entity = static_cast<PlayerGameplay *>(context)->world.SpawnEntity(scene, descriptor);
        return NEXORA_GAMEPLAY_OK;
      } catch (...) {
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      }
    };
    host.despawn_entity = [](void *context, std::uint64_t entity) -> int32_t {
      try {
        return static_cast<PlayerGameplay *>(context)->world.DestroyEntity(entity)
                   ? NEXORA_GAMEPLAY_OK
                   : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      } catch (...) {
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      }
    };
    host.capture_input = [](void *context, std::uint32_t user,
                            NexoraInputSnapshot *snapshot) -> int32_t {
      if (user || !snapshot)
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      *snapshot = static_cast<PlayerGameplay *>(context)->input;
      return NEXORA_GAMEPLAY_OK;
    };
    host.resolve_asset = [](void *, std::uint64_t, std::uint64_t, NexoraAssetHandle *) -> int32_t {
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    };
    host.raycast = [](void *, const NexoraRaycastRequest *, NexoraRaycastHit *) -> int32_t {
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    };
    host.debug_draw_line = [](void *, const NexoraDebugLine *) -> int32_t {
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    };
    host.get_diagnostics = [](void *context, NexoraFrameDiagnostics *diagnostics) -> int32_t {
      if (!diagnostics)
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      try {
        const auto &self = *static_cast<PlayerGameplay *>(context);
        *diagnostics = {self.updates, self.world.Query(self.scene).size(), 0, 0};
        return NEXORA_GAMEPLAY_OK;
      } catch (...) {
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      }
    };
    return host;
  }
};
} // namespace nexora::player
