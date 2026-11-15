#pragma once
#include "Nexora/TwoD/GameplayBridge.h"
#include "PlayApiProxy.h"
#include <cstring>
#include <stdexcept>

namespace nexora::editor::preview {
// Stable generation-owned parent/source tables for either a custom scene or main Game router.
// The gate and bridge outlive cached callbacks through module Stop/Destroy/library finalizers.
class PlayTwoDApiProxy final {
public:
  PlayTwoDApiProxy(PlayServiceGate &gate, twod::GameplayBridge &bridge)
      : gate_(gate), bridge_(bridge), sources_(gate, SourceTable(bridge)),
        parent_(gate, ParentTable(bridge), *this) {
#define NX_TWOD(field, writes)                                                                     \
  parent_.Bind<&NexoraTwoDApiV1::field,                                                            \
               offsetof(NexoraTwoDApiV1, field) + sizeof(NexoraTwoDApiV1::field), writes>()
    NX_TWOD(set_sprite, true);
    NX_TWOD(get_sprite, false);
    NX_TWOD(set_collider, true);
    NX_TWOD(get_collider, false);
    NX_TWOD(set_camera, true);
    NX_TWOD(get_camera, false);
    NX_TWOD(play_animation, true);
    NX_TWOD(pause_animation, true);
    NX_TWOD(advance_animation, true);
    NX_TWOD(rebuild_queries, true);
    NX_TWOD(overlap, false);
    NX_TWOD(sweep, false);
    NX_TWOD(paint_tiles, true);
    NX_TWOD(set_chunk_loaded, true);
    NX_TWOD(get_tile, false);
    NX_TWOD(create_instances, true);
    NX_TWOD(destroy_instances, true);
    NX_TWOD(read_instances, false);
    NX_TWOD(write_instances, true);
#undef NX_TWOD
#define NX_SOURCE(field, writes)                                                                   \
  sources_.Bind<&NexoraTwoDSourceApiV1::field,                                                     \
                offsetof(NexoraTwoDSourceApiV1, field) + sizeof(NexoraTwoDSourceApiV1::field),     \
                writes>()
    NX_SOURCE(set_sprite, true);
    NX_SOURCE(get_sprite, false);
    NX_SOURCE(create_instances, true);
    NX_SOURCE(read_instances, false);
    NX_SOURCE(write_instances, true);
#undef NX_SOURCE
  }
  int32_t GetApi(NexoraTwoDApiV1 *output) noexcept {
    if (!output)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    NexoraTwoDApiV1 admitted{};
    admitted.struct_size = output->struct_size;
    const auto result = bridge_.GetApi(&admitted);
    if (result == NEXORA_GAMEPLAY_OK) {
      auto api = parent_.Get();
      api.struct_size = admitted.struct_size;
      std::memcpy(output, &api, admitted.struct_size);
    }
    return result;
  }

private:
  struct Parent final : PlayApiProxy<NexoraTwoDApiV1> {
    using Base = PlayApiProxy<NexoraTwoDApiV1>;
    Parent(PlayServiceGate &gate, NexoraTwoDApiV1 provider, PlayTwoDApiProxy &owner)
        : Base(gate, provider), owner_(owner) {}
    [[nodiscard]] NexoraTwoDApiV1 Get() const noexcept {
      auto api = Base::Get();
      api.get_source_api = [](void *context, NexoraTwoDSourceApiV1 *output) noexcept -> int32_t {
        if (!context)
          return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
        auto *base = static_cast<Base *>(context);
        return static_cast<Parent *>(base)->owner_.GetSourceApi(output);
      };
      return api;
    }
    PlayTwoDApiProxy &owner_;
  };
  static NexoraTwoDApiV1 ParentTable(twod::GameplayBridge &bridge) {
    NexoraTwoDApiV1 value{};
    value.struct_size = sizeof(value);
    if (bridge.GetApi(&value) != NEXORA_GAMEPLAY_OK)
      throw std::runtime_error("Play 2D proxy unavailable");
    return value;
  }
  static NexoraTwoDSourceApiV1 SourceTable(twod::GameplayBridge &bridge) {
    NexoraTwoDSourceApiV1 value{};
    value.struct_size = sizeof(value);
    if (bridge.GetSourceApi(&value) != NEXORA_GAMEPLAY_OK)
      throw std::runtime_error("Play source proxy unavailable");
    return value;
  }
  int32_t GetSourceApi(NexoraTwoDSourceApiV1 *output) noexcept {
    if (!gate_.Check(false, false))
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    if (!output)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    NexoraTwoDSourceApiV1 admitted{};
    admitted.struct_size = output->struct_size;
    const auto result = bridge_.GetSourceApi(&admitted);
    if (result == NEXORA_GAMEPLAY_OK) {
      auto api = sources_.Get();
      api.struct_size = admitted.struct_size;
      std::memcpy(output, &api, admitted.struct_size);
    }
    return result;
  }
  PlayServiceGate &gate_;
  twod::GameplayBridge &bridge_;
  PlayApiProxy<NexoraTwoDSourceApiV1> sources_;
  Parent parent_;
};
} // namespace nexora::editor::preview
