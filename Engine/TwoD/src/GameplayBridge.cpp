#include "Nexora/TwoD/GameplayBridge.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <thread>

namespace nexora::twod {
namespace {
constexpr auto Ok = NEXORA_GAMEPLAY_OK;
constexpr auto Invalid = NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
constexpr auto Unsupported = NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
constexpr auto Lifecycle = NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
Collider Decode(const NexoraTwoDCollider &v) {
  return {static_cast<ShapeKind>(v.kind),
          {v.offset.x, v.offset.y},
          {v.half_extent.x, v.half_extent.y},
          v.radius,
          v.group,
          v.mask};
}
NexoraTwoDCollider Encode(const Collider &v) {
  return {static_cast<std::uint32_t>(v.kind),
          {v.offset.x, v.offset.y},
          {v.halfExtent.x, v.halfExtent.y},
          v.radius,
          v.group,
          v.mask};
}
Sprite Decode(const NexoraTwoDSprite &v) {
  return {{v.atlas_high, v.atlas_low},
          v.region,
          {v.size.x, v.size.y},
          {v.pivot.x, v.pivot.y},
          {v.color[0], v.color[1], v.color[2], v.color[3]},
          v.layer,
          v.order,
          (v.flags & 1U) != 0,
          (v.flags & 2U) != 0,
          (v.flags & 4U) != 0,
          (v.flags & 8U) != 0};
}
NexoraTwoDSprite Encode(const Sprite &v) {
  return {v.atlas.high,
          v.atlas.low,
          v.region,
          (v.flipX ? 1U : 0U) | (v.flipY ? 2U : 0U) | (v.sortY ? 4U : 0U) | (v.visible ? 8U : 0U),
          {v.size.x, v.size.y},
          {v.pivot.x, v.pivot.y},
          {v.color[0], v.color[1], v.color[2], v.color[3]},
          v.layer,
          v.order};
}
std::optional<Instance> Decode(const NexoraTwoDInstance &v) {
  const auto &t = v.transform;
  auto transform = runtime::NormalizedTransform({t.position.x, t.position.y, t.position.z,
                                                 t.rotation.x, t.rotation.y, t.rotation.z,
                                                 t.rotation.w, t.scale.x, t.scale.y, t.scale.z});
  auto sprite = Decode(v.sprite);
  if (!transform || v.sprite.flags > 15 || !ValidateSprite(sprite))
    return std::nullopt;
  return Instance{*transform, sprite};
}
InstanceHandle Decode(NexoraTwoDInstanceHandle v) { return {v.owner, v.index, v.generation}; }
NexoraTwoDInstance Encode(const Instance &v) {
  const auto &t = v.transform;
  return {{{t.x, t.y, t.z}, {t.qx, t.qy, t.qz, t.qw}, {t.sx, t.sy, t.sz}}, Encode(v.sprite)};
}
} // namespace

struct GameplayBridge::State {
  runtime::World &world;
  SpriteScene &scene;
  ClipResolver clips;
  Tilemap *tiles;
  InstancePool *pool;
  std::thread::id thread{std::this_thread::get_id()};
  bool entered{};
  NexoraTwoDCamera camera{{0, 0}, 20, 0};
  struct Playback {
    Animator animator;
    AssetId atlas;
  };
  std::map<runtime::Id, Playback> animations;
  std::array<SpatialIndex, 2> indices;
  std::array<bool, 2> ready{};

