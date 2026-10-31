#include "Nexora/TwoD/GameplayEffects.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <unordered_set>

namespace nexora::twod {
namespace {
std::atomic<std::uint64_t> nextPoolIdentity{1};
std::uint64_t PoolToken() {
  auto next = nextPoolIdentity.load(std::memory_order_relaxed);
  while (next != UINT64_MAX) {
    if (nextPoolIdentity.compare_exchange_weak(next, next + 1, std::memory_order_relaxed))
      return next;
  }
  return 0;
}
bool Positive(float v) { return std::isfinite(v) && v >= 0 && v <= 1e6F; }
bool Position(NexoraTwoDEffectsPointV1 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) <= 1e6F && std::abs(p.y) <= 1e6F;
}
bool Color(const float (&v)[3]) { return Positive(v[0]) && Positive(v[1]) && Positive(v[2]); }
double Cross(NexoraTwoDEffectsPointV1 a, NexoraTwoDEffectsPointV1 b, NexoraTwoDEffectsPointV1 c) {
  return (double(b.x) - a.x) * (double(c.y) - a.y) - (double(b.y) - a.y) * (double(c.x) - a.x);
}
bool OnSegment(NexoraTwoDEffectsPointV1 a, NexoraTwoDEffectsPointV1 b, NexoraTwoDEffectsPointV1 p) {
  return Cross(a, b, p) == 0 && p.x >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x) &&
         p.y >= std::min(a.y, b.y) && p.y <= std::max(a.y, b.y);
}
bool Intersects(NexoraTwoDEffectsPointV1 a, NexoraTwoDEffectsPointV1 b, NexoraTwoDEffectsPointV1 c,
                NexoraTwoDEffectsPointV1 d) {
  const auto ab_c = Cross(a, b, c), ab_d = Cross(a, b, d);
  const auto cd_a = Cross(c, d, a), cd_b = Cross(c, d, b);
  return (((ab_c > 0 && ab_d < 0) || (ab_c < 0 && ab_d > 0)) &&
          ((cd_a > 0 && cd_b < 0) || (cd_a < 0 && cd_b > 0))) ||
         OnSegment(a, b, c) || OnSegment(a, b, d) || OnSegment(c, d, a) || OnSegment(c, d, b);
}
bool Polygon(const NexoraTwoDEffectsOccluderV1 &v) {
  const auto n = v.vertex_count;
  if (!v.id || n < 3 || n > NEXORA_TWOD_EFFECTS_MAX_VERTICES)
    return false;
  double area = 0;
  for (uint32_t i = 0; i < n; ++i) {
    const auto a = v.vertices[i], b = v.vertices[(i + 1) % n];
    if (!Position(a) || (a.x == b.x && a.y == b.y))
      return false;
    area += double(a.x) * b.y - double(a.y) * b.x;
    // Adjacent edges may share only their common endpoint, never overlap/backtrack.
    const auto c = v.vertices[(i + 2) % n];
    if (Cross(a, b, c) == 0 && (OnSegment(a, b, c) || OnSegment(b, c, a)))
      return false;
    for (uint32_t j = i + 1; j < n; ++j) {
      if (j == i + 1 || (i == 0 && j == n - 1))
        continue;
      if (Intersects(a, b, v.vertices[j], v.vertices[(j + 1) % n]))
        return false;
    }
  }
  return std::isfinite(area) && area != 0;
}
template <class T> bool Version(const T *v) {
  return v && v->struct_size >= sizeof(T) && v->abi_version == 1;
}
} // namespace
struct GameplayEffects::State final {
  const std::thread::id owner{std::this_thread::get_id()};
  uint32_t features{};
  SpriteResolver sprite;
  MaskResolver mask;
  PoolResolver pool;
  using PoolKey = std::tuple<uint64_t, uint32_t, uint32_t>;
  std::map<PoolKey, uint64_t> poolTokens;
  std::map<uint64_t, EffectsPoolIdentity> poolHandles;
  void PrunePoolIdentities() {
    if (!pool)
      return;
    for (auto it = poolHandles.begin(); it != poolHandles.end();) {
      const auto &h = it->second;
      if (!pool(h)) {
        poolTokens.erase(PoolKey{h.owner, h.index, h.generation});
        it = poolHandles.erase(it);
      } else
        ++it;
    }
  }
  int32_t PoolIdentity(uint64_t ownerId, uint32_t index, uint32_t generation, uint64_t *output) {
    if (!output || !ownerId || !generation)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    if (!pool)
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    const EffectsPoolIdentity handle{ownerId, index, generation};
    if (!pool(handle))
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    const PoolKey key{ownerId, index, generation};
    if (const auto it = poolTokens.find(key); it != poolTokens.end()) {
      *output = it->second;
      return NEXORA_GAMEPLAY_OK;
    }
    if (poolHandles.size() >= NEXORA_TWOD_EFFECTS_MAX_SPRITES)
      PrunePoolIdentities();
    if (poolHandles.size() >= NEXORA_TWOD_EFFECTS_MAX_SPRITES)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    const auto token = PoolToken();
    if (!token)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    const auto inserted = poolTokens.emplace(key, token);
    try {
      poolHandles.emplace(token, handle);
    } catch (...) {
      poolTokens.erase(inserted.first);
      throw;
    }
    *output = token;
    return NEXORA_GAMEPLAY_OK;
  }
  std::shared_ptr<const EffectsSnapshot> active = std::make_shared<EffectsSnapshot>();
  std::shared_ptr<const EffectsSnapshot> pending;
  uint64_t accepted{};

