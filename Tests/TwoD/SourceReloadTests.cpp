#include "Nexora/Game/GameplayHostBridge.h"
#include "Nexora/Runtime/GameplayModuleHost.h"
#include "Nexora/TwoD/GameplayContent.h"
#include "Nexora/TwoD/GameplayGeneration.h"
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <new>
#include <thread>
#include <unordered_map>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
using namespace nexora;
using Host = runtime::GameplayModuleHost;
using Path = std::filesystem::path;
using JobCall = void (*)();
constexpr twod::AssetId atlasId{8, 9}, firstId{11, 12}, secondId{13, 14}, clipId{15, 16};
struct Owner;
struct Generation final {
  Owner *owner;
  std::unique_ptr<twod::GameplayGeneration> values;
};
struct Owner final {
  // This registry outlives every resolver and retired Clip owner.
  twod::GameplayContent content;
  std::unique_ptr<Generation> active, candidate, retired;
  struct Block final {
    std::uint64_t tag, size, alignment;
  };
  std::unordered_map<void *, Block> blocks;
  unsigned allocations{}, frees{}, commits{}, rollbacks{};
  std::mutex mutex;
  std::condition_variable changed;
  bool barrierEntered{}, releaseJob{};
  std::atomic<bool> jobFinished{true};

  Owner() {
    active = std::make_unique<Generation>(
        Generation{this, twod::GameplayGeneration::CreateSingleScene(
                             "Main", 8, [this](auto id) { return content.ResolveClip(id); })});
    assert(active->values);
  }
  static Generation &Get(void *p) {
    assert(p);
    return *static_cast<Generation *>(p);
  }
  template <typename Function> static std::int32_t Call(void *p, Function &&function) noexcept {
    if (!p)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    try {
      return function(*Get(p).values);
    } catch (...) {
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    }
  }
  NexoraGameplayHostV3 Api() {
    NexoraGameplayHostV3 host{};
    host.struct_size = sizeof(host);
    host.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
    host.context = active.get();
    host.capabilities = NEXORA_GAMEPLAY_CAPABILITY_HOST_ALLOCATOR |
                        NEXORA_GAMEPLAY_CAPABILITY_SCENE_API | NEXORA_GAMEPLAY_CAPABILITY_2D_API;
    host.allocate = [](void *p, std::uint64_t tag, std::uint64_t size,
                       std::uint64_t alignment) -> void * {
      if (!p || tag != NEXORA_ALLOCATION_OWNER_GAMEPLAY_STATE || !size || size > 16 * 1024 * 1024 ||
          alignment < alignof(void *) || alignment > 4096 || !std::has_single_bit(alignment))
        return nullptr;
      auto &owner = *Get(p).owner;
      auto *memory =
          ::operator new(static_cast<std::size_t>(size),
                         std::align_val_t{static_cast<std::size_t>(alignment)}, std::nothrow);
      if (!memory)
        return nullptr;
      try {
        owner.blocks.emplace(memory, Block{tag, size, alignment});
        ++owner.allocations;
        return memory;
      } catch (...) {
        ::operator delete(memory, std::align_val_t{static_cast<std::size_t>(alignment)});
        return nullptr;
      }
    };
    host.deallocate = [](void *p, std::uint64_t tag, void *memory, std::uint64_t size,
                         std::uint64_t alignment) {
      auto &owner = *Get(p).owner;
      const auto found = owner.blocks.find(memory);
      assert(found != owner.blocks.end() && found->second.tag == tag &&
             found->second.size == size && found->second.alignment == alignment);
      ::operator delete(memory, std::align_val_t{static_cast<std::size_t>(alignment)});
      owner.blocks.erase(found);
      ++owner.frees;
    };
    host.get_twod_api = [](void *p, NexoraTwoDApiV1 *api) {
      return Call(p, [&](auto &v) { return v.Bridge().GetApi(api); });
    };
    host.read_component = [](void *p, std::uint64_t entity, std::uint64_t type, void *data,
                             std::uint32_t size) {
      return Call(p, [&](auto &v) {
        return game::ReadGameplayComponent(v.World(), entity, type, data, size);
      });
    };
    host.write_component = [](void *p, std::uint64_t entity, std::uint64_t type, const void *data,
                              std::uint32_t size) {
      return Call(p, [&](auto &v) {
        return game::WriteGameplayComponent(v.World(), entity, type, data, size);
      });
    };
    host.load_scene = [](void *p, const char *name, std::uint32_t size, std::uint32_t persistent,
                         std::uint64_t *id) {
      return Call(p, [&](auto &v) -> std::int32_t {
        if (!name || !id || size != 4 || std::string_view(name, size) != "Main" || persistent)
          return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
        *id = v.Sprites()->SceneId();
        return NEXORA_GAMEPLAY_OK;
      });
    };
    host.activate_scene = [](void *p, std::uint64_t id) {
      return Call(p, [&](auto &v) -> std::int32_t {
        return id == v.Sprites()->SceneId() && v.World().Activate(id) ? 0 : -1;
      });
    };
    host.spawn_entity = [](void *p, std::uint64_t scene, const NexoraEntitySpawnDescriptor *d,
                           std::uint64_t *id) {
      return Call(p, [&](auto &v) -> std::int32_t {
        if (!d || !id || d->struct_size < sizeof(*d) || d->components || d->reserved ||
            scene != v.Sprites()->SceneId())
          return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
        const runtime::Transform transform{d->position.x, d->position.y, d->position.z};
        if (!runtime::IsValidTransform(transform))
          return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
        auto &entity = v.World().CreateEntity(scene);
        entity.transform = transform;
        *id = entity.id;
        return NEXORA_GAMEPLAY_OK;
      });
    };
    host.despawn_entity = [](void *p, std::uint64_t id) {
      return Call(p, [&](auto &v) -> std::int32_t {
        runtime::WorldCommandBuffer commands;
        commands.DestroyEntity(id);
        if (!commands.Apply(v.World()))
          return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
        for (const auto removed : commands.LastDestroyed())
          v.Bridge().ForgetEntity(removed);
        return NEXORA_GAMEPLAY_OK;
      });
    };
    return host;
  }
  Host::ReloadTransaction Transaction() {
    return {this,
            [](void *p, NexoraGameplayHostV3 &host) noexcept {
              auto &o = *static_cast<Owner *>(p);
              assert(o.jobFinished);
              try {
                auto staged = o.active->values->Stage();
                if (!staged)
                  return false;
                o.candidate = std::make_unique<Generation>(Generation{&o, std::move(staged)});
                host.context = o.candidate.get();
                host.capabilities |= NEXORA_GAMEPLAY_CAPABILITY_RELOAD_CANDIDATE;
                return true;
              } catch (...) {
                return false;
              }
            },
            [](void *p) noexcept {
              auto &o = *static_cast<Owner *>(p);
              o.candidate.reset();
              ++o.rollbacks;
            },
            [](void *p) noexcept {
              auto &o = *static_cast<Owner *>(p);
              o.retired = std::move(o.active);
            },
            [](void *p) noexcept {
              auto &o = *static_cast<Owner *>(p);
              o.active = std::move(o.candidate);
              o.retired.reset();
              ++o.commits;
            }};
  }
  Host::QuiescenceBarrier Barrier() {
    return {this, [](void *p, std::uint64_t) {
              auto &o = *static_cast<Owner *>(p);
              std::scoped_lock lock(o.mutex);
              o.barrierEntered = true;
              o.changed.notify_all();
            }};
  }
};

