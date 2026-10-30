#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/TwoD/GameplayEffects.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <thread>
using namespace nexora;
int main() {
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
}
