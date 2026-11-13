#include "Nexora/TwoD/GameplayAssets.h"
#include "Nexora/TwoD/GameplayBridge.h"
#include "Nexora/TwoD/GameplayContent.h"
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace nexora;
namespace {
constexpr twod::AssetId atlasId{8, 9}, firstId{11, 12}, secondId{13, 14}, clipId{15, 16};
void Check(bool value, const char *reason) {
  if (!value)
    throw std::runtime_error(reason);
}
twod::Clip Clip() {
  return {clipId, {}, true, {{0, 0.25, "second", firstId}, {0, 0.5, "first", secondId}}};
}
void CatalogClip(twod::GameplayContent &content, const twod::Bytes &payload) {
  runtime::DerivedDataCache cache;
  const auto blob = runtime::AssetCooker{}.Cook(
      {clipId, "sprite_clip", {firstId, secondId}, payload}, "test", "source-animation", cache);
  Check(bool(blob), "cook source Clip");
  auto wrong = *blob;
  wrong.dependencies = {atlasId};
  const std::array data{runtime::AssetCooker::Serialize(wrong),
                        runtime::AssetCooker::Serialize(*blob)};
  twod::GameplayAssets assets(
      content, {{2, data[0].size(), "0"}, {2, data[1].size(), "1"}},
      [&](const auto &file) { return data[std::size_t(file.path.front() - '0')]; });
  NexoraGameplayAssetsApiV1 api{};
  api.struct_size = sizeof(api);
  api.abi_version = 1;
  Check(assets.GetApi(&api) == 0, "asset catalog API");
  for (std::uint32_t index = 0; index < data.size(); ++index) {
    std::uint64_t ticket{};
    Check(api.request(api.context, index, &ticket) == 0, "request source Clip");
    NexoraAssetLoadStateV1 state{};
    state.struct_size = sizeof(state);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    do {
      assets.Tick();
      Check(api.poll(api.context, ticket, &state) == 0, "poll source Clip");
      if (state.phase >= NEXORA_ASSET_READY)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    Check(state.phase == (index ? NEXORA_ASSET_READY : NEXORA_ASSET_FAILED),
          "catalog source dependency admission");
    Check(bool(content.ResolveClip(clipId)) == bool(index),
          "failed catalog metadata published a Clip");
    Check(api.release_request(api.context, ticket) == 0, "release source Clip ticket");
  }
}
void Wire() {
  const twod::Clip legacy{{3, 4}, {1, 2}, true, {{11, 0.1, "a"}, {73, 0.2, "b"}, {901, 0.3, ""}}};
  const auto old = twod::EncodeClip(legacy);
  std::uint64_t hash = UINT64_C(14695981039346656037);
  for (const auto value : old)
    hash = (hash ^ std::to_integer<unsigned char>(value)) * UINT64_C(1099511628211);
  Check(old.size() == 98 && hash == UINT64_C(0x6f6732df49dcb468),
        "legacy bytes differ from independently compiled schema-1 baseline");
  auto clip = Clip();
  clip.frames.push_back(clip.frames.front());
  const auto bytes = twod::EncodeClip(clip);
  Check(bytes.size() == 161 && bytes[4] == std::byte{2}, "source Clip schema/size");
  const auto decoded = twod::DecodeClip(bytes);
  Check(decoded && twod::ClipUsesSources(*decoded) && twod::EncodeClip(*decoded) == bytes &&
            twod::ClipDependencies(*decoded) == std::vector{firstId, secondId},
        "source Clip canonical dependency closure");
  for (std::size_t n = 0; n < bytes.size(); ++n)
    Check(!twod::DecodeClip(std::span(bytes).first(n)), "truncated source Clip admitted");
  auto corrupt = bytes;
  corrupt.push_back(std::byte{0});
  Check(!twod::DecodeClip(corrupt), "trailing source Clip admitted");
  corrupt = bytes;
  corrupt[4] = std::byte{1};
  Check(!twod::DecodeClip(corrupt), "source payload admitted as legacy Clip");
  corrupt[4] = std::byte{3};
  Check(!twod::DecodeClip(corrupt), "unknown Clip schema admitted");
  auto bad = clip;
  bad.frames[1].source.reset();
  Check(!twod::ValidateClip(bad), "mixed binding Clip admitted");
  bad = clip;
  bad.atlas = atlasId;
  Check(twod::EncodeClip(bad).empty(), "ambiguous source Clip Atlas admitted");
  bad = clip;
  bad.frames[0].source = twod::AssetId{};
  Check(!twod::ValidateClip(bad), "present zero source UUID admitted");
  bad.frames[0].source = clipId;
  Check(!twod::ValidateClip(bad), "self dependency admitted");
  bad = clip;
  bad.frames[0].region = 1;
  Check(!twod::ValidateClip(bad), "ambiguous source Clip region admitted");
  bad = clip;
  bad.frames[0].event.assign(257, 'a');
  Check(!twod::ValidateClip(bad), "oversized source event admitted");
  runtime::ImporterRegistry registry;
  Check(twod::RegisterImporters(registry), "register importer");
  const auto canonical = registry.Import({clipId, ".nx2clip", "clip.nx2clip", bytes});
  Check(canonical && canonical->dependencies == std::vector{firstId, secondId} &&
            canonical->payload == bytes,
        "importer lost sorted source edges");
  twod::Animator animator;
  Check(animator.Play(Clip(), 2) && animator.SourceId() == firstId, "source animation start");
  animator.Pause(true);
  const auto paused = animator.Advance(1);
  Check(paused && paused->source == firstId && paused->events.empty(), "paused source clock");
  animator.Pause(false);
  const auto advanced = animator.Advance(0.125);
  Check(advanced && advanced->source == secondId && advanced->region == 0 &&
            advanced->events == std::vector<std::string>{"first"},
        "source speed/event boundary");
  auto invalid = Clip();
  invalid.frames[0].source.reset();
  Check(!animator.Play(invalid) && animator.SourceId() == secondId, "failed switch mutated clock");
  auto oneShot = Clip();
  oneShot.loop = false;
  Check(animator.Play(oneShot), "one-shot start");
  const auto ended = animator.Advance(1);
  Check(ended && ended->finished && ended->source == secondId && ended->events.size() == 1,
        "source one-shot finish");
}
void Playback() {
  twod::GameplayContent content;
  NexoraGameplayContentApiV1 registration{};
  registration.struct_size = sizeof(registration);
  Check(content.GetApi(&registration) == 0, "content API");
  NexoraGameplaySpriteContentApiV1 sourceRegistration{};
  sourceRegistration.struct_size = sizeof(sourceRegistration);
  Check(content.GetSpriteApi(&sourceRegistration) == 0, "source content API");
  auto clip = Clip();
  const auto bytes = twod::EncodeClip(clip);
  uint64_t high = 91, low = 92;
  Check(registration.register_clip(registration.context, bytes.data(), bytes.size(), &high, &low) ==
                -2 &&
            high == 91 && low == 92 && !content.ResolveClip(clipId),
        "missing source closure published Clip/output");
  twod::Image image{901, 2, 2, {}};
  image.rgba.assign(16, std::byte{255});
  auto atlas = twod::PackAtlas(atlasId, std::span(&image, 1), 8, 8, true);
  Check(bool(atlas), "sparse Atlas fixture");
  const auto atlasBytes = twod::EncodeAtlas(*atlas);
  Check(registration.register_atlas(registration.context, atlasBytes.data(), atlasBytes.size(),
                                    &high, &low) == 0,
        "register Atlas");
  twod::SpriteAsset source;
  source.id = firstId;
  source.atlas = atlasId;
  source.region = 901;
  source.sourceWidth = source.sourceHeight = 6;
  source.trimX = source.trimY = 1;
  source.trimWidth = source.trimHeight = 2;
  source.pivotX = 3;
  source.pivotY = source.pixelsPerUnit = 1;
  for (const auto id : {firstId, secondId}) {
    source.id = id;
    const auto payload = twod::EncodeSpriteAsset(source);
    Check(sourceRegistration.register_sprite_source(sourceRegistration.context, payload.data(),
                                                    payload.size(), &high, &low) == 0,
          "register source");
  }
  auto wrongType = Clip();
  wrongType.frames[0].source = atlasId;
  const auto wrongBytes = twod::EncodeClip(wrongType);
  const auto previousHigh = high, previousLow = low;
  Check(registration.register_clip(registration.context, wrongBytes.data(), wrongBytes.size(),
                                   &high, &low) == -2 &&
            high == previousHigh && low == previousLow && !content.ResolveClip(clipId),
        "wrong-typed source dependency mutated Clip registration");
  CatalogClip(content, bytes);
  runtime::World world(runtime::WorldKind::Play);
  const auto sceneId = world.LoadScene("Animation");
  const auto entity = world.CreateEntity(sceneId).id;
  twod::SpriteScene scene(world, sceneId);
  twod::GameplayBridge bridge(world, scene, [&](auto id) { return content.ResolveClip(id); });
  NexoraTwoDApiV1 api{};
  api.struct_size = sizeof(api);
  Check(bridge.GetApi(&api) == 0, "2D API");
  NexoraTwoDSourceApiV1 sources{};
  sources.struct_size = sizeof(sources);
  Check(bridge.GetSourceApi(&sources) == 0, "source API");
  NexoraTwoDSourceSpriteV1 sprite{};
  sprite.sprite = {0, 0, 0, 13, {1, 1}, {0.5F, 0.5F}, {1, 1, 1, 1}, 0, 7};
  sprite.source_high = firstId.high;
  sprite.source_low = firstId.low;
  sprite.sprite.flags = 13;
  sprite.sprite.order = 7;
  Check(sources.set_sprite(sources.context, entity, &sprite) == 0 &&
            api.play_animation(api.context, entity, clipId.high, clipId.low, 1) == 0,
        "start source Clip through unchanged C ABI");
  auto original = content.ResolveClip(clipId);
  NexoraTwoDAnimationResult result{};
  std::array<NexoraTwoDEvent, 2> events{};
  Check(api.advance_animation(api.context, entity, 0.25, nullptr, 0, &result) == 0 &&
            result.region == 0 && result.event_count == 1 && scene.Find(entity)->source == firstId,
        "count-only probe consumed source clock");
  Check(api.advance_animation(api.context, entity, 0.25, events.data(), 0, &result) == -2 &&
            scene.Find(entity)->source == firstId,
        "short event buffer consumed source clock");
  Check(api.advance_animation(api.context, entity, 0.125, events.data(), events.size(), &result) ==
                0 &&
            result.event_count == 0,
        "partial source clock");
  sprite.sprite.order = 123;
  Check(sources.set_sprite(sources.context, entity, &sprite) == 0 &&
            api.advance_animation(api.context, entity, 0.125, events.data(), events.size(),
                                  &result) == 0 &&
            scene.Find(entity)->source == secondId && scene.Find(entity)->order == 123 &&
            scene.Find(entity)->flipX && scene.Find(entity)->sortY && result.event_count == 1 &&
            events[0].length == 5,
        "style mutation reset clock or animation lost style");
  Check(content.Remove(firstId) == -3 && content.Remove(secondId) == -3,
        "active source Clip allowed removing dependency");
  clip.frames = {{0, 1, "replacement", firstId}};
  const auto replacement = twod::EncodeClip(clip);
  Check(registration.register_clip(registration.context, replacement.data(), replacement.size(),
                                   &high, &low) == 0 &&
            content.ResolveClip(clipId) != original && content.Remove(secondId) == -3,
        "replacement lost retired Clip dependency guard");
  original.reset();
  Check(content.Remove(clipId) == 0 && content.Remove(secondId) == -3,
        "live playback lost exact retired owner");
  Check(api.advance_animation(api.context, entity, 0.5, events.data(), events.size(), &result) ==
                0 &&
            scene.Find(entity)->source == firstId,
        "replacement changed an existing playback's frames");
  bridge.ForgetEntity(entity);
  Check(content.Remove(secondId) == 0 && content.Remove(firstId) == 0 &&
            content.Remove(atlasId) == 0,
        "released playback left dependency ownership stuck");
}
} // namespace
int main() {
  try {
    Wire();
    Playback();
    std::cout << "PASS source Clip wire, closure, transactional clocks and retired ownership\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
