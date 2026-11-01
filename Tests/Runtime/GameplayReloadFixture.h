#pragma once
#include "Nexora/Game/GameplayHostBridge.h"
#include "Nexora/Runtime/GameplayModuleHost.h"
#include "Nexora/Runtime/Runtime.h"
#include <atomic>
#include <bit>
#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <new>
#include <unordered_map>

namespace nexora::test::reload {
using Host = runtime::GameplayModuleHost;
struct Owner;
struct Context final {
  Owner *owner;
  runtime::World *world;
  bool candidate{};
};
struct Owner final {
  runtime::World world{runtime::WorldKind::Play};
  runtime::World staging{runtime::WorldKind::Play}, retired{runtime::WorldKind::Play};
  std::unique_ptr<Context> live{std::make_unique<Context>(Context{this, &world})}, candidate;
  runtime::Id scene{}, entity{};
  unsigned allocations{}, frees{}, rollbacks{}, commits{}, stops{}, finalizers{};
  struct Block final {
    std::uint64_t bytes, alignment, tag;
  };
  std::unordered_map<void *, Block> blocks;
  bool fail_begin{}, fail_create{}, fail_restore{}, fail_start{}, oversized{};
  void *active{};
  std::atomic<bool> job_finished{true}, captured_before_job{};
  std::mutex mutex;
  std::condition_variable changed;
  bool barrier_entered{}, release_job{};

  explicit Owner(bool seed = true) {
    scene = world.LoadScene("Reload", true);
    if (seed) {
      entity = world.CreateEntity(scene).id;
      assert(world.Activate(scene));
    }
  }

  static Context &Get(void *pointer) { return *static_cast<Context *>(pointer); }

  template <typename Function>
  static std::int32_t Call(void *pointer, Function &&function) noexcept {
    if (!pointer)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    try {
      return function(Get(pointer));
    } catch (...) {
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    }
  }