  int32_t Submit(const NexoraTwoDEffectsFrameV1 *v) {
    if (!v)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    if (!Version(v))
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    if (v->enabled > 1 || v->light_count > NEXORA_TWOD_EFFECTS_MAX_LIGHTS ||
        v->occluder_count > NEXORA_TWOD_EFFECTS_MAX_OCCLUDERS ||
        v->sprite_count > NEXORA_TWOD_EFFECTS_MAX_SPRITES || (v->light_count && !v->lights) ||
        (v->occluder_count && !v->occluders) || (v->sprite_count && !v->sprites) ||
        !Color(v->ambient) || !std::isfinite(v->strength) || v->strength < 0 || v->strength > 1 ||
        !Positive(v->bloom.intensity) || !std::isfinite(v->bloom.radius) || v->bloom.radius < 0 ||
        v->bloom.radius > 64 || v->bloom.levels > 5 || v->bloom.reserved ||
        (v->bloom.intensity > 0 && !v->bloom.levels))
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    if (v->enabled && (features & NEXORA_TWOD_EFFECTS_LIGHTING) == 0)
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    if (v->enabled && v->bloom.intensity > 0 && !(features & NEXORA_TWOD_EFFECTS_BLOOM))
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    uint32_t shadows = 0, edges = 0;
    std::unordered_set<uint64_t> ids;
    for (uint32_t i = 0; i < v->light_count; ++i) {
      const auto &l = v->lights[i];
      if (!l.id || !ids.insert(l.id).second || !Position(l.position) || !Color(l.color) ||
          !Positive(l.intensity) || !Positive(l.radius) || l.radius == 0 ||
          !std::isfinite(l.softness) || l.softness < 0 || l.softness > 1 || !Positive(l.bias) ||
          l.bias > l.radius || l.shadow > 1)
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      shadows += l.shadow;
    }
    if (shadows > NEXORA_TWOD_EFFECTS_MAX_SHADOW_LIGHTS)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    if (v->enabled && shadows && !(features & NEXORA_TWOD_EFFECTS_SHADOWS))
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    ids.clear();
    for (uint32_t i = 0; i < v->occluder_count; ++i) {
      const auto &o = v->occluders[i];
      if (!ids.insert(o.id).second || !Polygon(o))
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      edges += o.vertex_count;
      if (edges > NEXORA_TWOD_EFFECTS_MAX_EDGES)
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    }
    std::unordered_set<uint64_t> entities, instances;
    for (uint32_t i = 0; i < v->sprite_count; ++i) {
      const auto &s = v->sprites[i];
      if (!s.handle || s.kind > NEXORA_TWOD_EFFECTS_POOL || s.receive_light > 1 ||
          s.receive_shadow > 1 || s.reserved || !Color(s.emission_color) ||
          !Positive(s.emission_intensity) ||
          !(s.kind == NEXORA_TWOD_EFFECTS_ENTITY ? entities : instances).insert(s.handle).second)
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      if (s.kind == NEXORA_TWOD_EFFECTS_POOL && pool) {
        const auto found = poolHandles.find(s.handle);
        if (found == poolHandles.end() || !pool(found->second))
          return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      } else {
        if (!sprite)
          return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
        if (!sprite(s.kind, s.handle))
          return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      }
      const bool atlas = s.emission_atlas_high || s.emission_atlas_low;
      if (!atlas && (s.emission_region || s.emission_intensity > 0))
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      if (atlas) {
        if (!mask)
          return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
        if (!mask(s.emission_atlas_high, s.emission_atlas_low, s.emission_region))
          return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      }
    }
    if (accepted == std::numeric_limits<uint64_t>::max())
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    auto next = std::make_shared<EffectsSnapshot>();
    next->enabled = v->enabled != 0;
    std::copy(std::begin(v->ambient), std::end(v->ambient), next->ambient);
    next->strength = v->strength;
    next->bloom = v->bloom;
    if (v->light_count)
      next->lights.assign(v->lights, v->lights + v->light_count);
    if (v->occluder_count)
      next->occluders.assign(v->occluders, v->occluders + v->occluder_count);
    if (v->sprite_count)
      next->sprites.assign(v->sprites, v->sprites + v->sprite_count);
    next->revision = accepted + 1;
    pending = std::move(next);
    ++accepted;
    return NEXORA_GAMEPLAY_OK;
  }
  template <class F> static int32_t Call(void *p, F &&f) noexcept {
    if (!p)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    auto &s = *static_cast<State *>(p);
    if (s.owner != std::this_thread::get_id())
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    try {
      return f(s);
    } catch (...) {
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    }
  }
};
GameplayEffects::GameplayEffects(uint32_t features, SpriteResolver sprite, MaskResolver mask,
                                 PoolResolver pool)
    : state_(std::make_unique<State>()) {
  if ((features & ~7u) || ((features & 6u) && !(features & 1u)))
    throw std::invalid_argument("invalid 2D effects feature combination");
  state_->features = features;
  state_->sprite = std::move(sprite);
  state_->mask = std::move(mask);
  state_->pool = std::move(pool);
}
GameplayEffects::~GameplayEffects() = default;
std::shared_ptr<const EffectsSnapshot> GameplayEffects::BeginFrame() {
  if (state_->owner != std::this_thread::get_id())
    throw std::logic_error("2D effects requires owner thread");
  state_->PrunePoolIdentities();
  if (state_->pending)
    state_->active = std::move(state_->pending);
  return state_->active;
}
std::optional<EffectsPoolIdentity> GameplayEffects::ResolvePoolIdentity(uint64_t token) const {
  if (state_->owner != std::this_thread::get_id())
    throw std::logic_error("2D effects requires owner thread");
  const auto found = state_->poolHandles.find(token);
  if (found == state_->poolHandles.end() || !state_->pool || !state_->pool(found->second))
    return std::nullopt;
  return found->second;
}
NexoraGameplayTwoDEffectsApiV1 GameplayEffects::Api() noexcept {
  return {sizeof(NexoraGameplayTwoDEffectsApiV1),
          1,
          state_.get(),
          [](void *p, NexoraTwoDEffectsCapabilitiesV1 *v) {
            return State::Call(p, [&](auto &s) -> int32_t {
              if (!v)
                return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
              if (!Version(v))
                return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
              *v = {sizeof(*v), 1, s.features, 32, 8, 256, 16, 2048, 100000, 1024};
              return NEXORA_GAMEPLAY_OK;
            });
          },
          [](void *p, NexoraTwoDEffectsStateV1 *v) {
            return State::Call(p, [&](auto &s) -> int32_t {
              if (!v)
                return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
              if (!Version(v))
                return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
              const auto &a = *s.active;
              *v = {sizeof(*v),
                    1,
                    s.accepted,
                    a.revision,
                    uint32_t(a.enabled),
                    uint32_t(a.lights.size()),
                    uint32_t(a.occluders.size()),
                    uint32_t(a.sprites.size())};
              return NEXORA_GAMEPLAY_OK;
            });
          },
          [](void *p, const NexoraTwoDEffectsFrameV1 *v) {
            return State::Call(p, [&](auto &s) { return s.Submit(v); });
          },
          [](void *p, uint64_t ownerId, uint32_t index, uint32_t generation, uint64_t *output) {
            return State::Call(
                p, [&](auto &s) { return s.PoolIdentity(ownerId, index, generation, output); });
          }};
}
} // namespace nexora::twod
