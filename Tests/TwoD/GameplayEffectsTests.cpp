#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/TwoD/GameplayEffects.h"
#include "Nexora/TwoD/Lifecycle.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <thread>
using namespace nexora;
void DiagnosticContract() {
  bool invalid{};
  twod::GameplayEffects effects(7, {}, {}, {}, [&](auto &output) {
    output.completed_frame = 9;
    output.effects_revision = 3;
    output.timing_mask = 2;
    output.timing_source = 1;
    output.timing_status = 2;
    output.world_ms = invalid ? std::numeric_limits<double>::quiet_NaN() : 0.25;
    output.world_draws = 4;
    output.bloom_ms =
        std::numeric_limits<double>::quiet_NaN(); // Absent timing is explicitly cleared.
    return true;
  });
  const auto api = effects.Api();
  NexoraTwoDEffectsDiagnosticsV1 report{};
  report.struct_size = sizeof(report);
  report.abi_version = 1;
  assert(api.get_diagnostics(api.context, &report) == 0 && report.completed_frame == 9 &&
         report.world_ms == 0.25 && report.bloom_ms == 0);
  const auto valid = report;
  invalid = true;
  assert(api.get_diagnostics(api.context, &report) == -3 &&
         std::memcmp(&valid, &report, sizeof(report)) == 0);
  assert(api.get_diagnostics(api.context, nullptr) == -1);
  int32_t foreign{};
  std::thread thread([&] { foreign = api.get_diagnostics(api.context, &report); });
  thread.join();
  assert(foreign == -3);
  assert(api.set_debug_view(api.context, 1, 7) == 0);
  const auto first = effects.BeginFrame();
  assert(first->revision == 1 && first->debugMode == 1 && first->debugShadowSlot == 7);
  NexoraTwoDEffectsFrameV1 frame{};
  frame.struct_size = sizeof(frame);
  frame.abi_version = 1;
  frame.strength = 1;
  assert(api.submit_frame(api.context, &frame) == 0);
  assert(api.set_debug_view(api.context, 1, 7) ==
         0); // Unchanged request does not advance revision.
  assert(api.set_debug_view(api.context, 2, 0) == -1 &&
         api.set_debug_view(api.context, 1, 8) == -1);
  assert(api.set_debug_view(api.context, 0, 1) == -1);
  const auto second = effects.BeginFrame();
  assert(second->revision == 2 && second->debugMode == 1 && first->revision == 1);
  assert(api.set_debug_view(api.context, 0, 0) == 0);
  assert(effects.BeginFrame()->revision == 3 && effects.BeginFrame()->debugMode == 0);
  twod::GameplayEffects absent;
  const auto old = absent.Api();
  assert(old.get_diagnostics(old.context, &report) == -2 &&
         std::memcmp(&valid, &report, sizeof(report)) == 0);
  assert(old.set_debug_view(old.context, 1, 0) == -2 && old.set_debug_view(old.context, 0, 0) == 0);
}
int main() {
  DiagnosticContract();
  twod::GameplayEffects effects(
      7, [](uint32_t, uint64_t id) { return id == 42; },
      [](uint64_t hi, uint64_t lo, uint32_t region) { return hi == 1 && lo == 2 && region == 3; });
  auto api = effects.Api();
  NexoraTwoDEffectsCapabilitiesV1 caps{};
  caps.struct_size = sizeof(caps);
  caps.abi_version = 1;
  assert(api.get_capabilities(api.context, &caps) == 0 && caps.features == 7 &&
         caps.max_lights == 32 && caps.shadow_samples == 1024);
  NexoraTwoDEffectsLightV1 light{1, {2, 3}, {1, 0.5F, 0.25F}, 2, 10, 0.1F, 0.01F, 1, 1};
  NexoraTwoDEffectsOccluderV1 polygon{1, 1, 4, {{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
  NexoraTwoDEffectsSpriteV1 sprite{42, 0, 1, 1, 1, 1, 2, 3, 0, {1, 1, 1}, 4};
  NexoraTwoDEffectsFrameV1 frame{
      sizeof(frame), 1,      1,        1,      1, 1, {0.1F, 0.1F, 0.1F}, 1,
      {1, 12, 5, 0}, &light, &polygon, &sprite};
  NexoraTwoDEffectsStateV1 state{};
  state.struct_size = sizeof(state);
  state.abi_version = 1;
  const auto empty = effects.BeginFrame();
  assert(api.submit_frame(api.context, &frame) == 0);
  assert(api.get_state(api.context, &state) == 0 && state.accepted_revision == 1 &&
         state.active_revision == 0);
  light.position.x = 99;
  auto first = effects.BeginFrame();
  assert(first->lights[0].position.x == 2 && first->revision == 1 && empty->lights.empty());
  // Failed batches never replace a pending or active snapshot.
  auto invalid = frame;
  invalid.light_count = 33;
  assert(api.submit_frame(api.context, &invalid) == -1);
  invalid = frame;
  invalid.lights = nullptr;
  assert(api.submit_frame(api.context, &invalid) == -1);
  light.radius = std::numeric_limits<float>::quiet_NaN();
  assert(api.submit_frame(api.context, &frame) == -1);
  light.radius = 10;
  sprite.handle = 43; // stale generation / unknown identity
  assert(api.submit_frame(api.context, &frame) == -1);
  sprite.handle = 42;
  sprite.emission_region = 4;
  assert(api.submit_frame(api.context, &frame) == -1);
  sprite.emission_region = 3;
  polygon.vertices[2] = {-1, 0.5F}; // crossing opposite edge
  assert(api.submit_frame(api.context, &frame) == -1);
  polygon.vertices[2] = {0, 0}; // repeated nonadjacent vertex
  assert(api.submit_frame(api.context, &frame) == -1);
  polygon.vertices[2] = {1, 1};
  assert(effects.BeginFrame() == first);
  assert(api.get_state(api.context, &state) == 0 && state.accepted_revision == 1);
  assert(api.submit_frame(api.context, &frame) == 0);
  assert(api.submit_frame(api.context, &invalid) == -1);
  auto second = effects.BeginFrame();
  assert(second->revision == 2 && second->lights[0].position.x == 99 &&
         first->lights[0].position.x == 2);
  // Every scalar version check preserves caller output.
  caps.struct_size = 1;
  const auto before = caps;
  assert(api.get_capabilities(api.context, &caps) == -2 &&
         std::memcmp(&before, &caps, sizeof(caps)) == 0);
  assert(api.get_state(api.context, nullptr) == -1 && api.submit_frame(nullptr, &frame) == -1);
  int result = 0;
  std::thread worker([&] { result = api.submit_frame(api.context, &frame); });
  worker.join();
  assert(result == -3);
  twod::GameplayEffects unavailable;
  auto no = unavailable.Api();
  assert(no.submit_frame(no.context, &frame) == -2);
  frame.enabled = 0;
  frame.sprite_count = 0;
  assert(no.submit_frame(no.context, &frame) == 0); // data is retained, never rendered
  // Full capacity: 8 shadowed lights accepted, ninth rejected; no truncated success.
  std::array<NexoraTwoDEffectsLightV1, 32> lights{};
  for (uint32_t i = 0; i < lights.size(); ++i) {
    lights[i] = light;
    lights[i].id = i + 1;
    lights[i].shadow = i < 8;
  }
  frame.enabled = 1;
  frame.light_count = uint32_t(lights.size());
  frame.lights = lights.data();
  assert(api.submit_frame(api.context, &frame) == 0);
  lights[8].shadow = 1;
  assert(api.submit_frame(api.context, &frame) == -1);
  lights[8].shadow = 0;
  lights[31].id = lights[0].id;
  assert(api.submit_frame(api.context, &frame) == -1);
  // Polygon edge budget is independent from polygon/vertex count limits.
  frame.light_count = 0;
  frame.sprite_count = 0;
  std::array<NexoraTwoDEffectsOccluderV1, 129> polygons{};
  for (uint32_t i = 0; i < polygons.size(); ++i) {
    auto &o = polygons[i];
    o.id = i + 1;
    o.vertex_count = 16;
    for (uint32_t j = 0; j < 16; ++j) {
      const double angle = double(j) * 6.283185307179586 / 16;
      o.vertices[j] = {float(std::cos(angle)), float(std::sin(angle))};
    }
  }
  frame.occluders = polygons.data();
  frame.occluder_count = 128;
  assert(api.submit_frame(api.context, &frame) == 0); // exactly 2048 edges
  frame.occluder_count = 129;
  assert(api.submit_frame(api.context, &frame) == -1);
  frame.occluder_count = 1;
  polygon.vertex_count = 5;
  polygon.vertices[0] = {0, 0};
  polygon.vertices[1] = {2, 0};
  polygon.vertices[2] = {1, 1};
  polygon.vertices[3] = {2, 2};
  polygon.vertices[4] = {0, 2};
  frame.occluders = &polygon;
  assert(api.submit_frame(api.context, &frame) == 0); // simple concave accepted
  polygon.vertex_count = 4;
  polygon.vertices[0] = {0, 0};
  polygon.vertices[1] = {2, 0};
  polygon.vertices[2] = {1, 0};
  polygon.vertices[3] = {0, 1};
  assert(api.submit_frame(api.context, &frame) == -1); // adjacent backtracking rejected
  // One ABI prefix per byte: never write past the negotiated caller prefix.
  runtime::GameplayServices services;
  services.twod_effects = api;
  alignas(NexoraGameplayServicesApiV1)
      std::array<unsigned char, sizeof(NexoraGameplayServicesApiV1) + 16>
          bytes{};
  for (size_t size = NEXORA_GAMEPLAY_SERVICES_V1_BASE_SIZE;
       size <= sizeof(NexoraGameplayServicesApiV1); ++size) {
    bytes.fill(0xa5);
    auto *table = reinterpret_cast<NexoraGameplayServicesApiV1 *>(bytes.data());
    table->struct_size = uint32_t(size);
    table->abi_version = 1;
    assert(services.GetApi(table) == 0 && table->struct_size <= size);
    for (size_t i = size; i < bytes.size(); ++i)
      assert(bytes[i] == 0xa5);
    if (table->struct_size == sizeof(*table)) {
      NexoraGameplayTwoDEffectsApiV1 received{};
      received.struct_size = sizeof(received);
      received.abi_version = 1;
      assert(table->get_twod_effects_api(table->context, &received) == 0 &&
             received.context == api.context);
    }
  }
  // The extended effects table preserves its original three-callback prefix byte-for-byte.
  NexoraGameplayServicesApiV1 full{};
  full.struct_size = sizeof(full);
  full.abi_version = 1;
  assert(services.GetApi(&full) == 0);
  constexpr auto effectsPrefix = offsetof(NexoraGameplayTwoDEffectsApiV1, pool_identity);
  alignas(NexoraGameplayTwoDEffectsApiV1) std::array<unsigned char, sizeof(api) + 16> effectBytes{};
  for (std::size_t requested = 8; requested <= sizeof(api) + 8; ++requested) {
    effectBytes.fill(0xa5);
    auto *out = reinterpret_cast<NexoraGameplayTwoDEffectsApiV1 *>(effectBytes.data());
    out->struct_size = uint32_t(requested);
    out->abi_version = 1;
    assert(full.get_twod_effects_api(full.context, out) == (requested < effectsPrefix ? -2 : 0));
    std::size_t written = requested < effectsPrefix ? 8 : effectsPrefix;
    if (requested >= effectsPrefix)
      for (const auto end : {offsetof(NexoraGameplayTwoDEffectsApiV1, get_diagnostics),
                             offsetof(NexoraGameplayTwoDEffectsApiV1, set_debug_view), sizeof(api)})
        if (end <= requested)
          written = end;
    for (auto n = written; n < effectBytes.size(); ++n)
      assert(effectBytes[n] == 0xa5);
    if (requested >= effectsPrefix)
      assert(out->struct_size == written);
  }
  services.twod_effects.struct_size = effectsPrefix; // Old registered host, new caller.
  NexoraGameplayTwoDEffectsApiV1 old{};
  old.struct_size = sizeof(old);
  old.abi_version = 1;
  assert(full.get_twod_effects_api(full.context, &old) == 0 && old.struct_size == effectsPrefix &&
         !old.pool_identity);

  twod::InstancePool instances(1), foreign(1);
  twod::Instance value;
  value.sprite.atlas = {1, 2};
  value.sprite.region = 1;
  auto handles = instances.Create(std::span(&value, 1));
  auto otherHandles = foreign.Create(std::span(&value, 1));
  assert(handles && otherHandles);
  const auto poolResolver = [&](twod::EffectsPoolIdentity h) {
    return instances.Find({h.owner, h.index, h.generation}) != nullptr;
  };
  twod::GameplayEffects pooled(1, {}, {}, poolResolver);
  const auto pooledApi = pooled.Api();
  auto h = handles->front();
  uint64_t token = 0, repeat = 0;
  assert(pooledApi.pool_identity(pooledApi.context, h.owner, h.index, h.generation, &token) == 0 &&
         token);
  assert(pooledApi.pool_identity(pooledApi.context, h.owner, h.index, h.generation, &repeat) == 0 &&
         repeat == token);
  const auto foreignHandle = otherHandles->front();
  assert(pooledApi.pool_identity(pooledApi.context, foreignHandle.owner, foreignHandle.index,
                                 foreignHandle.generation, &repeat) == -1 &&
         repeat == token);
  assert(pooledApi.pool_identity(pooledApi.context, h.owner, h.index, h.generation, nullptr) == -1);
  std::thread poolWorker([&] {
    result = pooledApi.pool_identity(pooledApi.context, h.owner, h.index, h.generation, &repeat);
  });
  poolWorker.join();
  assert(result == -3 && repeat == token);
  NexoraTwoDEffectsSpriteV1 poolSprite{};
  poolSprite.kind = NEXORA_TWOD_EFFECTS_POOL;
  poolSprite.handle = token;
  NexoraTwoDEffectsFrameV1 poolFrame{};
  poolFrame.struct_size = sizeof(poolFrame);
  poolFrame.abi_version = 1;
  poolFrame.enabled = 1;
  poolFrame.strength = 1;
  poolFrame.sprite_count = 1;
  poolFrame.sprites = &poolSprite;
  assert(pooledApi.submit_frame(pooledApi.context, &poolFrame) == 0);
  const auto retained = pooled.BeginFrame();
  assert(instances.Destroy(*handles));
  handles = instances.Create(std::span(&value, 1));
  assert(handles && handles->front().index == h.index &&
         handles->front().generation != h.generation);
  assert(pooledApi.submit_frame(pooledApi.context, &poolFrame) == -1);
  assert(!pooled.ResolvePoolIdentity(token));
  assert(pooledApi.pool_identity(pooledApi.context, h.owner, h.index, h.generation, &repeat) ==
             -1 &&
         repeat == token);
  h = handles->front();
  assert(pooledApi.pool_identity(pooledApi.context, h.owner, h.index, h.generation, &repeat) == 0 &&
         repeat != token);
  assert(pooled.BeginFrame() == retained && retained->sprites.front().handle == token);
  twod::GameplayEffects another(1, {}, {}, poolResolver);
  const auto anotherApi = another.Api();
  uint64_t anotherToken{};
  assert(anotherApi.pool_identity(anotherApi.context, h.owner, h.index, h.generation,
                                  &anotherToken) == 0 &&
         anotherToken != repeat);
  poolSprite.handle = repeat;
  assert(pooledApi.submit_frame(pooledApi.context, &poolFrame) == 0);
  assert(anotherApi.submit_frame(anotherApi.context, &poolFrame) == -1); // Foreign service token.
  assert(no.pool_identity(no.context, h.owner, h.index, h.generation, &anotherToken) == -2);
}