  bool Live(runtime::Id entity) const {
    const auto *s = world.FindScene(scene.SceneId());
    return s && s->state != runtime::SceneState::Unloading &&
           s->state != runtime::SceneState::Unloaded &&
           std::any_of(s->entities.begin(), s->entities.end(),
                       [entity](const auto &e) { return e.id == entity; });
  }
  template <class F> static int32_t Call(void *opaque, F &&f) noexcept {
    if (!opaque)
      return Invalid;
    auto &s = *static_cast<State *>(opaque);
    if (s.thread != std::this_thread::get_id() || s.entered)
      return Lifecycle;
    s.entered = true;
    struct Leave {
      bool &flag;
      ~Leave() { flag = false; }
    } leave{s.entered};
    try {
      return f(s);
    } catch (...) {
      return Lifecycle;
    }
  }
  int32_t Source(std::uint32_t source) const {
    if (source > 1)
      return Invalid;
    if (source == 1 && !tiles)
      return Unsupported;
    return Ok;
  }
  NexoraTwoDApiV1 Api() {
    NexoraTwoDApiV1 api{};
    api.struct_size = sizeof(api);
    api.abi_version = NEXORA_2D_API_VERSION;
    api.context = this;
    api.set_sprite = [](void *p, uint64_t id, const NexoraTwoDSprite *v) {
      return Call(p, [&](auto &s) -> int32_t {
        if (!s.Live(id))
          return Invalid;
        if (!v) {
          s.scene.Remove(id);
          s.animations.erase(id);
          return Ok;
        }
        if (v->flags > 15 || !s.scene.Set(id, Decode(*v)))
          return Invalid;
        auto it = s.animations.find(id);
        if (it != s.animations.end() && it->second.atlas != s.scene.Find(id)->atlas)
          s.animations.erase(it);
        return Ok;
      });
    };
    api.get_sprite = [](void *p, uint64_t id, NexoraTwoDSprite *v) {
      return Call(p, [&](auto &s) -> int32_t {
        if (!v || !s.Live(id))
          return Invalid;
        const auto *sprite = s.scene.Find(id);
        if (!sprite)
          return Invalid;
        *v = Encode(*sprite);
        return Ok;
      });
    };
    api.set_collider = [](void *p, uint64_t id, const NexoraTwoDCollider *v) {
      return Call(p, [&](auto &s) -> int32_t {
        if (!s.Live(id))
          return Invalid;
        if (v && (v->kind > 1 || !s.scene.SetCollider(id, Decode(*v))))
          return Invalid;
        if (!v)
          s.scene.RemoveCollider(id);
        s.ready[0] = false;
        return Ok;
      });
    };
    api.get_collider = [](void *p, uint64_t id, NexoraTwoDCollider *v) {
      return Call(p, [&](auto &s) -> int32_t {
        if (!v || !s.Live(id))
          return Invalid;
        const auto *c = s.scene.FindCollider(id);
        if (!c)
          return Invalid;
        *v = Encode(*c);
        return Ok;
      });
    };
    api.set_camera = [](void *p, const NexoraTwoDCamera *v) {
      return Call(p, [&](auto &s) -> int32_t {
        if (!v || !std::isfinite(v->center.x) || !std::isfinite(v->center.y) ||
            std::abs(v->center.x) > 1e6F || std::abs(v->center.y) > 1e6F ||
            !std::isfinite(v->vertical_size) || v->vertical_size < 0.00001F ||
            v->vertical_size > 1e6F || !std::isfinite(v->rotation_radians) ||
            std::abs(v->rotation_radians) > 1e6F)
          return Invalid;
        s.camera = *v;
        return Ok;
      });
    };
    api.get_camera = [](void *p, NexoraTwoDCamera *v) {
      return Call(p, [&](auto &s) -> int32_t {
        if (!v)
          return Invalid;
        *v = s.camera;
        return Ok;
      });
    };
    api.play_animation = [](void *p, uint64_t id, uint64_t hi, uint64_t lo, double speed) {
      return Call(p, [&](auto &s) -> int32_t {
        if (!s.Live(id) || !s.scene.Find(id) || (!hi && !lo) || !std::isfinite(speed) ||
            speed <= 0 || speed > 1000)
          return Invalid;
        if (!s.clips)
          return Unsupported;
        const auto clip = s.clips({hi, lo});
        if (!clip)
          return Unsupported;
        const auto *current = s.scene.Find(id);
        if (!s.Live(id) || !current)
          return Invalid;
        Playback playback;
        playback.atlas = clip->atlas;
        if (clip->id != AssetId{hi, lo} || clip->atlas != current->atlas ||
            !playback.animator.Play(*clip, speed))
          return Invalid;
        auto sprite = *current;
        sprite.region = *playback.animator.RegionId();
        s.animations.insert_or_assign(id, std::move(playback));
        s.scene.Set(id, sprite);
        return Ok;
      });
    };
    api.pause_animation = [](void *p, uint64_t id, uint32_t paused) {
      return Call(p, [&](auto &s) -> int32_t {
        auto it = s.animations.find(id);
        if (paused > 1 || !s.Live(id) || it == s.animations.end())
          return Invalid;
        it->second.animator.Pause(paused != 0);
        return Ok;
      });
    };
    api.advance_animation = [](void *p, uint64_t id, double delta, NexoraTwoDEvent *events,
                               uint32_t capacity, NexoraTwoDAnimationResult *result) {
      return Call(p, [&](auto &s) -> int32_t {
        auto it = s.animations.find(id);
        if (!result || (!events && capacity) || capacity > 65536 || !s.Live(id) ||
            !s.scene.Find(id) || it == s.animations.end())
          return Invalid;
        auto candidate = it->second.animator;
        auto update = candidate.Advance(delta);
        if (!update)
          return Invalid;
        const NexoraTwoDAnimationResult value{update->region, update->finished ? 1U : 0U,
                                              static_cast<uint32_t>(update->events.size()), 0};
        // Count-only probes and insufficient capacity never consume time/events.
        if (!events || capacity < update->events.size()) {
          *result = value;
          return events ? Unsupported : Ok;
        }
        auto sprite = *s.scene.Find(id);
        sprite.region = update->region;
        if (!s.scene.Set(id, sprite))
          return Invalid;
        it->second.animator = std::move(candidate);
        for (std::size_t i = 0; i < update->events.size(); ++i) {
          events[i] = {};
          events[i].length = static_cast<uint32_t>(update->events[i].size());
          std::memcpy(events[i].name, update->events[i].data(), events[i].length);
        }
        *result = value;
        return Ok;
      });
    };
    api.rebuild_queries = [](void *p, uint32_t source) {
      return Call(p, [&](auto &s) -> int32_t {
        const auto status = s.Source(source);
        if (status != Ok)
          return status;
        auto shapes = source ? s.tiles->CollisionShapes() : s.scene.CollisionShapes();
        if (!shapes || !s.indices[source].Build(*shapes))
          return Invalid;
        s.ready[source] = true;
        return Ok;
      });
    };
    api.overlap = [](void *p, uint32_t source, const NexoraTwoDShape *v, uint64_t *hits,
                     uint32_t capacity, uint32_t *count) {
      return Call(p, [&](auto &s) -> int32_t {
        const auto status = s.Source(source);
        if (status != Ok)
          return status;
        if (!v || !count || (!hits && capacity) || capacity > 100000 || v->geometry.kind > 1)
          return Invalid;
        const auto c = Decode(v->geometry);
        Shape shape{v->exclude_id, c.kind, c.offset, c.halfExtent, c.radius, c.group, c.mask};
        if (!ValidateShape(shape))
          return Invalid;
        if (!s.ready[source])
          return Lifecycle;
        auto result = s.indices[source].Query(shape);
        *count = static_cast<uint32_t>(result.size());
        if (!hits)
          return Ok;
        if (capacity < result.size())
          return Unsupported;
        std::copy(result.begin(), result.end(), hits);
        return Ok;
      });
    };
    api.sweep = [](void *p, uint32_t source, const NexoraTwoDSweep *v, NexoraTwoDHit *hits,
                   uint32_t capacity, uint32_t *count) {
      return Call(p, [&](auto &s) -> int32_t {
        const auto status = s.Source(source);
        if (status != Ok)
          return status;
        if (!v || !count || (!hits && capacity) || capacity > 100000)
          return Invalid;
        auto validPoint = [](NexoraVec2 point) {
          return std::isfinite(point.x) && std::isfinite(point.y) && std::abs(point.x) <= 1e6F &&
                 std::abs(point.y) <= 1e6F;
        };
        if (!validPoint(v->from) || !validPoint(v->to) || !std::isfinite(v->radius) ||
            v->radius < 0 || v->radius > 1e6F)
          return Invalid;
        if (!s.ready[source])
          return Lifecycle;
        auto result = s.indices[source].Sweep({v->from.x, v->from.y}, {v->to.x, v->to.y}, v->radius,
                                              v->group, v->mask);
        *count = static_cast<uint32_t>(result.size());
        if (!hits)
          return Ok;
        if (capacity < result.size())
          return Unsupported;
        for (std::size_t i = 0; i < result.size(); ++i)
          hits[i] = {result[i].id, result[i].fraction};
        return Ok;
      });
    };
    api.paint_tiles = [](void *p, const NexoraTwoDTile *v, uint32_t count) {
      return Call(p, [&](auto &s) -> int32_t {
        if (!s.tiles)
          return Unsupported;
        if ((!v && count) || count > 100000)
          return Invalid;
        std::vector<Tile> tiles;
        tiles.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
          if (v[i].collision > 1)
            return Invalid;
          tiles.push_back({v[i].x, v[i].y, v[i].layer, v[i].region, v[i].collision != 0});
        }
        if (!s.tiles->Paint(tiles))
          return Invalid;
        s.ready[1] = false;
        return Ok;
      });
    };
    api.set_chunk_loaded = [](void *p, int32_t x, int32_t y, uint32_t loaded) {
      return Call(p, [&](auto &s) -> int32_t {
        if (!s.tiles)
          return Unsupported;
        if (loaded > 1 || !s.tiles->SetChunkLoaded({x, y}, loaded != 0))
          return Invalid;
        s.ready[1] = false;
        return Ok;
      });
    };
    api.get_tile = [](void *p, int32_t x, int32_t y, int32_t layer, NexoraTwoDTile *v) {
      return Call(p, [&](auto &s) -> int32_t {
        if (!s.tiles)
          return Unsupported;
        if (!v)
          return Invalid;
        auto it = s.tiles->Tiles().find({layer, y, x});
        if (it == s.tiles->Tiles().end())
          return Invalid;
        const auto &t = it->second;
        *v = {t.x, t.y, t.layer, t.region, t.collision ? 1U : 0U};
        return Ok;
      });
    };
    api.create_instances = [](void *p, const NexoraTwoDInstance *v, uint32_t count,
                              NexoraTwoDInstanceHandle *handles) {
      return Call(p, [&](auto &s) -> int32_t {
        if (!s.pool)
          return Unsupported;
        if (count > 100000 || (count && (!v || !handles)))
          return Invalid;
        std::vector<Instance> instances;
        instances.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
          auto value = Decode(v[i]);
          if (!value)
            return Invalid;
          instances.push_back(*value);
        }
        auto created = s.pool->Create(instances);
        if (!created)
          return Invalid;
        for (std::size_t i = 0; i < created->size(); ++i) {
          const auto h = (*created)[i];
          handles[i] = {h.owner, h.index, h.generation};
        }
        return Ok;
      });
    };
    api.destroy_instances = [](void *p, const NexoraTwoDInstanceHandle *v, uint32_t count) {
      return Call(p, [&](auto &s) -> int32_t {
        if (!s.pool)
          return Unsupported;
        if (count > 100000 || (count && !v))
          return Invalid;
        std::vector<InstanceHandle> handles;
        handles.reserve(count);
        for (uint32_t i = 0; i < count; ++i)
          handles.push_back(Decode(v[i]));
        return s.pool->Destroy(handles) ? Ok : Invalid;
      });
    };
    api.read_instances = [](void *p, const NexoraTwoDInstanceHandle *v, uint32_t count,
                            NexoraTwoDInstance *values) {
      return Call(p, [&](auto &s) -> int32_t {
        if (!s.pool)
          return Unsupported;
        if (count > 100000 || (count && (!v || !values)))
          return Invalid;
        if (!count)
          return Ok;
        std::vector<NexoraTwoDInstance> copies;
        copies.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
          const auto *value = s.pool->Find(Decode(v[i]));
          if (!value)
            return Invalid;
          copies.push_back(Encode(*value));
        }
        std::copy(copies.begin(), copies.end(), values);
        return Ok;
      });
    };
    api.write_instances = [](void *p, const NexoraTwoDInstanceHandle *v, uint32_t count,
                             const NexoraTwoDInstance *values) {
      return Call(p, [&](auto &s) -> int32_t {
        if (!s.pool)
          return Unsupported;
        if (count > 100000 || (count && (!v || !values)))
          return Invalid;
        std::vector<std::pair<Instance *, Instance>> updates;
        updates.reserve(count);
        std::set<std::uint32_t> seen;
        for (uint32_t i = 0; i < count; ++i) {
          auto *target = s.pool->Find(Decode(v[i]));
          auto value = Decode(values[i]);
          if (!target || !value || !seen.insert(v[i].index).second)
            return Invalid;
          updates.emplace_back(target, *value);
        }
        for (const auto &[target, value] : updates)
          *target = value;
        return Ok;
      });
    };
    return api;
  }
};

