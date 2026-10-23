#include "Nexora/TwoD/GameplayContent.h"
#include "nexora/nexora.h"
#include <algorithm>
#include <atomic>
#include <map>
#include <stdexcept>
#include <thread>

namespace nexora::twod {
namespace {
constexpr int Ok = NEXORA_GAMEPLAY_OK, Invalid = NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
              Missing = NEXORA_GAMEPLAY_ERROR_UNSUPPORTED,
              Lifecycle = NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
constexpr std::size_t MaximumBytes = 64 * 1024 * 1024;
using Key = std::pair<std::uint64_t, std::uint64_t>;
Key Identity(AssetId id) { return {id.high, id.low}; }
std::uint64_t TextureId() {
  // Dedicated process-wide immutable content texture namespace. UI hosts reserve outside it.
  static std::atomic<std::uint64_t> next{UINT64_C(1) << 62};
  auto value = next.load(std::memory_order_relaxed);
  while (value < (UINT64_C(1) << 63)) {
    if (next.compare_exchange_weak(value, value + 1, std::memory_order_relaxed))
      return value;
  }
  return 0;
}
bool Same(const Atlas &a, const Atlas &b) {
  return a.id == b.id && a.width == b.width && a.height == b.height && a.gutter == b.gutter &&
         a.nearest == b.nearest && a.regions == b.regions && a.rgba == b.rgba;
}
} // namespace
struct GameplayContent::State final {
  struct Entry final {
    std::shared_ptr<const Atlas> atlas;
    std::shared_ptr<const Clip> clip;
    std::uint64_t texture{};
    std::size_t bytes{};
  };
  struct Retired final {
    std::weak_ptr<const void> owner;
    std::uint64_t texture{}, bytes{}, pixels{};
  };
  Limits limits;
  std::vector<Retired> retired;
  explicit State(Limits value) : limits(value) {
    if (!limits.assets || limits.assets > 4096 || !limits.active_bytes ||
        limits.active_bytes > 1024ULL * 1024 * 1024 || limits.live_bytes < limits.active_bytes ||
        limits.live_bytes > 2ULL * 1024 * 1024 * 1024)
      throw std::invalid_argument("Content capacity");
  }
  Usage Statistics() const {
    Usage result{static_cast<std::uint32_t>(entries.size()),
                 static_cast<std::uint32_t>(retired.size()), bytes, 0, 0};
    for (const auto &[_, e] : entries)
      if (e.atlas)
        result.texture_bytes += e.atlas->rgba.size();
    for (const auto &r : retired) {
      result.retired_bytes += r.bytes;
      result.texture_bytes += r.pixels;
    }
    return result;
  }
  void PrepareRetirement(const Entry *entry) {
    if (entry)
      retired.reserve(retired.size() + 1);
  }
  void Retire(const Entry &entry) {
    if (entry.atlas)
      retired.push_back({entry.atlas, entry.texture, entry.bytes, entry.atlas->rgba.size()});
    else
      retired.push_back({entry.clip, 0, entry.bytes, 0});
  }
  std::thread::id thread{std::this_thread::get_id()};
  std::map<Key, Entry> entries;
  std::size_t bytes{};
  template <class Fn> static int Call(void *ctx, Fn &&fn) noexcept {
    if (!ctx)
      return Invalid;
    auto &s = *static_cast<State *>(ctx);
    if (s.thread != std::this_thread::get_id())
      return Lifecycle;
    try {
      return fn(s);
    } catch (...) {
      return Lifecycle;
    }
  }
  bool Budget(Key key, std::size_t size) const {
    const auto existing = entries.find(key);
    if (existing == entries.end() && entries.size() >= limits.assets)
      return false;
    const auto previous = existing == entries.end() ? 0 : existing->second.bytes;
    const auto usage = Statistics();
    return retired.size() < 65536 && size <= limits.active_bytes &&
           bytes - previous <= limits.active_bytes - size &&
           usage.active_bytes + usage.retired_bytes <= limits.live_bytes - size;
  }
  static bool Buffer(const void *data, std::uint64_t length, std::uint64_t *high,
                     std::uint64_t *low) {
    return data && length && length <= MaximumBytes && high && low && high != low;
  }
  static NexoraContentAsset Describe(const Key &id, const Entry &e) {
    if (e.atlas)
      return {id.first,
              id.second,
              NEXORA_CONTENT_ATLAS,
              static_cast<std::uint32_t>(e.atlas->regions.size()),
              e.atlas->width,
              e.atlas->height,
              id.first,
              id.second};
    return {
        id.first, id.second, NEXORA_CONTENT_CLIP, static_cast<std::uint32_t>(e.clip->frames.size()),
        0,        0,         e.clip->atlas.high,  e.clip->atlas.low};
  }
  static int AtlasBytes(void *ctx, const void *data, std::uint64_t length, std::uint64_t *high,
                        std::uint64_t *low) noexcept {
    return Call(ctx, [&](State &s) {
      if (!Buffer(data, length, high, low))
        return Invalid;
      auto atlas =
          DecodeAtlas({static_cast<const std::byte *>(data), static_cast<std::size_t>(length)});
      if (!atlas)
        return Invalid;
      const auto key = Identity(atlas->id);
      const auto old = s.entries.find(key);
      if (old != s.entries.end() && old->second.clip)
        return Invalid;
      for (const auto &[_, entry] : s.entries)
        if (entry.clip && entry.clip->atlas == atlas->id)
          for (const auto &frame : entry.clip->frames)
            if (!atlas->Find(frame.region))
              return Invalid;
      if (old != s.entries.end() && Same(*old->second.atlas, *atlas)) {
        *high = key.first;
        *low = key.second;
        return Ok;
      }
      const auto size = atlas->rgba.size() + atlas->regions.size() * sizeof(Region);
      if (!s.Budget(key, size))
        return Missing;
      s.PrepareRetirement(old == s.entries.end() ? nullptr : &old->second);
      auto owner = std::make_shared<const Atlas>(std::move(*atlas));
      const auto texture = TextureId();
      if (!texture)
        return Missing;
      const auto previous = old == s.entries.end() ? 0 : old->second.bytes;
      if (old != s.entries.end())
        s.Retire(old->second);
      s.entries.insert_or_assign(key, Entry{std::move(owner), {}, texture, size});
      s.bytes = s.bytes - previous + size;
      *high = key.first;
      *low = key.second;
      return Ok;
    });
  }
  static int ClipBytes(void *ctx, const void *data, std::uint64_t length, std::uint64_t *high,
                       std::uint64_t *low) noexcept {
    return Call(ctx, [&](State &s) {
      if (!Buffer(data, length, high, low))
        return Invalid;
      auto clip =
          DecodeClip({static_cast<const std::byte *>(data), static_cast<std::size_t>(length)});
      if (!clip)
        return Invalid;
      const auto key = Identity(clip->id);
      const auto old = s.entries.find(key);
      if (old != s.entries.end() && old->second.atlas)
        return Invalid;
      const auto dependency = s.entries.find(Identity(clip->atlas));
      if (dependency == s.entries.end() || !dependency->second.atlas)
        return Missing;
      for (const auto &frame : clip->frames)
        if (!dependency->second.atlas->Find(frame.region))
          return Invalid;
      std::size_t size = clip->frames.size() * sizeof(AnimationFrame);
      for (const auto &frame : clip->frames)
        size += frame.event.size();
      if (!s.Budget(key, size))
        return Missing;
      const auto previous = old == s.entries.end() ? 0 : old->second.bytes;
      s.PrepareRetirement(old == s.entries.end() ? nullptr : &old->second);
      auto owner = std::make_shared<const Clip>(std::move(*clip));
      if (old != s.entries.end())
        s.Retire(old->second);
      s.entries.insert_or_assign(key, Entry{{}, std::move(owner), 0, size});
      s.bytes = s.bytes - previous + size;
      *high = key.first;
      *low = key.second;
      return Ok;
    });
  }
  static int List(void *ctx, NexoraContentAsset *assets, std::uint32_t capacity,
                  std::uint32_t *count) noexcept {
    return Call(ctx, [&](State &s) {
      if (!count || (!assets && capacity))
        return Invalid;
      const auto needed = static_cast<std::uint32_t>(s.entries.size());
      if (!assets || capacity < needed) {
        *count = needed;
        return !assets ? Ok : Missing;
      }
      std::uint32_t index = 0;
      for (const auto kind : {NEXORA_CONTENT_ATLAS, NEXORA_CONTENT_CLIP})
        for (const auto &[id, entry] : s.entries) {
          const auto value = Describe(id, entry);
          if (value.kind == static_cast<std::uint32_t>(kind))
            assets[index++] = value;
        }
      *count = needed;
      return Ok;
    });
  }
  static int Find(void *ctx, std::uint64_t high, std::uint64_t low,
                  NexoraContentAsset *out) noexcept {
    return Call(ctx, [&](State &s) {
      if (!out || (!high && !low))
        return Invalid;
      const auto found = s.entries.find({high, low});
      if (found == s.entries.end())
        return Missing;
      *out = Describe(found->first, found->second);
      return Ok;
    });
  }
};
GameplayContent::GameplayContent() : GameplayContent(Limits{}) {}
GameplayContent::GameplayContent(Limits limits) : state_(std::make_unique<State>(limits)) {}
GameplayContent::~GameplayContent() = default;
int32_t GameplayContent::GetApi(NexoraGameplayContentApiV1 *out) noexcept {
  return State::Call(state_.get(), [&](State &) {
    if (!out)
      return Invalid;
    if (out->struct_size < sizeof(*out))
      return Missing;
    *out = {sizeof(*out),     1,           state_.get(), State::AtlasBytes,
            State::ClipBytes, State::List, State::Find};
    return Ok;
  });
}
std::optional<AtlasBinding> GameplayContent::ResolveAtlas(AssetId id) const {
  if (state_->thread != std::this_thread::get_id())
    throw std::logic_error("Content owner thread");
  const auto found = state_->entries.find(Identity(id));
  if (found == state_->entries.end() || !found->second.atlas)
    return {};
  return AtlasBinding{found->second.atlas, found->second.texture};
}
std::shared_ptr<const Clip> GameplayContent::ResolveClip(AssetId id) const {
  if (state_->thread != std::this_thread::get_id())
    throw std::logic_error("Content owner thread");
  const auto found = state_->entries.find(Identity(id));
  return found == state_->entries.end() ? nullptr : found->second.clip;
}
int32_t GameplayContent::Remove(AssetId id) noexcept {
  return State::Call(state_.get(), [&](State &s) {
    if (!id.high && !id.low)
      return Invalid;
    const auto found = s.entries.find(Identity(id));
    if (found == s.entries.end())
      return Missing;
    if (found->second.atlas)
      for (const auto &[_, e] : s.entries)
        if (e.clip && e.clip->atlas == id)
          return Lifecycle;
    if (s.retired.size() >= 65536)
      return Missing;
    s.PrepareRetirement(&found->second);
    s.Retire(found->second);
    s.bytes -= found->second.bytes;
    s.entries.erase(found);
    return Ok;
  });
}
GameplayContent::Usage GameplayContent::Statistics() const {
  if (state_->thread != std::this_thread::get_id())
    throw std::logic_error("Content owner thread");
  return state_->Statistics();
}
std::vector<std::uint64_t> GameplayContent::CollectRetiredTextures() {
  if (state_->thread != std::this_thread::get_id())
    throw std::logic_error("Content owner thread");
  std::vector<std::uint64_t> textures;
  textures.reserve(state_->retired.size());
  std::erase_if(state_->retired, [&](const auto &r) {
    if (!r.owner.expired())
      return false;
    if (r.texture)
      textures.push_back(r.texture);
    return true;
  });
  return textures;
}
} // namespace nexora::twod
