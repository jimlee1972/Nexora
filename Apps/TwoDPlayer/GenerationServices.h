#pragma once

#include "Nexora/Runtime/GameplayServices.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <utility>

namespace nexora::player::detail {
enum class GenerationPhase { Active, Capture, Candidate, Retired };
struct GenerationServiceStats final {
  std::size_t capture_rejections{}, candidate_rejections{}, retired_rejections{};
};
struct GenerationAdmission final {
  GenerationPhase phase;
  GenerationServiceStats &stats;
  const std::thread::id thread{std::this_thread::get_id()};
};
template <class Table> class ServiceGate;
template <class Table, auto Member, class Signature> struct ServiceCall;
template <class Table, auto Member, class... Args>
struct ServiceCall<Table, Member, std::int32_t (*)(void *, Args...)> final {
  static std::int32_t Invoke(void *context, Args... args) noexcept {
    if (!context)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    auto &gate = *static_cast<ServiceGate<Table> *>(context);
    if (std::this_thread::get_id() != gate.admission_.thread)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    if (gate.admission_.phase == GenerationPhase::Capture ||
        (!gate.isolated_ && gate.admission_.phase != GenerationPhase::Active)) {
      auto &stats = gate.admission_.stats;
      if (gate.admission_.phase == GenerationPhase::Capture)
        ++stats.capture_rejections;
      else if (gate.admission_.phase == GenerationPhase::Candidate)
        ++stats.candidate_rejections;
      else
        ++stats.retired_rejections;
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    }
    try {
      return (gate.source_.*Member)(gate.source_.context, args...);
    } catch (...) {
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    }
  }
};
// Each cached table keeps its generation's gate context from initial Load through finalizers.
// Build a fresh zero table: an added ABI member never escapes unguarded by default.
template <class Table> class ServiceGate final {
public:
  explicit ServiceGate(GenerationAdmission &admission, bool isolated = false, void *owner = nullptr)
      : admission_(admission), isolated_(isolated), owner_(owner) {}
  void *Owner() const noexcept { return owner_; }
  Table Begin(const Table &source) noexcept {
    source_ = source;
    Table result{};
    result.struct_size = std::min<std::uint32_t>(source.struct_size, sizeof(Table));
    result.abi_version = source.abi_version;
    result.context = this;
    return result;
  }
  template <auto Member> auto Function(std::size_t offset) noexcept {
    using Function = std::remove_cvref_t<decltype(std::declval<Table>().*Member)>;
    return source_.struct_size >= offset + sizeof(Function) && source_.*Member
               ? static_cast<Function>(&ServiceCall<Table, Member, Function>::Invoke)
               : Function{};
  }

private:
  template <class, auto, class> friend struct ServiceCall;
  GenerationAdmission &admission_;
  bool isolated_;
  void *owner_;
  Table source_{};
};

#define NEXORA_PLAYER_SERVICE_BEGIN(Type)                                                          \
  inline Type GuardService(ServiceGate<Type> &gate, const Type &source) noexcept {                 \
    using Table = Type;                                                                            \
    auto result = gate.Begin(source);
#define NEXORA_PLAYER_SERVICE_FIELD(Member)                                                        \
  result.Member = gate.Function<&Table::Member>(offsetof(Table, Member));
#define NEXORA_PLAYER_SERVICE_END                                                                  \
  return result;                                                                                   \
  }

NEXORA_PLAYER_SERVICE_BEGIN(NexoraGameplayInputApiV1)
NEXORA_PLAYER_SERVICE_FIELD(read_action)
NEXORA_PLAYER_SERVICE_FIELD(bind)
NEXORA_PLAYER_SERVICE_FIELD(clear_bindings)
NEXORA_PLAYER_SERVICE_FIELD(read_device)
NEXORA_PLAYER_SERVICE_FIELD(read_pointer)
NEXORA_PLAYER_SERVICE_END
NEXORA_PLAYER_SERVICE_BEGIN(NexoraGameplayUiApiV1)
NEXORA_PLAYER_SERVICE_FIELD(begin_frame)
NEXORA_PLAYER_SERVICE_FIELD(panel)
NEXORA_PLAYER_SERVICE_FIELD(text)
NEXORA_PLAYER_SERVICE_FIELD(button)
NEXORA_PLAYER_SERVICE_FIELD(end_frame)
NEXORA_PLAYER_SERVICE_FIELD(get_font)
NEXORA_PLAYER_SERVICE_FIELD(has_glyph)
NEXORA_PLAYER_SERVICE_FIELD(register_font)
NEXORA_PLAYER_SERVICE_END
NEXORA_PLAYER_SERVICE_BEGIN(NexoraGameplayAudioApiV1)
NEXORA_PLAYER_SERVICE_FIELD(play)
NEXORA_PLAYER_SERVICE_FIELD(stop)
NEXORA_PLAYER_SERVICE_FIELD(set_paused)
NEXORA_PLAYER_SERVICE_FIELD(set_gain)
NEXORA_PLAYER_SERVICE_FIELD(set_bus_gain)
NEXORA_PLAYER_SERVICE_FIELD(is_playing)
NEXORA_PLAYER_SERVICE_FIELD(output_available)
NEXORA_PLAYER_SERVICE_FIELD(stop_all)
NEXORA_PLAYER_SERVICE_FIELD(register_wave)
NEXORA_PLAYER_SERVICE_FIELD(remove_asset)
NEXORA_PLAYER_SERVICE_END
NEXORA_PLAYER_SERVICE_BEGIN(NexoraStorageApiV1)
NEXORA_PLAYER_SERVICE_FIELD(save)
NEXORA_PLAYER_SERVICE_FIELD(load)
NEXORA_PLAYER_SERVICE_FIELD(remove)
NEXORA_PLAYER_SERVICE_FIELD(exists)
NEXORA_PLAYER_SERVICE_END
NEXORA_PLAYER_SERVICE_BEGIN(NexoraGameplayContentApiV1)
NEXORA_PLAYER_SERVICE_FIELD(register_atlas)
NEXORA_PLAYER_SERVICE_FIELD(register_clip)
NEXORA_PLAYER_SERVICE_FIELD(list_assets)
NEXORA_PLAYER_SERVICE_FIELD(describe_asset)
NEXORA_PLAYER_SERVICE_END
NEXORA_PLAYER_SERVICE_BEGIN(NexoraGameplayDisplayApiV1)
NEXORA_PLAYER_SERVICE_FIELD(get_state)
NEXORA_PLAYER_SERVICE_FIELD(request_window)
NEXORA_PLAYER_SERVICE_FIELD(set_aspect)
NEXORA_PLAYER_SERVICE_END
NEXORA_PLAYER_SERVICE_BEGIN(NexoraGameplayUiDrawApiV1)
NEXORA_PLAYER_SERVICE_FIELD(register_image)
NEXORA_PLAYER_SERVICE_FIELD(remove_image)
NEXORA_PLAYER_SERVICE_FIELD(image)
NEXORA_PLAYER_SERVICE_FIELD(nine_slice)
NEXORA_PLAYER_SERVICE_FIELD(interact)
NEXORA_PLAYER_SERVICE_FIELD(get_glyph)
NEXORA_PLAYER_SERVICE_FIELD(register_font)
NEXORA_PLAYER_SERVICE_FIELD(remove_font)
NEXORA_PLAYER_SERVICE_END
NEXORA_PLAYER_SERVICE_BEGIN(NexoraGameplayAssetsApiV1)
NEXORA_PLAYER_SERVICE_FIELD(describe_catalog)
NEXORA_PLAYER_SERVICE_FIELD(request)
NEXORA_PLAYER_SERVICE_FIELD(poll)
NEXORA_PLAYER_SERVICE_FIELD(release_request)
NEXORA_PLAYER_SERVICE_FIELD(remove_asset)
NEXORA_PLAYER_SERVICE_FIELD(get_usage)
NEXORA_PLAYER_SERVICE_FIELD(copy_bytes)
NEXORA_PLAYER_SERVICE_END
NEXORA_PLAYER_SERVICE_BEGIN(NexoraGameplayAudioStateApiV1)
NEXORA_PLAYER_SERVICE_FIELD(get_usage)
NEXORA_PLAYER_SERVICE_FIELD(play_voice)
NEXORA_PLAYER_SERVICE_FIELD(set_voice_bus_gain)
NEXORA_PLAYER_SERVICE_END
NEXORA_PLAYER_SERVICE_BEGIN(NexoraGameplayLifecycleApiV1)
NEXORA_PLAYER_SERVICE_FIELD(get_state)
NEXORA_PLAYER_SERVICE_FIELD(set_close_guard)
NEXORA_PLAYER_SERVICE_FIELD(resolve_close)
NEXORA_PLAYER_SERVICE_FIELD(rumble)
NEXORA_PLAYER_SERVICE_END
NEXORA_PLAYER_SERVICE_BEGIN(NexoraGameplayTwoDEffectsApiV1)
NEXORA_PLAYER_SERVICE_FIELD(get_capabilities)
NEXORA_PLAYER_SERVICE_FIELD(get_state)
NEXORA_PLAYER_SERVICE_FIELD(submit_frame)
NEXORA_PLAYER_SERVICE_FIELD(pool_identity)
NEXORA_PLAYER_SERVICE_FIELD(get_diagnostics)
NEXORA_PLAYER_SERVICE_FIELD(set_debug_view)
NEXORA_PLAYER_SERVICE_END
NEXORA_PLAYER_SERVICE_BEGIN(NexoraGameplaySpriteContentApiV1)
NEXORA_PLAYER_SERVICE_FIELD(register_sprite_source)
NEXORA_PLAYER_SERVICE_END
NEXORA_PLAYER_SERVICE_BEGIN(NexoraGameplaySceneApiV1)
NEXORA_PLAYER_SERVICE_FIELD(resolve_scene)
NEXORA_PLAYER_SERVICE_FIELD(request_load)
NEXORA_PLAYER_SERVICE_FIELD(request_unload)
NEXORA_PLAYER_SERVICE_FIELD(get_status)
NEXORA_PLAYER_SERVICE_FIELD(get_active_scene)
NEXORA_PLAYER_SERVICE_FIELD(get_scene_info)
NEXORA_PLAYER_SERVICE_END
NEXORA_PLAYER_SERVICE_BEGIN(NexoraTwoDApiV1)
NEXORA_PLAYER_SERVICE_FIELD(set_sprite)
NEXORA_PLAYER_SERVICE_FIELD(get_sprite)
NEXORA_PLAYER_SERVICE_FIELD(set_collider)
NEXORA_PLAYER_SERVICE_FIELD(get_collider)
NEXORA_PLAYER_SERVICE_FIELD(set_camera)
NEXORA_PLAYER_SERVICE_FIELD(get_camera)
NEXORA_PLAYER_SERVICE_FIELD(play_animation)
NEXORA_PLAYER_SERVICE_FIELD(pause_animation)
NEXORA_PLAYER_SERVICE_FIELD(advance_animation)
NEXORA_PLAYER_SERVICE_FIELD(rebuild_queries)
NEXORA_PLAYER_SERVICE_FIELD(overlap)
NEXORA_PLAYER_SERVICE_FIELD(sweep)
NEXORA_PLAYER_SERVICE_FIELD(paint_tiles)
NEXORA_PLAYER_SERVICE_FIELD(set_chunk_loaded)
NEXORA_PLAYER_SERVICE_FIELD(get_tile)
NEXORA_PLAYER_SERVICE_FIELD(create_instances)
NEXORA_PLAYER_SERVICE_FIELD(destroy_instances)
NEXORA_PLAYER_SERVICE_FIELD(read_instances)
NEXORA_PLAYER_SERVICE_FIELD(write_instances)
// get_source_api is installed by GenerationHost with its independently guarded nested context.
NEXORA_PLAYER_SERVICE_END
NEXORA_PLAYER_SERVICE_BEGIN(NexoraTwoDSourceApiV1)
NEXORA_PLAYER_SERVICE_FIELD(set_sprite)
NEXORA_PLAYER_SERVICE_FIELD(get_sprite)
NEXORA_PLAYER_SERVICE_FIELD(create_instances)
NEXORA_PLAYER_SERVICE_FIELD(read_instances)
NEXORA_PLAYER_SERVICE_FIELD(write_instances)
NEXORA_PLAYER_SERVICE_END
#undef NEXORA_PLAYER_SERVICE_BEGIN
#undef NEXORA_PLAYER_SERVICE_FIELD
#undef NEXORA_PLAYER_SERVICE_END

class GenerationServices final {
public:
  explicit GenerationServices(GenerationAdmission &admission)
      : input_(admission), ui_(admission), audio_(admission), storage_(admission),
        content_(admission), display_(admission), ui_draw_(admission), assets_(admission),
        audio_state_(admission), lifecycle_(admission), effects_(admission), sources_(admission),
        scenes_(admission, true) {}
  GenerationServices(const GenerationServices &) = delete;
  GenerationServices &operator=(const GenerationServices &) = delete;
  // Only before this generation's host is exposed; no borrowed table is rebound afterward.
  void Bind(const runtime::GameplayServices &source, NexoraGameplaySceneApiV1 scene) noexcept {
    api_.input = GuardService(input_, source.input);
    api_.ui = GuardService(ui_, source.ui);
    api_.audio = GuardService(audio_, source.audio);
    api_.storage = GuardService(storage_, source.storage);
    api_.content = GuardService(content_, source.content);
    api_.display = GuardService(display_, source.display);
    api_.ui_draw = GuardService(ui_draw_, source.ui_draw);
    api_.assets = GuardService(assets_, source.assets);
    api_.audio_state = GuardService(audio_state_, source.audio_state);
    api_.lifecycle = GuardService(lifecycle_, source.lifecycle);
    api_.twod_effects = GuardService(effects_, source.twod_effects);
    api_.sprite_content = GuardService(sources_, source.sprite_content);
    // Scene requests are mutable values owned by GameplayGeneration, so candidate and retired
    // contexts may operate on their own SceneSession without touching the published one.
    api_.scene = GuardService(scenes_, scene);
  }
  runtime::GameplayServices &Api() noexcept { return api_; }

private:
  runtime::GameplayServices api_;
  ServiceGate<NexoraGameplayInputApiV1> input_;
  ServiceGate<NexoraGameplayUiApiV1> ui_;
  ServiceGate<NexoraGameplayAudioApiV1> audio_;
  ServiceGate<NexoraStorageApiV1> storage_;
  ServiceGate<NexoraGameplayContentApiV1> content_;
  ServiceGate<NexoraGameplayDisplayApiV1> display_;
  ServiceGate<NexoraGameplayUiDrawApiV1> ui_draw_;
  ServiceGate<NexoraGameplayAssetsApiV1> assets_;
  ServiceGate<NexoraGameplayAudioStateApiV1> audio_state_;
  ServiceGate<NexoraGameplayLifecycleApiV1> lifecycle_;
  ServiceGate<NexoraGameplayTwoDEffectsApiV1> effects_;
  ServiceGate<NexoraGameplaySpriteContentApiV1> sources_;
  ServiceGate<NexoraGameplaySceneApiV1> scenes_;
};
} // namespace nexora::player::detail