GameplayBridge::GameplayBridge(runtime::World &world, SpriteScene &scene, ClipResolver clips,
                               Tilemap *tiles, InstancePool *pool)
    : state_(std::make_unique<State>(world, scene, std::move(clips), tiles, pool)) {
  if (&scene.WorldOwner() != &world)
    throw std::invalid_argument("2D gameplay scene must belong to the supplied World");
}
GameplayBridge::~GameplayBridge() = default;
int32_t GameplayBridge::GetApi(NexoraTwoDApiV1 *output) noexcept {
  return State::Call(state_.get(), [&](auto &s) -> int32_t {
    if (!output)
      return Invalid;
    if (output->struct_size < sizeof(NexoraTwoDApiV1))
      return Unsupported;
    *output = s.Api();
    return Ok;
  });
}
Camera GameplayBridge::RenderCamera(std::uint32_t width, std::uint32_t height) const {
  if (state_->thread != std::this_thread::get_id())
    throw std::logic_error("2D gameplay camera requires owner thread");
  const auto &c = state_->camera;
  return {{c.center.x, c.center.y}, c.vertical_size, c.rotation_radians, width, height};
}
void GameplayBridge::ForgetEntity(runtime::Id entity) {
  if (state_->thread != std::this_thread::get_id() || state_->entered)
    throw std::logic_error("2D gameplay teardown requires serialized owner thread");
  state_->scene.Remove(entity);
  state_->scene.RemoveCollider(entity);
  state_->animations.erase(entity);
  state_->ready[0] = false;
}
} // namespace nexora::twod
