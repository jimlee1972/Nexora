#include "Nexora/TwoD/GameplayAssets.h"
#include "nexora/nexora.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <future>
#include <map>
#include <stdexcept>
#include <thread>

namespace nexora::twod {
namespace {
constexpr int Ok = NEXORA_GAMEPLAY_OK, Invalid = NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
              Missing = NEXORA_GAMEPLAY_ERROR_UNSUPPORTED,
              Lifecycle = NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
constexpr std::uint64_t FileLimit = 64 * 1024 * 1024;
} // namespace
struct GameplayAssets::State final {
  struct Request final {
    NexoraAssetLoadStateV1 state{};
    std::uint64_t reservation{};
    Bytes bytes;
  };
  GameplayContent &content;
  std::vector<GameplayAssetFile> catalog;
  Reader reader;
  const std::thread::id thread{std::this_thread::get_id()};
  std::map<std::uint64_t, Request> requests;
  std::uint64_t next{1}, reserved{}, limit{}, peak{}, current{}, current_bytes{};
  bool abandoned{}, stopped{};
  std::future<Bytes> pending;
  State(GameplayContent &c, std::vector<GameplayAssetFile> files, Reader read, std::uint64_t size)
      : content(c), catalog(std::move(files)), reader(std::move(read)), limit(size) {
    if (!reader || catalog.size() > 4096 || !limit || limit > 128 * 1024 * 1024)
      throw std::invalid_argument("Asset catalog capacity");
    for (const auto &f : catalog)
      if (f.kind < 1 || f.kind > 3 || !f.bytes || f.bytes > FileLimit || f.path.empty() ||
          f.path.size() > 1024)
        throw std::invalid_argument("Asset catalog entry");
  }
  template <class Fn> static int Call(void *ctx, Fn &&fn) noexcept {
    if (!ctx)
      return Invalid;
    auto &s = *static_cast<State *>(ctx);
    if (s.thread != std::this_thread::get_id())
      return Lifecycle;
    if (s.stopped)
      return Lifecycle;
    try {
      return fn(s);
    } catch (...) {
      return Lifecycle;
    }
  }
  void Observe() {
    const auto u = content.Statistics();
    peak = std::max(peak, u.active_bytes + u.retired_bytes);
  }
  static int Describe(void *ctx, std::uint32_t index, NexoraAssetCatalogEntryV1 *out) noexcept {
    return Call(ctx, [&](State &s) {
      if (!out || out->struct_size < sizeof(*out))
        return Invalid;
      if (index >= s.catalog.size())
        return Missing;
      const auto &f = s.catalog[index];
      *out = {sizeof(*out), f.kind, f.bytes};
      return Ok;
    });
  }
  static int Load(void *ctx, std::uint32_t index, std::uint64_t *out) noexcept {
    return Call(ctx, [&](State &s) {
      if (!out)
        return Invalid;
      if (index >= s.catalog.size() || s.requests.size() >= 64 || !s.next)
        return Missing;
      const auto size = s.catalog[index].bytes;
      if (size > s.limit || s.reserved > s.limit - size)
        return Missing;
      const auto ticket = s.next;
      Request request;
      request.state = {sizeof(NexoraAssetLoadStateV1), NEXORA_ASSET_QUEUED, 0, index, 0, 0, size};
      request.reservation = size;
      s.requests.emplace(ticket, std::move(request));
      ++s.next;
      s.reserved += size;
      *out = ticket;
      return Ok;
    });
  }
  static int Poll(void *ctx, std::uint64_t ticket, NexoraAssetLoadStateV1 *out) noexcept {
    return Call(ctx, [&](State &s) {
      if (!out || out->struct_size < sizeof(*out))
        return Invalid;
      const auto found = s.requests.find(ticket);
      if (found == s.requests.end())
        return Missing;
      *out = found->second.state;
      return Ok;
    });
  }
  static int Release(void *ctx, std::uint64_t ticket) noexcept {
    return Call(ctx, [&](State &s) {
      const auto found = s.requests.find(ticket);
      if (found == s.requests.end())
        return Missing;
      if (s.current == ticket)
        s.abandoned = true;
      else
        s.reserved -= found->second.reservation;
      s.requests.erase(found);
      return Ok;
    });
  }
  static int Remove(void *ctx, std::uint64_t high, std::uint64_t low) noexcept {
    return Call(ctx, [&](State &s) {
      const auto result = s.content.Remove({high, low});
      s.Observe();
      return result;
    });
  }
  static int Usage(void *ctx, NexoraAssetUsageV1 *out) noexcept {
    return Call(ctx, [&](State &s) {
      if (!out || out->struct_size < sizeof(*out))
        return Invalid;
      s.Observe();
      const auto u = s.content.Statistics();
      *out = {sizeof(*out),
              u.active_assets,
              u.retired_generations,
              static_cast<std::uint32_t>(s.requests.size()),
              u.active_bytes,
              u.retired_bytes,
              u.texture_bytes,
              s.reserved,
              s.peak};
      return Ok;
    });
  }
  static int Copy(void *ctx, std::uint64_t ticket, void *out, std::uint64_t capacity,
                  std::uint64_t *length) noexcept {
    return Call(ctx, [&](State &s) {
      if (!length || (!out && capacity))
        return Invalid;
      const auto found = s.requests.find(ticket);
      if (found == s.requests.end())
        return Missing;
      const auto &r = found->second;
      if (r.state.phase != NEXORA_ASSET_READY || s.catalog[r.state.catalog_index].kind != 3)
        return Lifecycle;
      const auto size = r.bytes.size();
      if (!out || capacity < size) {
        *length = size;
        return out ? Missing : Ok;
      }
      std::memcpy(out, r.bytes.data(), size);
      *length = size;
      return Ok;
    });
  }
  int Publish(Request &r, Bytes bytes) {
    const auto &f = catalog[r.state.catalog_index];
    if (bytes.size() != f.bytes)
      return Invalid;
    if (f.kind == 3) {
      r.bytes = std::move(bytes);
      return Ok;
    }
    const auto blob = runtime::AssetCooker::Deserialize(bytes);
    if (!blob || blob->type != (f.kind == 1 ? "sprite_atlas" : "sprite_clip"))
      return Invalid;
    // Catalog loads never replace somebody else's active UUID, including preload assets.
    NexoraGameplayContentApiV1 api{};
    api.struct_size = sizeof(api);
    if (content.GetApi(&api) != Ok)
      return Lifecycle;
    NexoraContentAsset description{};
    if (api.describe_asset(api.context, blob->id.high, blob->id.low, &description) == Ok)
      return Missing;
    if (f.kind == 1) {
      const auto decoded = DecodeAtlas(blob->payload);
      if (!decoded || decoded->id != blob->id || !blob->dependencies.empty())
        return Invalid;
    } else {
      const auto decoded = DecodeClip(blob->payload);
      if (!decoded || decoded->id != blob->id ||
          blob->dependencies != std::vector<AssetId>{decoded->atlas})
        return Invalid;
    }
    const auto call = f.kind == 1 ? api.register_atlas : api.register_clip;
    const auto result =
        call(api.context, blob->payload.data(), blob->payload.size(), &r.state.high, &r.state.low);
    return result;
  }
  void Tick() {
    if (thread != std::this_thread::get_id())
      throw std::logic_error("Assets owner thread");
    if (stopped)
      return;
    if (current) {
      if (pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
      auto found = requests.find(current);
      int result = Lifecycle;
      try {
        auto bytes = pending.get();
        if (!abandoned)
          result = Publish(found->second, std::move(bytes));
      } catch (...) {
        result = Lifecycle;
      }
      if (abandoned)
        reserved -= current_bytes;
      else {
        auto &r = found->second;
        r.state.result = result;
        r.state.phase = result == Ok ? NEXORA_ASSET_READY : NEXORA_ASSET_FAILED;
        if (result != Ok || catalog[r.state.catalog_index].kind != 3) {
          reserved -= r.reservation;
          r.reservation = 0;
        }
      }
      current = current_bytes = 0;
      abandoned = false;
      Observe();
    }
    for (auto &[ticket, r] : requests) {
      if (r.state.phase != NEXORA_ASSET_QUEUED)
        continue;
      try {
        const auto file = catalog[r.state.catalog_index];
        pending = std::async(std::launch::async, [read = reader, file] { return read(file); });
        current = ticket;
        current_bytes = r.reservation;
        r.state.phase = NEXORA_ASSET_LOADING;
      } catch (...) {
        reserved -= r.reservation;
        r.reservation = 0;
        r.state.phase = NEXORA_ASSET_FAILED;
        r.state.result = Lifecycle;
      }
      break;
    }
  }
};
GameplayAssets::GameplayAssets(GameplayContent &c, std::vector<GameplayAssetFile> files,
                               Reader read, std::uint64_t staging)
    : state_(std::make_unique<State>(c, std::move(files), std::move(read), staging)) {}
GameplayAssets::~GameplayAssets() = default;
void GameplayAssets::Tick() { state_->Tick(); }
void GameplayAssets::Shutdown() {
  if (state_->thread != std::this_thread::get_id())
    throw std::logic_error("Assets owner thread");
  if (state_->pending.valid())
    state_->pending.wait();
  state_->pending = {};
  state_->requests.clear();
  state_->current = state_->current_bytes = state_->reserved = 0;
  state_->stopped = true;
}
int32_t GameplayAssets::GetApi(NexoraGameplayAssetsApiV1 *out) noexcept {
  return State::Call(state_.get(), [&](State &s) {
    if (!out)
      return Invalid;
    if (out->struct_size < sizeof(*out) || out->abi_version != 1)
      return Missing;
    *out = {sizeof(*out),
            1,
            &s,
            State::Describe,
            State::Load,
            State::Poll,
            State::Release,
            State::Remove,
            State::Usage,
            State::Copy};
    return Ok;
  });
}
} // namespace nexora::twod