NexoraTwoDApiV1 Api(twod::GameplayGeneration &v) {
  NexoraTwoDApiV1 api{};
  api.struct_size = sizeof(api);
  assert(v.Bridge().GetApi(&api) == 0);
  return api;
}
void Register(Owner &o, bool repack = false) {
  NexoraGameplayContentApiV1 api{};
  api.struct_size = sizeof(api);
  NexoraGameplaySpriteContentApiV1 sources{};
  sources.struct_size = sizeof(sources);
  assert(o.content.GetApi(&api) == 0 && o.content.GetSpriteApi(&sources) == 0);
  twod::Image image{repack ? 73U : 901U, 2, 2, twod::Bytes(16, std::byte{255})};
  const auto atlas =
      twod::PackAtlas(repack ? twod::AssetId{18, 19} : atlasId, std::span(&image, 1), 8, 8, true);
  assert(atlas);
  auto bytes = twod::EncodeAtlas(*atlas);
  std::uint64_t high{}, low{};
  assert(api.register_atlas(api.context, bytes.data(), bytes.size(), &high, &low) == 0);
  twod::SpriteAsset source;
  source.atlas = atlas->id;
  source.region = image.id;
  source.sourceWidth = source.sourceHeight = 6;
  source.trimX = source.trimY = 1;
  source.trimWidth = source.trimHeight = 2;
  source.pivotX = repack ? 3.0F : 2.0F;
  source.pivotY = source.pixelsPerUnit = 1;
  for (const auto id : {firstId, secondId}) {
    if (repack && id == secondId)
      continue;
    source.id = id;
    bytes = twod::EncodeSpriteAsset(source);
    assert(sources.register_sprite_source(sources.context, bytes.data(), bytes.size(), &high,
                                          &low) == 0);
  }
  if (!repack) {
    bytes = twod::EncodeClip(
        {clipId, {}, true, {{0, .25, "second", firstId}, {0, .5, "first", secondId}}});
    assert(api.register_clip(api.context, bytes.data(), bytes.size(), &high, &low) == 0);
  }
}
std::uint32_t Hits(twod::GameplayGeneration &v) {
  auto api = Api(v);
  NexoraTwoDShape shape{};
  shape.geometry.offset = {-2, 2};
  shape.geometry.radius = 1;
  shape.geometry.group = 1;
  shape.geometry.mask = UINT32_MAX;
  std::uint32_t count{};
  assert(api.overlap(api.context, 0, &shape, nullptr, 0, &count) == 0);
  return count;
}
struct Snapshot final {
  std::string world;
  twod::Bytes sprites, tiles;
  NexoraTwoDCamera camera{};
  std::uint32_t nextEvents{};
  explicit Snapshot(twod::GameplayGeneration &v, runtime::Id entity) {
    const auto w = v.World().SaveScene(v.Sprites()->SceneId());
    const auto s = v.Sprites()->Snapshot();
    assert(w && s);
    world = *w;
    sprites = *s;
    tiles = v.Tiles()->Encode();
    auto api = Api(v);
    assert(api.get_camera(api.context, &camera) == 0);
    NexoraTwoDAnimationResult result{};
    assert(api.advance_animation(api.context, entity, .125, nullptr, 0, &result) == 0);
    nextEvents = result.event_count;
  }
  void Verify(twod::GameplayGeneration &v, runtime::Id entity) const {
    const Snapshot actual(v, entity);
    assert(world == actual.world && sprites == actual.sprites && tiles == actual.tiles &&
           camera.center.x == actual.camera.center.x && camera.center.y == actual.camera.center.y &&
           camera.vertical_size == actual.camera.vertical_size &&
           camera.rotation_radians == actual.camera.rotation_radians &&
           nextEvents == actual.nextEvents);
  }
};
void Verify(Owner &o, runtime::Id entity, runtime::Id counter, twod::InstanceHandle sentinel,
            twod::InstanceHandle handle, std::uint64_t frames, std::uint64_t ticks,
            std::uint64_t jobs) {
  auto &v = *o.active->values;
  const auto *position = v.World().FindEntity(counter);
  assert(position && position->transform.x == static_cast<double>(frames) &&
         position->transform.y == static_cast<double>(jobs));
  const auto *anchor = v.World().FindEntity(entity);
  const auto *sprite = v.Sprites()->Find(entity);
  const auto *instance = v.Pool().Find(handle);
  assert(anchor && anchor->transform.x == -2 && anchor->transform.y == 2 && sprite && instance &&
         sprite->source == (ticks % 6 < 2 ? firstId : secondId) && sprite->order == 7 &&
         sprite->flipX == bool(frames % 4 & 1) && sprite->flipY == bool(frames % 4 & 2) &&
         sprite->sortY && sprite->visible && instance->sprite.source == sprite->source &&
         instance->transform.x == 3 && instance->transform.y == 2 && instance->transform.sx == -1 &&
         v.Pool().Find(sentinel) && v.Pool().Size() == 2 && Hits(v) == 1);
  const auto camera = v.Bridge().RenderCamera(640, 480);
  assert(camera.verticalSize == 10);
  const auto frame = v.Sprites()->Extract(
      camera, [&](auto id) { return o.content.ResolveAtlas(id); },
      [&](auto id) { return o.content.ResolveSprite(id); });
  assert(frame && frame->statistics.visible == 1 && frame->statistics.unavailable == 0 &&
         frame->vertices.size() == 4 && frame->indices.size() == 6);
  assert(o.blocks.size() == 1 && o.allocations - o.frees == 1);
}
JobCall BorrowJob(const Path &library) {
#if defined(_WIN32)
  const auto handle = LoadLibraryW(library.c_str());
  assert(handle);
  auto *symbol = reinterpret_cast<void *>(GetProcAddress(handle, "NexoraSourceReloadFixtureJob"));
#else
  auto *handle = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
  assert(handle);
  auto *symbol = dlsym(handle, "NexoraSourceReloadFixtureJob");
#endif
  assert(symbol);
  JobCall result{};
  static_assert(sizeof(result) == sizeof(symbol));
  std::memcpy(&result, &symbol, sizeof(result));
#if defined(_WIN32)
  assert(FreeLibrary(handle));
#else
  assert(dlclose(handle) == 0);
#endif
  return result;
}
bool Loaded(const Path &path) {
#if defined(_WIN32)
  return GetModuleHandleW(path.c_str()) != nullptr;
#else
  auto *handle = dlopen(path.c_str(), RTLD_NOW | RTLD_NOLOAD);
  if (!handle)
    return false;
  assert(dlclose(handle) == 0);
  return true;
#endif
}
Path Copy(const Path &source, const Path &directory, const std::string &name) {
  const auto path = directory / (name + source.extension().string());
  assert(std::filesystem::copy_file(source, path));
  return path;
}
} // namespace

