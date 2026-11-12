#include "nexora/nexora.h"
#include <cstdio>
#include <cstring>
#include <new>

namespace {
struct State final {
  NexoraGameplayHostV3 host;
  NexoraGameplayContentApiV1 content{};
  NexoraGameplayAssetsApiV1 assets{};
  uint64_t ticket{};
  unsigned stage{};
};
int32_t Create(void **out, const NexoraGameplayHostV3 *host) {
  if (!host || !host->allocate || !host->deallocate)
    return -2;
  void *memory = host->allocate(host->context, 1, sizeof(State), alignof(State));
  if (!memory)
    return -3;
  *out = new (memory) State{*host};
  return 0;
}
bool Source(const State &s, uint64_t low) {
  NexoraContentAsset asset{};
  return s.content.describe_asset(s.content.context, 201, low, &asset) == 0 &&
         asset.kind == NEXORA_CONTENT_SPRITE_SOURCE && asset.count == 1 && asset.width == 4 &&
         asset.height == 4 && asset.atlas_high == 200 && asset.atlas_low == low;
}
int32_t Start(void *opaque) {
  auto &s = *static_cast<State *>(opaque);
  NexoraGameplayServicesApiV1 services{};
  services.struct_size = sizeof(services);
  services.abi_version = 1;
  s.content.struct_size = sizeof(s.content);
  s.content.abi_version = 1;
  s.assets.struct_size = sizeof(s.assets);
  s.assets.abi_version = 1;
  uint64_t scene{};
  if (NexoraGetGameplayServicesV1(&s.host, &services) ||
      services.get_content_api(services.context, &s.content) ||
      services.get_assets_api(services.context, &s.assets) || !Source(s, 1) ||
      s.host.load_scene(s.host.context, "Main", 4, 0, &scene) ||
      s.host.activate_scene(s.host.context, scene))
    return -3;
  const uint32_t kinds[]{NEXORA_ASSET_CATALOG_SPRITE_SOURCE, NEXORA_ASSET_CATALOG_ATLAS,
                         NEXORA_ASSET_CATALOG_OPAQUE};
  for (unsigned i = 0; i < 3; ++i) {
    NexoraAssetCatalogEntryV1 entry{};
    entry.struct_size = sizeof(entry);
    if (s.assets.describe_catalog(s.assets.context, i, &entry) || entry.kind != kinds[i] ||
        !entry.file_bytes)
      return -3;
  }
  return s.assets.request(s.assets.context, 0, &s.ticket);
}
int32_t Update(void *opaque, double) {
  auto &s = *static_cast<State *>(opaque);
  if (s.stage == 6)
    return 0;
  NexoraAssetLoadStateV1 state{};
  state.struct_size = sizeof(state);
  if (s.assets.poll(s.assets.context, s.ticket, &state))
    return -3;
  if (state.phase < NEXORA_ASSET_READY)
    return 0;
  const bool failure = s.stage == 0 || s.stage == 3;
  if ((failure &&
       (state.phase != NEXORA_ASSET_FAILED || state.result != -2 || state.high || state.low)) ||
      (!failure && (state.phase != NEXORA_ASSET_READY || state.result)))
    return -3;
  if (s.stage == 0 && Source(s, 2))
    return -3;
  if (s.stage == 2 && (!Source(s, 2) || state.high != 201 || state.low != 2))
    return -3;
  if (s.stage == 4) {
    char raw[3]{};
    uint64_t size{};
    if (s.assets.copy_bytes(s.assets.context, s.ticket, raw, sizeof(raw), &size) || size != 3 ||
        std::memcmp(raw, "SP3", 3))
      return -3;
  }
  if (s.assets.release_request(s.assets.context, s.ticket))
    return -3;
  ++s.stage;
  if (s.stage < 5) {
    const uint32_t indices[]{0, 1, 0, 0, 2};
    return s.assets.request(s.assets.context, indices[s.stage], &s.ticket);
  }
  if (s.assets.remove_asset(s.assets.context, 200, 2) != -3 ||
      s.assets.remove_asset(s.assets.context, 201, 2) ||
      s.assets.remove_asset(s.assets.context, 200, 2) || !Source(s, 1) || Source(s, 2))
    return -3;
  NexoraAssetUsageV1 usage{};
  usage.struct_size = sizeof(usage);
  if (s.assets.get_usage(s.assets.context, &usage) || usage.active_assets != 2 || usage.requests ||
      usage.staging_bytes)
    return -3;
  s.stage = 6;
  std::fprintf(stderr, "SPRITE CATALOG PASS: preload, dependency order, deferred source, "
                       "duplicate, opaque, removal\n");
  return 0;
}
void Stop(void *) {}
void Destroy(void *opaque) {
  auto *s = static_cast<State *>(opaque);
  const auto host = s->host;
  s->~State();
  host.deallocate(host.context, 1, s, sizeof(State), alignof(State));
}
} // namespace
#if defined(_WIN32)
#define CATALOG_EXPORT __declspec(dllexport)
#else
#define CATALOG_EXPORT __attribute__((visibility("default")))
#endif
extern "C" CATALOG_EXPORT int32_t NexoraGameModuleLoad(uint32_t abi, NexoraGameModuleV3 *module) {
  if (abi != NEXORA_GAMEPLAY_ABI_VERSION || !module || module->struct_size < sizeof(*module))
    return -2;
  *module = {sizeof(*module), abi,    0,    nullptr, Create,  Start,
             nullptr,         Update, Stop, Destroy, nullptr, nullptr};
  return 0;
}
