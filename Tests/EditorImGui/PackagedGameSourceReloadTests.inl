namespace source_reload {
using Owner = editor::preview::ReloadablePlayGameplayModule;
constexpr runtime::AssetUuid nextId{103, 1}, clipId{105, 1};
enum class Fault { None, Create, Restore, Start, CaptureWrite, CachedOldSource };
Fault fault{};
unsigned mutations{}, retired_callbacks{};
struct Payload final {
  std::uint64_t version{1}, counter{}, scene{}, entity{};
  NexoraTwoDInstanceHandle pool{};
};
struct State final {
  NexoraGameplayHostV3 host{};
  NexoraTwoDApiV1 api{};
  NexoraTwoDSourceApiV1 source{};
  NexoraGameplaySceneApiV1 scenes{};
  Payload payload;
};
State *latest{};
NexoraTwoDSourceApiV1 cached_source{};
runtime::GameProjectPackage AnimatedPackage() {
  auto package = Package();
  auto source = twod::DecodeSpriteAsset(package.assets.back().payload);
  Check(bool(source), "animated fixture source");
  source->id = nextId;
  source->pivotX = 1;
  package.assets.push_back(
      testscene::Cook(nextId, "sprite_source", twod::EncodeSpriteAsset(*source), {atlasId}));
  const twod::Clip clip{clipId, {}, true, {{0, .25, "first", sourceId}, {0, .25, "next", nextId}}};
  package.assets.push_back(
      testscene::Cook(clipId, "sprite_clip", twod::EncodeClip(clip), twod::ClipDependencies(clip)));
  package.animation_clips = {clipId};
  return package;
}
int32_t Write(State &state, double x) {
  const game::GameplayTransformWire value{x, 0, 0};
  return state.host.write_component(state.host.context, state.payload.entity,
                                    game::TransformComponentType(), &value, sizeof(value));
}
void Free(State *state) {
  const auto host = state->host;
  if (latest == state)
    latest = nullptr;
  state->~State();
  host.deallocate(host.context, 17, state, sizeof(State), alignof(State));
}
int32_t Change(State &state, float tint, bool extra_pool = false) {
  NexoraTwoDSourceSpriteV1 sprite{};
  if (state.source.get_sprite(state.source.context, state.payload.entity, &sprite))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  sprite.sprite.color[0] = tint;
  if (state.source.set_sprite(state.source.context, state.payload.entity, &sprite) ||
      Write(state, double(state.payload.counter)))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  NexoraTwoDSourceInstanceV1 value{};
  if (extra_pool) {
    value.transform.rotation.w = 1;
    value.transform.scale = {1, 1, 1};
    value.sprite = sprite;
    NexoraTwoDInstanceHandle ignored{};
    if (state.source.create_instances(state.source.context, &value, 1, &ignored))
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  } else {
    if (state.source.read_instances(state.source.context, &state.payload.pool, 1, &value))
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    value.sprite.source_high = nextId.high;
    value.sprite.source_low = nextId.low;
    value.sprite.sprite.color[0] = tint;
    if (state.source.write_instances(state.source.context, &state.payload.pool, 1, &value))
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  }
  ++mutations;
  return 0;
}
int32_t Create(void **output, const NexoraGameplayHostV3 *host) {
  auto *memory = host->allocate(host->context, 17, sizeof(State), alignof(State));
  if (!memory)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  auto *state = new (memory) State;
  state->host = *host;
  state->api.struct_size = sizeof(state->api);
  state->source.struct_size = sizeof(state->source);
  state->scenes.struct_size = sizeof(state->scenes);
  state->scenes.abi_version = 1;
  NexoraGameplayServicesApiV1 services{};
  services.struct_size = sizeof(services);
  services.abi_version = 1;
  if (!host->get_twod_api || host->get_twod_api(host->context, &state->api) ||
      NexoraGetTwoDSourcesV1(&state->api, &state->source) ||
      NexoraGetGameplayServicesV1(host, &services) ||
      services.get_scene_api(services.context, &state->scenes) ||
      state->scenes.get_active_scene(state->scenes.context, &state->payload.scene)) {
    Free(state);
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  }
  for (std::uint64_t id = 1; id < 128 && !state->payload.entity; ++id) {
    NexoraTwoDSourceSpriteV1 value{};
    if (state->source.get_sprite(state->source.context, id, &value) == 0)
      state->payload.entity = id;
  }
  if (!state->payload.entity ||
      ((host->capabilities & NEXORA_GAMEPLAY_CAPABILITY_RELOAD_CANDIDATE) &&
       fault == Fault::Create)) {
    const bool changed = state->payload.entity && Change(*state, .875F, true) == 0;
    Free(state);
    return changed ? NEXORA_GAMEPLAY_ERROR_UNSUPPORTED : NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  }
  latest = state;
  *output = state;
  return 0;
}
int32_t Start(void *context) {
  auto &state = *static_cast<State *>(context);
  if (state.host.capabilities & NEXORA_GAMEPLAY_CAPABILITY_RELOAD_CANDIDATE) {
    if (fault == Fault::CachedOldSource) {
      NexoraTwoDSourceSpriteV1 value{};
      if (cached_source.get_sprite(cached_source.context, state.payload.entity, &value))
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      // Ignoring the forbidden old-generation write must still invalidate final admission.
      static_cast<void>(
          cached_source.set_sprite(cached_source.context, state.payload.entity, &value));
    }
    return fault == Fault::Start ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : 0;
  }
  NexoraTwoDSourceSpriteV1 sprite{};
  if (state.source.get_sprite(state.source.context, state.payload.entity, &sprite))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  sprite.sprite.color[0] = .5F;
  sprite.sprite.flags |= 1U | 4U;
  if (state.source.set_sprite(state.source.context, state.payload.entity, &sprite))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  NexoraTwoDSourceInstanceV1 value{};
  value.transform.rotation.w = 1;
  value.transform.scale = {-1, 1, 1};
  value.sprite = sprite;
  return state.source.create_instances(state.source.context, &value, 1, &state.payload.pool);
}
int32_t Update(void *context, double) {
  auto &state = *static_cast<State *>(context);
  NexoraTwoDAnimationResult result{};
  NexoraTwoDEvent events[4]{};
  return state.api.play_animation(state.api.context, state.payload.entity, clipId.high, clipId.low,
                                  1) ||
                 state.api.advance_animation(state.api.context, state.payload.entity, .125, events,
                                             4, &result)
             ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE
             : 0;
}
std::uint32_t Save(void *context, void *output, std::uint32_t capacity) {
  auto &state = *static_cast<State *>(context);
  cached_source = state.source;
  if (fault == Fault::CaptureWrite) {
    NexoraTwoDSourceSpriteV1 value{};
    if (state.source.get_sprite(state.source.context, state.payload.entity, &value) == 0)
      static_cast<void>(
          state.source.set_sprite(state.source.context, state.payload.entity, &value));
  }
  if (!output)
    return sizeof(Payload);
  if (capacity < sizeof(Payload))
    return 0;
  std::memcpy(output, &state.payload, sizeof(Payload));
  return sizeof(Payload);
}
int32_t Restore(void *context, const void *data, std::uint32_t size) {
  auto &state = *static_cast<State *>(context);
  if (size != sizeof(Payload))
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  std::memcpy(&state.payload, data, sizeof(Payload));
  if (state.payload.version != 1)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  ++state.payload.counter;
  NexoraTwoDEvent events[4]{};
  NexoraTwoDAnimationResult result{};
  NexoraTwoDSourceSpriteV1 sprite{};
  if (state.api.advance_animation(state.api.context, state.payload.entity, .125, events, 4,
                                  &result) ||
      state.source.get_sprite(state.source.context, state.payload.entity, &sprite) ||
      sprite.source_high != nextId.high || Change(state, float(state.payload.counter) / 64) != 0)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  return fault == Fault::Restore ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : 0;
}
void Stop(void *context) {
  auto &state = *static_cast<State *>(context);
  // These are real cached API writes in candidate/retired callbacks, never direct World edits.
  static_cast<void>(Write(state, 99999));
  static_cast<void>(Change(state, .9375F));
  ++retired_callbacks;
}
void Destroy(void *context) {
  Stop(context);
  Free(static_cast<State *>(context));
}
int32_t Loader(std::uint32_t version, NexoraGameModuleV3 *module) {
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = version;
  module->capabilities =
      NEXORA_GAMEPLAY_CAPABILITY_STATE_MIGRATION | NEXORA_GAMEPLAY_CAPABILITY_RELOAD_LIFECYCLE;
  module->create = Create;
  module->on_start = Start;
  module->update = Update;
  module->save_state = Save;
  module->load_state = Restore;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return 0;
}
void Run() {
  editor::preview::PackagedPlayContext context(Load(AnimatedPackage()));
  const auto draw = [](const auto &, const twod::SpriteFrame &) { return SurfaceStatus::Ready; };
  Check(context.RenderComposed({0, 0, 640, 480}, false, draw).committed && context.Sprites(),
        "main source startup not committed");
  std::uint64_t request{};
  Check(context.Scenes().RequestLoad(testscene::kTitle, runtime::GameSceneLoadMode::Additive, false,
                                     request) == runtime::GameSceneError::None &&
            context.RenderComposed({0, 0, 640, 480}, false, draw).committed,
        "main source additive startup");
  auto &world = context.World().InternalWorld();
  editor::preview::PlayTwoDBinding binding;
  binding.game = context.Sprites();
  Owner owner;
  Check(owner.Load(world, Loader, {}, binding, context.Scenes().Api(), context.ReloadHooks()) &&
            latest && latest->payload.pool.owner && owner.Update(0),
        "main packaged source module negotiation/start");
  auto *live = latest;
  const auto entity = live->payload.entity;
  const auto pool = live->payload.pool;
  const auto primary = live->payload.scene;
  unsigned rejected{};
  for (unsigned generation = 1; generation <= 32; ++generation) {
    Check(owner.Update(0), "main source old dispatch unavailable");
    const auto before = world.SaveScene(primary);
    const auto source_owner = context.Sprites();
    const auto frame = source_owner->Extract(640, 480);
    Check(frame && frame->statistics.input == 3, "main source frame scope");
    NexoraTwoDSourceInstanceV1 retained{};
    Check(live->source.read_instances(live->source.context, &pool, 1, &retained) == 0,
          "main source retained pool");
    const auto revision = owner.Generation();
    for (const auto failure : {Fault::Create, Fault::Restore, Fault::Start, Fault::CaptureWrite,
                               Fault::CachedOldSource}) {
      fault = failure;
      const auto changes = mutations;
      Check(!owner.Reload(Loader) && owner.IsLoaded() && owner.Generation() == revision &&
                context.Sprites() == source_owner && world.SaveScene(primary) == before &&
                context.Sprites()->Pool().Size() == 1,
            "rejected main Game reload published World/owner/pool state");
      NexoraTwoDSourceSpriteV1 sprite{};
      NexoraTwoDSourceInstanceV1 value{};
      Check(live->source.get_sprite(live->source.context, entity, &sprite) == 0 &&
                sprite.source_high == sourceId.high &&
                live->source.read_instances(live->source.context, &pool, 1, &value) == 0 &&
                value.sprite.source_high == retained.sprite.source_high &&
                value.sprite.sprite.color[0] == retained.sprite.sprite.color[0] &&
                (failure == Fault::CaptureWrite || mutations > changes),
            "candidate/retired callbacks changed the published source/pool");
      ++rejected;
    }
    fault = Fault::None;
    Check(owner.Reload(Loader) && owner.Generation() == revision + 1 && latest &&
              latest->payload.counter == generation && latest->payload.entity == entity &&
              latest->payload.pool.owner == pool.owner &&
              latest->payload.pool.index == pool.index &&
              latest->payload.pool.generation == pool.generation &&
              context.Sprites() != source_owner && &context.Sprites()->WorldOwner() == &world,
          "accepted main Game reload lost identity or retained the old owner");
    live = latest;
    NexoraTwoDSourceSpriteV1 sprite{};
    NexoraTwoDSourceInstanceV1 value{};
    game::GameplayTransformWire transform{};
    Check(live->source.get_sprite(live->source.context, entity, &sprite) == 0 &&
              sprite.source_high == nextId.high &&
              sprite.sprite.color[0] == float(generation) / 64 &&
              live->source.read_instances(live->source.context, &pool, 1, &value) == 0 &&
              value.sprite.sprite.color[0] == sprite.sprite.color[0] &&
              live->host.read_component(live->host.context, entity, game::TransformComponentType(),
                                        &transform, sizeof(transform)) == 0 &&
              transform.x == generation &&
              context.RenderComposed({0, 0, 640, 480}, false, draw).drawn,
          "retired callbacks corrupted committed source, clock, pool, World or compositor");
  }
  Check(rejected == 160 && retired_callbacks >= 64, "main source reload paths not exercised");
  // Host despawn must invalidate cached queries before EndFrame/Reconcile can run.
  const NexoraTwoDCollider collider{0, {0, 0}, {.5F, .5F}, .5F, 1, UINT32_MAX};
  const float x = static_cast<float>(live->payload.counter);
  const NexoraTwoDShape query{0, {0, {x, 0}, {.5F, .5F}, .5F, 1, UINT32_MAX}};
  const NexoraTwoDSweep sweep{{x - 2, 0}, {x + 2, 0}, 0, 1, UINT32_MAX};
  std::uint64_t hits[2]{};
  NexoraTwoDHit swept[2]{};
  std::uint32_t count{};
  Check(live->api.set_collider(live->api.context, entity, &collider) == 0 &&
            live->api.rebuild_queries(live->api.context, 0) == 0 &&
            live->api.overlap(live->api.context, 0, &query, hits, 2, &count) == 0 && count == 1 &&
            hits[0] == entity &&
            live->api.sweep(live->api.context, 0, &sweep, swept, 2, &count) == 0 && count == 1 &&
            swept[0].id == entity,
        "main source despawn query fixture");
  Check(live->host.despawn_entity(live->host.context, entity) == 0, "main source host despawn");
  count = 777;
  Check(live->api.overlap(live->api.context, 0, &query, hits, 2, &count) ==
                NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
            count == 777 &&
            live->api.sweep(live->api.context, 0, &sweep, swept, 2, &count) ==
                NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
            count == 777,
        "main source host despawn retained a stale query snapshot");
  NexoraTwoDSourceInstanceV1 retained_pool{};
  Check(live->api.rebuild_queries(live->api.context, 0) == 0 &&
            live->api.overlap(live->api.context, 0, &query, hits, 2, &count) == 0 && count == 0 &&
            live->api.sweep(live->api.context, 0, &sweep, swept, 2, &count) == 0 && count == 0 &&
            context.Sprites()->Pool().Size() == 1 &&
            live->source.read_instances(live->source.context, &pool, 1, &retained_pool) == 0 &&
            retained_pool.sprite.source_high == nextId.high &&
            retained_pool.sprite.sprite.color[0] == float(live->payload.counter) / 64 &&
            context.RenderComposed({0, 0, 640, 480}, false, draw).drawn,
        "main source despawn retained query hits or damaged the live pool/compositor");
  owner.Unload();
  Check(!latest && owner.LastTeardownAllocations().live_allocations == 0,
        "main source module teardown retained allocation state");
}
} // namespace source_reload