int main(int argc, char **argv) {
  assert(argc == 6);
  const auto parent = std::filesystem::canonical(std::filesystem::current_path());
  const auto directory =
      parent / ("source-reload-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  assert(std::filesystem::create_directory(directory));
  std::cerr << "Retained on failure: " << directory << '\n';
  Owner owner;
  Register(owner);
  twod::Sprite sentinelSprite;
  sentinelSprite.source = firstId;
  const twod::Instance sentinelInstance{{7, 3, 0}, sentinelSprite};
  const auto sentinels = owner.active->values->Pool().Create(std::span(&sentinelInstance, 1));
  assert(sentinels && sentinels->size() == 1);
  const auto sentinel = sentinels->front();
  Host host(owner.Api(), owner.Barrier(), owner.Transaction());
  auto current = Copy(Path{argv[1]}, directory, "generation-1");
  assert(Host::Inspect(current) && host.Load(current));
  auto &initial = *owner.active->values;
  const auto entity = initial.Sprites()->Sprites().begin()->first;
  runtime::Id counter{};
  for (const auto &value : initial.World().FindScene(initial.Sprites()->SceneId())->entities)
    if (value.id != entity)
      counter = value.id;
  assert(counter && initial.Sprites()->SetCollider(entity, {}));
  auto api = Api(initial);
  assert(api.rebuild_queries(api.context, 0) == 0);
  // Find the fixture's only other live initial handle without assuming free-list order.
  twod::InstanceHandle handle{};
  for (std::uint32_t index = 0; index < 8; ++index) {
    const twod::InstanceHandle probe{sentinel.owner, index, 1};
    if (probe != sentinel && initial.Pool().Find(probe))
      handle = probe;
  }
  assert(handle.owner && initial.Pool().Find(handle));
  std::uint64_t frames{}, ticks{}, jobs{};
  auto update = [&] {
    assert(host.Update(.125));
    ++frames;
    if (frames % 9)
      ++ticks;
    Verify(owner, entity, counter, sentinel, handle, frames, ticks, jobs);
  };
  update();
  std::array<Path, 4> failures;
  for (std::size_t i = 0; i < failures.size(); ++i)
    failures[i] = Copy(Path{argv[i + 2]}, directory, "rejected-" + std::to_string(i));
  const std::array reasons{Host::ReloadFailure::Initialization, Host::ReloadFailure::RestoreState,
                           Host::ReloadFailure::Initialization, Host::ReloadFailure::RestoreState};
  for (unsigned round = 0; round < 32; ++round) {
    const auto generation = host.Generation();
    for (std::size_t i = 0; i < failures.size(); ++i) {
      const Snapshot before(*owner.active->values, entity);
      const bool replaced = host.Reload(failures[i]);
      const auto reason = host.GetReloadFailure();
      if (replaced || reason != reasons[i])
        std::cerr << "Rejected candidate mismatch: round=" << round << " case=" << i
                  << " replaced=" << replaced << " reason=" << static_cast<unsigned>(reason)
                  << " expected=" << static_cast<unsigned>(reasons[i]) << '\n';
      assert(!replaced && reason == reasons[i]);
      assert(host.Generation() == generation && !Loaded(failures[i]));
      before.Verify(*owner.active->values, entity);
      Verify(owner, entity, counter, sentinel, handle, frames, ticks, jobs);
      update();
    }
    if (round == 16) {
      const Snapshot before(*owner.active->values, entity);
      Register(owner, true);
      before.Verify(*owner.active->values, entity);
      const auto source = owner.content.ResolveSprite(firstId);
      assert(source && source->atlas == (twod::AssetId{18, 19}) && source->region == 73 &&
             source->pivotX == 3);
      Verify(owner, entity, counter, sentinel, handle, frames, ticks, jobs);
    }
    const auto next = Copy(Path{argv[1]}, directory, "generation-" + std::to_string(round + 2));
    const Snapshot before(*owner.active->values, entity);
    if (round == 15) {
      auto lease = host.AcquireWork(generation);
      assert(lease);
      auto call = BorrowJob(current);
      {
        std::scoped_lock lock(owner.mutex);
        owner.barrierEntered = owner.releaseJob = false;
      }
      owner.jobFinished = false;
      std::thread job([&, ticket = std::move(lease)]() mutable {
        {
          std::unique_lock lock(owner.mutex);
          assert(owner.changed.wait_for(lock, std::chrono::seconds(10),
                                        [&] { return owner.releaseJob; }));
        }
        call();
        call = nullptr;
        owner.jobFinished = true;
        ticket = {};
      });
      std::thread release([&] {
        std::unique_lock lock(owner.mutex);
        assert(owner.changed.wait_for(lock, std::chrono::seconds(10),
                                      [&] { return owner.barrierEntered; }));
        assert(!host.AcquireWork(generation));
        owner.releaseJob = true;
        owner.changed.notify_all();
      });
      assert(host.Reload(next));
      job.join();
      release.join();
      assert(owner.jobFinished && call == nullptr);
      jobs += 100;
    } else {
      assert(host.Reload(next));
    }
    before.Verify(*owner.active->values, entity);
    assert(!Loaded(current) && host.Generation() == generation + 1 &&
           !host.AcquireWork(generation));
    current = next;
    update();
  }
  assert(host.Generation() == 33 && host.GetReloadStats().successful_reloads == 32 &&
         host.GetReloadStats().migrated_bytes == 80 && owner.commits == 32 &&
         owner.rollbacks == 128);
  host.Unload();
  assert(owner.blocks.empty() && owner.allocations == owner.frees && !Loaded(current) &&
         owner.active->values->World().ActiveEntityCount() == 0 &&
         owner.active->values->Pool().Size() == 1 && owner.active->values->Pool().Find(sentinel));
  const auto physical = std::filesystem::canonical(directory);
  assert(physical == directory && physical.parent_path() == parent);
  std::filesystem::remove_all(physical);
  std::cout
      << "PASS 32 real SDK source reloads, 128 rejected candidates, old-code lease and reimport\n";
}