  NexoraGameplayHostV3 Api() {
    NexoraGameplayHostV3 host{};
    host.struct_size = sizeof(host);
    host.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
    host.context = live.get();
    host.capabilities =
        NEXORA_GAMEPLAY_CAPABILITY_HOST_ALLOCATOR | NEXORA_GAMEPLAY_CAPABILITY_SCENE_API;
    host.log = [](void *pointer, std::uint32_t, const char *, std::uint32_t) {
      auto &context = Get(pointer);
      auto &owner = *context.owner;
      assert(context.candidate && owner.candidate.get() == &context &&
             context.world == &owner.staging);
      ++owner.finalizers;
    };
    host.allocate = [](void *pointer, std::uint64_t tag, std::uint64_t size,
                       std::uint64_t alignment) -> void * {
      if (!pointer || tag != NEXORA_ALLOCATION_OWNER_GAMEPLAY_STATE || !size ||
          size > 16 * 1024 * 1024 || alignment < alignof(void *) || alignment > 4096 ||
          !std::has_single_bit(alignment))
        return nullptr;
      auto &owner = *Get(pointer).owner;
      auto *memory =
          ::operator new(static_cast<std::size_t>(size),
                         std::align_val_t{static_cast<std::size_t>(alignment)}, std::nothrow);
      if (!memory)
        return nullptr;
      try {
        owner.blocks.emplace(memory, Block{size, alignment, tag});
        ++owner.allocations;
        return memory;
      } catch (...) {
        ::operator delete(memory, std::align_val_t{static_cast<std::size_t>(alignment)});
        return nullptr;
      }
    };
    host.deallocate = [](void *pointer, std::uint64_t tag, void *memory, std::uint64_t size,
                         std::uint64_t alignment) {
      auto &owner = *Get(pointer).owner;
      const auto found = owner.blocks.find(memory);
      assert(found != owner.blocks.end() && found->second.bytes == size &&
             found->second.alignment == alignment && found->second.tag == tag);
      ::operator delete(memory, std::align_val_t{static_cast<std::size_t>(alignment)});
      owner.blocks.erase(found);
      ++owner.frees;
    };
    host.read_component = [](void *pointer, std::uint64_t entity, std::uint64_t type, void *output,
                             std::uint32_t size) {
      return Call(pointer, [&](Context &context) {
        return game::ReadGameplayComponent(*context.world, entity, type, output, size);
      });
    };
    host.write_component = [](void *pointer, std::uint64_t entity, std::uint64_t type,
                              const void *input, std::uint32_t size) {
      return Call(pointer, [&](Context &context) {
        return game::WriteGameplayComponent(*context.world, entity, type, input, size);
      });
    };
    host.load_scene = [](void *pointer, const char *name, std::uint32_t size,
                         std::uint32_t persistent, std::uint64_t *output) {
      return Call(pointer, [&](Context &context) -> std::int32_t {
        if (!name || !size || size > 256 || persistent > 1 || !output)
          return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
        const std::string_view text{name, size};
        if (text.find('\0') != std::string_view::npos)
          return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
        *output = text == "Reload" ? context.owner->scene
                                   : context.world->LoadScene(std::string(text), persistent != 0);
        return NEXORA_GAMEPLAY_OK;
      });
    };
    host.activate_scene = [](void *pointer, std::uint64_t scene) {
      return Call(pointer, [&](Context &context) -> std::int32_t {
        return context.world->Activate(scene) ? NEXORA_GAMEPLAY_OK
                                              : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      });
    };
    host.spawn_entity = [](void *pointer, std::uint64_t scene,
                           const NexoraEntitySpawnDescriptor *descriptor, std::uint64_t *output) {
      return Call(pointer, [&](Context &context) -> std::int32_t {
        if (!descriptor || descriptor->struct_size < sizeof(*descriptor) || !output ||
            descriptor->components || descriptor->reserved)
          return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
        const runtime::Transform transform{descriptor->position.x, descriptor->position.y,
                                           descriptor->position.z};
        if (!runtime::IsValidTransform(transform))
          return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
        auto &entity = context.world->CreateEntity(scene);
        entity.transform = transform;
        *output = entity.id;
        if (!context.candidate && context.owner->entity == 0)
          context.owner->entity = entity.id;
        return NEXORA_GAMEPLAY_OK;
      });
    };
    host.despawn_entity = [](void *pointer, std::uint64_t entity) {
      return Call(pointer, [&](Context &context) -> std::int32_t {
        runtime::WorldCommandBuffer commands;
        commands.DestroyEntity(entity);
        return commands.Apply(*context.world) ? NEXORA_GAMEPLAY_OK
                                              : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      });
    };
    return host;
  }

  Host::ReloadTransaction Transaction() {
    return {this,
            [](void *pointer, NexoraGameplayHostV3 &host) noexcept {
              auto &owner = *static_cast<Owner *>(pointer);
              try {
                owner.staging = owner.world;
                owner.candidate = std::make_unique<Context>(Context{&owner, &owner.staging, true});
                host.context = owner.candidate.get();
                return !owner.fail_begin;
              } catch (...) {
                return false;
              }
            },
            [](void *pointer) noexcept {
              auto &owner = *static_cast<Owner *>(pointer);
              owner.candidate.reset();
              owner.staging = runtime::World{runtime::WorldKind::Play};
              ++owner.rollbacks;
            },
            [](void *pointer) noexcept {
              auto &owner = *static_cast<Owner *>(pointer);
              owner.retired = std::move(owner.world);
              owner.live->world = &owner.retired;
            },
            [](void *pointer) noexcept {
              auto &owner = *static_cast<Owner *>(pointer);
              owner.world = std::move(owner.staging);
              owner.live = std::move(owner.candidate);
              owner.live->world = &owner.world;
              owner.live->candidate = false;
              owner.retired = runtime::World{runtime::WorldKind::Play};
              ++owner.commits;
            }};
  }

  Host::QuiescenceBarrier Barrier() {
    return {this, [](void *pointer, std::uint64_t) {
              auto &owner = *static_cast<Owner *>(pointer);
              std::scoped_lock lock(owner.mutex);
              owner.barrier_entered = true;
              owner.changed.notify_all();
            }};
  }
};

} // namespace nexora::test::reload
