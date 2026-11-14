#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/TwoD/GameplayBridge.h"
#include "Nexora/TwoD/GameplayContent.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <thread>

using namespace nexora;
namespace {
static_assert(sizeof(NexoraTwoDSprite) == 64 && sizeof(NexoraTwoDInstance) == 144);
static_assert(sizeof(NexoraTwoDSourceSpriteV1) == 80 && sizeof(NexoraTwoDSourceInstanceV1) == 160);
NexoraTwoDSprite Legacy() { return {8, 9, 1, 8, {1, 1}, {0.5F, 0.5F}, {1, 1, 1, 1}, 0, 0}; }
NexoraTwoDSourceSpriteV1 Source() {
  auto style = Legacy();
  style.atlas_high = style.atlas_low = 0;
  style.region = 0;
  style.flags = 13;
  style.order = 7;
  return {style, 11, 12};
}
NexoraTwoDSourceInstanceV1 Instance() { return {{{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}}, Source()}; }
void Tables(twod::GameplayBridge &bridge) {
  alignas(NexoraTwoDApiV1) std::array<unsigned char, sizeof(NexoraTwoDApiV1) + 16> bytes{};
  for (size_t requested = 0; requested <= sizeof(NexoraTwoDApiV1) + 8; ++requested) {
    bytes.fill(0xa5);
    auto *api = reinterpret_cast<NexoraTwoDApiV1 *>(bytes.data());
    api->struct_size = uint32_t(requested);
    const auto before = bytes;
    const auto result = bridge.GetApi(api);
    if (requested < NEXORA_2D_API_V1_BASE_SIZE) {
      assert(result == -2 && bytes == before);
      continue;
    }
    const auto count = requested < sizeof(*api) ? NEXORA_2D_API_V1_BASE_SIZE : sizeof(*api);
    assert(result == 0 && api->struct_size == count && api->abi_version == 1);
    for (size_t i = count; i < bytes.size(); ++i)
      assert(bytes[i] == 0xa5);
    NexoraTwoDSourceApiV1 sources{};
    sources.struct_size = sizeof(sources);
    const auto original = sources;
    assert(NexoraGetTwoDSourcesV1(api, &sources) == (count < sizeof(*api) ? -2 : 0));
    if (count < sizeof(*api))
      assert(std::memcmp(&sources, &original, sizeof(sources)) == 0);
    else
      assert(sources.get_sprite && sources.write_instances && sources.context == api->context);
  }
  NexoraTwoDSourceApiV1 shortTable{};
  shortTable.struct_size = sizeof(shortTable) - 1;
  const auto original = shortTable;
  assert(bridge.GetSourceApi(&shortTable) == -2 &&
         std::memcmp(&original, &shortTable, sizeof(original)) == 0);
  assert(bridge.GetSourceApi(nullptr) == -1);
  struct Extended final {
    NexoraTwoDSourceApiV1 api{};
    uint64_t tail{123};
  } extended;
  extended.api.struct_size = sizeof(extended);
  assert(bridge.GetSourceApi(&extended.api) == 0 && extended.tail == 123 &&
         extended.api.struct_size == sizeof(extended.api));
}
void Registry(runtime::GameplayServices &services, twod::GameplayContent &content) {
  services.content.struct_size = sizeof(services.content);
  assert(content.GetApi(&services.content) == 0);
  services.sprite_content.struct_size = sizeof(services.sprite_content);
  assert(content.GetSpriteApi(&services.sprite_content) == 0);
  alignas(NexoraGameplayServicesApiV1)
      std::array<unsigned char, sizeof(NexoraGameplayServicesApiV1) + 16>
          bytes{};
  for (size_t size = NEXORA_GAMEPLAY_SERVICES_V1_BASE_SIZE;
       size <= sizeof(NexoraGameplayServicesApiV1) + 8; ++size) {
    bytes.fill(0xa5);
    auto *table = reinterpret_cast<NexoraGameplayServicesApiV1 *>(bytes.data());
    table->struct_size = uint32_t(size);
    table->abi_version = 1;
    assert(services.GetApi(table) == 0 && table->struct_size <= size);
    for (size_t i = table->struct_size; i < bytes.size(); ++i)
      assert(bytes[i] == 0xa5);
    if (table->struct_size == sizeof(*table)) {
      NexoraGameplaySpriteContentApiV1 output{};
      output.struct_size = sizeof(output);
      output.abi_version = 1;
      assert(table->get_sprite_content_api(table->context, &output) == 0 &&
             output.context == services.sprite_content.context);
    }
  }
}
} // namespace
int main() {
  runtime::World world(runtime::WorldKind::Play);
  const auto sceneId = world.LoadScene("Source API");
  const auto entity = world.CreateEntity(sceneId).id;
  const auto foreignScene = world.LoadScene("Other");
  const auto foreign = world.CreateEntity(foreignScene).id;
  twod::SpriteScene scene(world, sceneId);
  twod::InstancePool pool(4);
  twod::GameplayBridge bridge(world, scene, {}, nullptr, &pool);
  Tables(bridge);
  NexoraTwoDApiV1 legacy{};
  legacy.struct_size = sizeof(legacy);
  assert(bridge.GetApi(&legacy) == 0);
  NexoraTwoDSourceApiV1 sources{};
  sources.struct_size = sizeof(sources);
  assert(NexoraGetTwoDSourcesV1(&legacy, &sources) == 0);
  auto source = Source();
  assert(sources.set_sprite(sources.context, entity, &source) == 0);
  assert(scene.Find(entity)->source == twod::AssetId({11, 12}));
  NexoraTwoDSourceSpriteV1 read{};
  assert(sources.get_sprite(sources.context, entity, &read) == 0 && read.source_high == 11 &&
         read.source_low == 12 && read.sprite.flags == 13 && read.sprite.order == 7);
  auto legacyRead = Legacy();
  assert(legacy.get_sprite(legacy.context, entity, &legacyRead) == -2 && legacyRead.region == 1);
  assert(legacy.play_animation(legacy.context, entity, 3, 4, 1) == -2);
  auto bad = source;
  bad.sprite.flags = 16;
  assert(sources.set_sprite(sources.context, entity, &bad) == -1);
  bad = source;
  bad.sprite.size.x = std::numeric_limits<float>::quiet_NaN();
  assert(sources.set_sprite(sources.context, entity, &bad) == -1);
  assert(sources.set_sprite(sources.context, foreign, &source) == -1 &&
         scene.Find(entity)->source == twod::AssetId({11, 12}));
  auto old = Legacy();
  assert(legacy.set_sprite(legacy.context, entity, &old) == 0 && !scene.Find(entity)->source);
  assert(sources.get_sprite(sources.context, entity, &read) == 0 && !read.source_high &&
         !read.source_low && read.sprite.atlas_high == 8);
  assert(sources.set_sprite(sources.context, entity, &source) == 0);
  std::array<NexoraTwoDSourceInstanceV1, 2> inputs{Instance(), Instance()};
  inputs[1].transform.position.x = 2;
  std::array<NexoraTwoDInstanceHandle, 2> handles{};
  assert(sources.create_instances(sources.context, inputs.data(), 2, handles.data()) == 0);
  assert(pool.Size() == 2);
  std::array<NexoraTwoDSourceInstanceV1, 2> outputs{};
  assert(sources.read_instances(sources.context, handles.data(), 2, outputs.data()) == 0 &&
         outputs[0].sprite.source_low == 12 && outputs[1].transform.position.x == 2);
  std::array<NexoraTwoDInstance, 2> legacyOutputs{};
  legacyOutputs[0].sprite.order = 123;
  assert(legacy.read_instances(legacy.context, handles.data(), 2, legacyOutputs.data()) == -2 &&
         legacyOutputs[0].sprite.order == 123);
  auto updates = inputs;
  updates[0].transform.position.x = 5;
  updates[1].transform.scale.x = std::numeric_limits<double>::infinity();
  assert(sources.write_instances(sources.context, handles.data(), 2, updates.data()) == -1 &&
         pool.Find({handles[0].owner, handles[0].index, handles[0].generation})->transform.x == 0);
  const std::array<NexoraTwoDInstanceHandle, 2> duplicates{handles[0], handles[0]};
  assert(sources.write_instances(sources.context, duplicates.data(), 2, inputs.data()) == -1);
  auto invalid = handles;
  ++invalid[1].owner;
  outputs[0].sprite.sprite.order = 321;
  assert(sources.read_instances(sources.context, invalid.data(), 2, outputs.data()) == -1 &&
         outputs[0].sprite.sprite.order == 321);
  inputs[1].sprite = {Legacy(), 0, 0};
  assert(sources.write_instances(sources.context, handles.data(), 2, inputs.data()) == 0);
  assert(!pool.Find({handles[1].owner, handles[1].index, handles[1].generation})->sprite.source);
  const auto size = pool.Size();
  auto failed = inputs;
  failed[1].sprite.sprite.flags = 16;
  std::array<NexoraTwoDInstanceHandle, 2> untouched{{{99, 98, 97}, {96, 95, 94}}};
  assert(sources.create_instances(sources.context, failed.data(), 2, untouched.data()) == -1 &&
         pool.Size() == size && untouched[0].owner == 99);
  assert(sources.create_instances(sources.context, nullptr, 0, nullptr) == 0 &&
         sources.read_instances(sources.context, nullptr, 0, nullptr) == 0 &&
         sources.write_instances(sources.context, nullptr, 0, nullptr) == 0);
  assert(sources.create_instances(sources.context, nullptr, 100001, nullptr) == -1);
  std::thread worker([&] {
    assert(sources.set_sprite(sources.context, entity, &source) == -3);
    NexoraTwoDSourceApiV1 api{};
    api.struct_size = sizeof(api);
    assert(bridge.GetSourceApi(&api) == -3 && !api.set_sprite);
  });
  worker.join();
  // Register through the same independently negotiated services suffix used by the Player.
  twod::GameplayContent content;
  runtime::GameplayServices services;
  Registry(services, content);
  NexoraGameplayServicesApiV1 serviceApi{};
  serviceApi.struct_size = sizeof(serviceApi);
  serviceApi.abi_version = 1;
  assert(services.GetApi(&serviceApi) == 0);
  NexoraGameplaySpriteContentApiV1 registration{};
  registration.struct_size = sizeof(registration);
  registration.abi_version = 1;
  assert(serviceApi.get_sprite_content_api(serviceApi.context, &registration) == 0);
  twod::Image image{1, 2, 2, {}};
  image.rgba.assign(16, std::byte{255});
  const auto atlas = twod::PackAtlas({8, 9}, std::span(&image, 1), 8, 8, true);
  assert(atlas);
  const auto atlasBytes = twod::EncodeAtlas(*atlas);
  uint64_t high{}, low{};
  assert(services.content.register_atlas(services.content.context, atlasBytes.data(),
                                         atlasBytes.size(), &high, &low) == 0);
  twod::SpriteAsset asset;
  asset.id = {11, 12};
  asset.atlas = {8, 9};
  asset.region = 1;
  asset.sourceWidth = asset.sourceHeight = 4;
  asset.trimX = asset.trimY = 1;
  asset.trimWidth = asset.trimHeight = 2;
  asset.pivotX = 2;
  asset.pixelsPerUnit = 2;
  const auto bytes = twod::EncodeSpriteAsset(asset);
  assert(registration.register_sprite_source(registration.context, bytes.data(), bytes.size(),
                                             &high, &low) == 0 &&
         high == 11 && low == 12);
  auto frame = scene.Extract(
      {}, [&](auto id) { return content.ResolveAtlas(id); },
      [&](auto id) { return content.ResolveSprite(id); });
  assert(frame && frame->statistics.visible == 1 && frame->retainedAtlases.size() == 1);
  assert(legacy.destroy_instances(legacy.context, handles.data(), 2) == 0 && pool.Size() == 0);
  assert(sources.read_instances(sources.context, handles.data(), 1, outputs.data()) == -1);
  assert(sources.set_sprite(sources.context, entity, nullptr) == 0 && !scene.Find(entity));
}
