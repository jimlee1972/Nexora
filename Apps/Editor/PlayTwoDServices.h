#pragma once

#include "Nexora/Runtime/Runtime.h"
#if defined(NEXORA_HAS_2D)
#include "Nexora/Editor/ProjectTwoDPlayAssets.h"
#include "Nexora/Runtime/GameplayAudio.h"
#include "Nexora/Runtime/GameplayGamepad.h"
#include "Nexora/Runtime/GameplayInput.h"
#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/Runtime/GameplayStorage.h"
#include "Nexora/TwoD/GameplayBridge.h"
#include "Nexora/TwoD/GameplayUi.h"
#include "Nexora/Window/Window.h"
#include "PlayApiProxy.h"
#include "PlayInputState.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <stdexcept>
#endif

namespace nexora::editor::preview {
// Owning preparation; the caller rechecks Matches immediately before loading the module.
struct PlayTwoDBinding final {
  runtime::Id scene{};
#if defined(NEXORA_HAS_2D)
  std::shared_ptr<const ProjectTwoDPlayAssets> assets;
#endif
};

#if defined(NEXORA_HAS_2D)
inline std::optional<PlayTwoDBinding> CapturePlayTwoD(const ProjectWorkspace &workspace,
                                                      const ProjectContentSession &content,
                                                      const SceneDocument &document,
                                                      bool authorized, std::string &error) try {
  error.clear();
  std::vector<runtime::AssetUuid> roots;
  for (const auto &item : content.Browser().Items()) {
    if (!item.twod && item.type != ".nx2atlas" && item.type != ".nx2clip")
      continue;
    if (roots.size() == 4096) {
      error = "Play 2D asset selection exceeds 4096 sources.";
      return {};
    }
    roots.push_back(item.id);
  }
  if (roots.empty())
    return PlayTwoDBinding{};
  const auto source = document.CaptureRuntimeScene();
  if (!source) {
    error = "Play 2D scene capture is unavailable.";
    return {};
  }
  const auto generation = content.Browser().ProjectGeneration();
  auto assets = ProjectTwoDPlayAssets::Capture(workspace, content, generation, generation, roots,
                                               authorized, &error);
  if (!assets)
    return {};
  return PlayTwoDBinding{source->scene, std::move(assets)};
} catch (...) {
  error = "Play 2D asset preparation failed.";
  return {};
}

struct PlayTwoDFrame final {
  twod::SpriteFrame sprites; // Retains immutable Atlas owners through native submission.
  twod::GameplayUiFrame data;
  [[nodiscard]] Nexora::Presentation::UiDrawData DrawData() const { return data.DrawData(); }
};
inline std::uint64_t PlayTextureId() {
  static std::atomic<std::uint64_t> next{std::uint64_t{1} << 59};
  const auto value = next.fetch_add(1);
  if (value >= (std::uint64_t{1} << 60))
    throw std::runtime_error("Play texture identity exhausted");
  return value;
}
// Borrows the actual Play clone. No second World and no authoring Sprite metadata are created.
// Destroy after module callbacks and before the borrowed World. Calls serialize on its owner
// thread.
class PlayTwoDServices final {
public:
  static std::unique_ptr<PlayTwoDServices> Bind(runtime::World &world,
                                                const PlayTwoDBinding &binding, bool native = false,
                                                PlayServiceGate *gate = nullptr) {
    // Fresh binding is initial-Play work, never a substitute for a reload resource fork.
    if (gate && !gate->Check(true, true))
      return {};
    const auto *scene = world.FindScene(binding.scene);
    if (world.Kind() != runtime::WorldKind::Play || !scene ||
        scene->state != runtime::SceneState::Active || !binding.assets || !binding.assets->Assets())
      return {};
    return std::unique_ptr<PlayTwoDServices>(new PlayTwoDServices(world, binding, native, gate));
  }
  // Called after dispatch/work drains, with an identical unpublished World clone. No fresh
  // Bind, device open, focus handshake, default bindings or native texture allocation occurs.
  [[nodiscard]] std::unique_ptr<PlayTwoDServices>
  ForkForReload(runtime::World &world, PlayServiceGate &candidate_gate) const try {
    if (!gate_.OwnerThread() || !candidate_gate.OwnerThread() || &gate_ == &candidate_gate ||
        gate_.CurrentPhase() != PlayServiceGate::Phase::Capture ||
        candidate_gate.CurrentPhase() != PlayServiceGate::Phase::Candidate || !gate_.Valid() ||
        !owns_textures_ || &world == &scene_->WorldOwner())
      return {};
    return std::unique_ptr<PlayTwoDServices>(
        new PlayTwoDServices(ReloadBranch{}, *this, world, candidate_gate));
  } catch (...) {
    return {};
  }
  // Retarget only after the embedding moves identical World storage at commit/retirement.
  // Generation-owned API/proxy addresses, animation clocks and pool handles remain stable.
  [[nodiscard]] bool RebindForReload(runtime::World &world) noexcept {
    if (!gate_.OwnerThread() || !scene_->RebindForReload(world))
      return false;
    return bridge_->RebindForReload(world, *scene_, nullptr, pool_.get());
  }
  // After candidate validation and World rebind, transfer the existing native output owner.
  // Both cached audio proxies are rerouted BEFORE old stop/destroy/finalizers can execute.
  // Native device, pending queue and gamepad adapter are neither reopened nor reset.
  [[nodiscard]] bool CommitReloadResources(PlayTwoDServices &previous) noexcept {
    if (this == &previous || !gate_.OwnerThread() || !previous.gate_.OwnerThread() ||
        gate_.CurrentPhase() != PlayServiceGate::Phase::Candidate ||
        previous.gate_.CurrentPhase() != PlayServiceGate::Phase::Retired || !gate_.Valid() ||
        reload_owner_ != previous.reload_owner_ || parent_generation_ != previous.generation_ ||
        owns_textures_ || !previous.owns_textures_ || !ReloadReady() || !previous.ReloadReady() ||
        !audio_->SwapReloadMixer(*previous.audio_))
      return false;
    audio_.swap(previous.audio_);
    // Full same-version tables from these related owners make redirection nonallocating.
    const bool redirected = audio_proxy_->Redirect(audio_->Api()) &&
                            previous.audio_proxy_->Redirect(previous.audio_->Api());
    previous.native_ = previous.audio_open_ = false;
    owns_textures_ = true;
    previous.owns_textures_ = false;
    return redirected;
  }
  [[nodiscard]] bool ReloadReady() const noexcept {
    return gate_.OwnerThread() && gate_.Valid() && ui_->ReloadReady();
  }
  // Capture completed candidate UI before allocation-free World/native-owner transfer.
  // Paused Play can then display a reloaded-hook frame without advancing gameplay.
  [[nodiscard]] bool PrepareReloadCommit() noexcept try {
    if (!ReloadReady())
      return false;
    ui_frame_ = ui_->Frame();
    return true;
  } catch (...) {
    return false;
  }
  int32_t GetApi(NexoraTwoDApiV1 *output) noexcept {
    const auto result = bridge_->GetApi(output);
    if (result == NEXORA_GAMEPLAY_OK)
      *output = twod_proxy_->Get();
    return result;
  }
  int32_t GetServices(NexoraGameplayServicesApiV1 *output) noexcept {
    return services_.GetApi(output);
  }
  void ForgetEntity(runtime::Id entity) {
    if (gate_.Check(true, false))
      bridge_->ForgetEntity(entity);
  }
  [[nodiscard]] const twod::SpriteScene &Scene() const noexcept { return *scene_; }
  void RetireTextures(std::vector<std::uint64_t> &output) const {
    if (!gate_.OwnerThread() || !owns_textures_)
      return;
    for (const auto &[source, texture] : textures_) {
      static_cast<void>(source);
      output.push_back(texture);
    }
    for (const auto &[source, texture] : ui_textures_) {
      static_cast<void>(source);
      output.push_back(texture);
    }
  }
  void RevokeInput() noexcept {
    if (!gate_.Check(true, true))
      return;
    play_keys_.Reset();
    try {
      (void)input_->SetFocused(false);
      ui_->BeginInput({.focused = false});
      pointer_down_ = false;
    } catch (...) {
      ui_->CancelFrame();
    }
  }
  // Only accepted Editor capture batches reach these services; acquisition clicks are discarded.
  void ProcessInput(std::span<const Nexora::Window::WindowEvent> events, bool accepted,
                    std::uint32_t user) noexcept try {
    if (!gate_.Check(true, true))
      return;
    using namespace Nexora::Window;
    (void)input_->BeginFrame();
    if (!accepted) {
      RevokeInput();
      // Capture may anchor the pointer without admitting its click or any held controls.
      for (const auto &event : events)
        if (event.type == WindowEventType::Pointer) {
          pointer_x_ = event.value0;
          pointer_y_ = event.value1;
        } else if (event.type == WindowEventType::Key)
          static_cast<void>(play_keys_.KeyDown(event));
      return;
    }
    (void)input_->SetFocused(true);
    const bool domain = std::any_of(events.begin(), events.end(), [](const auto &e) {
      return e.type == WindowEventType::Resized || e.type == WindowEventType::DpiChanged ||
             e.type == WindowEventType::DisplayChanged;
    });
    if (domain) {
      (void)input_->SetFocused(false);
      (void)input_->SetFocused(true);
    }
    for (const auto &event : events) {
      if (event.type == WindowEventType::Pointer)
        (void)input_->SubmitPointer(user, event.value0, event.value1);
      else if (event.type == WindowEventType::Wheel && !domain)
        (void)input_->SubmitPointer(user, pointer_x_, pointer_y_, event.value0, event.value1);
      else if (event.type == WindowEventType::Key && event.value0 > 0 &&
               event.value0 <= static_cast<int>(Key::F12))
        (void)input_->Submit(user, NEXORA_INPUT_DEVICE_KEYBOARD, 0,
                             static_cast<std::uint32_t>(event.value0),
                             play_keys_.KeyDown(event) ? 1.F : 0.F);
      else if (event.type == WindowEventType::PointerButton && event.value0 >= 0 &&
               event.value0 < 5)
        (void)input_->Submit(user, NEXORA_INPUT_DEVICE_MOUSE, 0,
                             static_cast<std::uint32_t>(event.value0), event.value1 ? 1.F : 0.F);
      if (event.type == WindowEventType::Pointer) {
        pointer_x_ = event.value0;
        pointer_y_ = event.value1;
      }
    }
    if (native_)
      (void)gamepads_->Poll(*input_, user);
    NexoraGameplayPointerStateV1 pointer{};
    pointer.struct_size = sizeof(pointer);
    pointer.abi_version = 1;
    (void)services_.input.read_pointer(services_.input.context, user, &pointer);
    const auto pressed = [&](std::uint32_t action) {
      NexoraGameplayActionStateV1 state{};
      state.struct_size = sizeof(state);
      state.abi_version = 1;
      return services_.input.read_action(services_.input.context, user, action, &state) == 0 &&
             state.pressed;
    };
    twod::GameplayUiInput uiInput;
    uiInput.pointer_x = static_cast<float>(pointer_x_) - static_cast<float>(viewport_.x);
    uiInput.pointer_y = static_cast<float>(pointer_y_) - static_cast<float>(viewport_.y);
    uiInput.pointer_down = (pointer.buttons & 1) != 0;
    uiInput.pointer_pressed = uiInput.pointer_down && !pointer_down_;
    uiInput.pointer_released = !uiInput.pointer_down && pointer_down_;
    pointer_down_ = uiInput.pointer_down;
    uiInput.navigate = pressed(253) ? 1 : pressed(254) ? -1 : 0;
    uiInput.activate = pressed(255);
    uiInput.cancel = pressed(256);
    ui_->BeginInput(uiInput);
  } catch (...) {
    RevokeInput();
  }
  void SetViewport(Nexora::Presentation::SceneViewport value) noexcept {
    if (gate_.Check(true, true))
      viewport_ = value;
  }
  void UpdateAudio(double seconds) {
    if (!gate_.Check(true, true))
      return;
    ui_frame_ = ui_->Frame(); // Paused redraws retain the last completed gameplay UI.
    if (!native_)
      return;
    const auto now = std::chrono::steady_clock::now();
    if (!audio_open_ && now >= retry_audio_) {
      std::string reason;
      audio_open_ = audio_->OpenDefaultOutput(reason);
      retry_audio_ = now + std::chrono::seconds(1);
      audio_frames_ = 0;
    }
    if (!audio_open_)
      return;
    audio_frames_ += std::min(seconds, 0.25) * 48000;
    while (audio_frames_ >= 1) {
      const auto count = static_cast<std::uint32_t>(std::min(audio_frames_, 4096.0));
      if (!audio_->Pump(count)) {
        audio_->CloseOutput();
        audio_open_ = false;
        audio_frames_ = 0;
        retry_audio_ = now + std::chrono::seconds(1);
        break;
      }
      audio_frames_ -= count;
    }
  }
  [[nodiscard]] std::optional<PlayTwoDFrame>
  ExtractFrame(Nexora::Presentation::SceneViewport viewport, bool includeSprites = true) {
    if (!gate_.Check(true, true))
      return {};
    if (!viewport.width || !viewport.height)
      return {};
    auto overlay = ui_frame_;
    used_ |= !scene_->Sprites().empty() || pool_->Size() || !overlay.commands.empty();
    if (!used_)
      return {}; // A 3D module in a project with 2D assets keeps its 3D Game View.
    const auto resolver = [&](const twod::Atlas &atlas) {
      auto [found, inserted] = textures_.try_emplace(atlas.id, 0);
      if (inserted)
        found->second = PlayTextureId();
      return found->second;
    };
    auto sprites = includeSprites ? ExtractFrame(viewport.width, viewport.height, resolver)
                                  : std::optional{twod::SpriteFrame{}};
    if (!sprites || sprites->statistics.unavailable)
      throw std::runtime_error("Play 2D extraction or asset resolution failed");
    PlayTwoDFrame result{std::move(*sprites), {}};
    auto &data = result.data;
    data.uploads = result.sprites.Uploads();
    data.vertices = std::move(result.sprites.vertices);
    data.indices = std::move(result.sprites.indices);
    data.commands = std::move(result.sprites.batches);
    const auto vertexOffset = static_cast<std::int32_t>(data.vertices.size());
    const auto indexOffset = static_cast<std::uint32_t>(data.indices.size());
    data.vertices.insert(data.vertices.end(), overlay.vertices.begin(), overlay.vertices.end());
    data.indices.insert(data.indices.end(), overlay.indices.begin(), overlay.indices.end());
    const auto uiTexture = [&](std::uint64_t source) {
      auto [found, inserted] = ui_textures_.try_emplace(source, 0);
      if (inserted)
        found->second = PlayTextureId();
      return found->second;
    };
    for (auto command : overlay.commands) {
      command.vertexOffset += vertexOffset;
      command.indexOffset += indexOffset;
      command.textureId = uiTexture(command.textureId);
      data.commands.push_back(command);
    }
    for (auto upload : overlay.uploads) {
      upload.textureId = uiTexture(upload.textureId);
      upload.immutable = true;
      data.uploads.push_back(upload);
    }
    data.font_owners = std::move(overlay.font_owners);
    for (auto &vertex : data.vertices) {
      vertex.position[0] += static_cast<float>(viewport.x);
      vertex.position[1] += static_cast<float>(viewport.y);
    }
    for (auto &command : data.commands) {
      // Clip all module geometry, including UI, to the physical Game content rectangle.
      const auto x = std::clamp<std::int64_t>(command.clipX, 0, viewport.width);
      const auto y = std::clamp<std::int64_t>(command.clipY, 0, viewport.height);
      const auto right = std::clamp<std::int64_t>(
          static_cast<std::int64_t>(command.clipX) + command.clipWidth, x, viewport.width);
      const auto bottom = std::clamp<std::int64_t>(
          static_cast<std::int64_t>(command.clipY) + command.clipHeight, y, viewport.height);
      command.clipX = static_cast<std::int32_t>(viewport.x + x);
      command.clipY = static_cast<std::int32_t>(viewport.y + y);
      command.clipWidth = static_cast<std::uint32_t>(right - x);
      command.clipHeight = static_cast<std::uint32_t>(bottom - y);
    }
    std::erase_if(data.commands, [](const auto &command) {
      return command.clipWidth == 0 || command.clipHeight == 0;
    });
    return result;
  }
  [[nodiscard]] std::shared_ptr<const TwoDPlayAssets> Assets() const noexcept { return assets_; }
  using TextureResolver = std::function<std::uint64_t(const twod::Atlas &)>;
  // An owning, globally sorted CPU snapshot. Texture registration is host-owned and serialized.
  [[nodiscard]] std::optional<twod::SpriteFrame>
  ExtractFrame(std::uint32_t width, std::uint32_t height, const TextureResolver &textures) try {
    if (!gate_.Check(true, true))
      return {};
    if (!width || !height || width > 16384 || height > 16384 || !textures)
      return {};
    const auto camera = bridge_->RenderCamera(width, height);
    if (scene_->Sprites().size() + pool_->Size() > 4096)
      return {};
    const auto &world = scene_->WorldOwner();
    const auto *scene = world.FindScene(scene_->SceneId());
    if (!scene || scene->state != runtime::SceneState::Active || scene->entities.size() > 100000)
      return {};
    auto poses = world.SceneWorldPoses(scene_->SceneId());
    if (!poses || poses->size() > 100000)
      return {};
    std::sort(poses->begin(), poses->end(),
              [](const auto &a, const auto &b) { return a.id < b.id; });
    std::vector<twod::SpriteInstance> instances;
    instances.reserve(scene_->Sprites().size() + pool_->Size());
    for (const auto &pose : *poses)
      if (const auto *sprite = scene_->Find(pose.id))
        instances.push_back({instances.size() + 1, *sprite, pose.matrix});
    pool_->ForEach([&](auto &instance) {
      instances.push_back(
          {instances.size() + 1, instance.sprite, runtime::ToMatrix(instance.transform)});
    });
    return twod::ExtractSprites(
        instances, camera,
        [assets = assets_, &textures](twod::AssetId id) -> std::optional<twod::AtlasBinding> {
          const auto atlas = assets->Atlas(id);
          if (!atlas)
            return {};
          const auto texture = textures(*atlas);
          return texture ? std::optional{twod::AtlasBinding{atlas, texture}} : std::nullopt;
        });
  } catch (...) {
    return {};
  }

private:
  struct ReloadBranch final {};
  PlayTwoDServices(ReloadBranch, const PlayTwoDServices &source, runtime::World &world,
                   PlayServiceGate &gate)
      : gate_(gate), assets_(source.assets_), pool_(source.pool_->ForkForReload()),
        input_(source.input_->ForkForReload()), play_keys_(source.play_keys_),
        gamepads_(source.gamepads_), audio_(source.audio_->ForkForReload()),
        ui_(source.ui_->ForkForReload()), ui_frame_(source.ui_frame_), storage_(source.storage_),
        textures_(source.textures_), ui_textures_(source.ui_textures_), viewport_(source.viewport_),
        retry_audio_(source.retry_audio_), audio_frames_(source.audio_frames_),
        pointer_x_(source.pointer_x_), pointer_y_(source.pointer_y_),
        pointer_down_(source.pointer_down_), native_(source.native_),
        audio_open_(source.audio_open_), used_(source.used_), reload_owner_(source.reload_owner_),
        owns_textures_(false), parent_generation_(source.generation_) {
    auto scene = source.scene_->ForkForReload(world);
    if (!scene || !pool_ || !input_ || !audio_ || !ui_)
      throw std::runtime_error("Play resource snapshot unavailable");
    scene_ = std::make_unique<twod::SpriteScene>(std::move(*scene));
    bridge_ = source.bridge_->ForkForReload(world, *scene_, nullptr, pool_.get());
    if (!bridge_)
      throw std::runtime_error("Play bridge snapshot unavailable");
    services_.input = input_->Api();
    services_.audio = audio_->Api();
    services_.ui.struct_size = sizeof(services_.ui);
    services_.ui.abi_version = 1;
    services_.storage.struct_size = sizeof(services_.storage);
    services_.storage.abi_version = 1;
    if (ui_->GetApi(&services_.ui) || storage_->GetApi(services_.storage))
      throw std::runtime_error("Play resource API unavailable");
    BindProxies();
  }
  void BindProxies() {
    NexoraTwoDApiV1 twod{};
    twod.struct_size = sizeof(twod);
    if (bridge_->GetApi(&twod) != NEXORA_GAMEPLAY_OK)
      throw std::runtime_error("Play 2D proxy unavailable");
    twod_proxy_.emplace(gate_, twod);
    input_proxy_.emplace(gate_, services_.input);
    ui_proxy_.emplace(gate_, services_.ui);
    audio_proxy_.emplace(gate_, services_.audio);
    storage_proxy_.emplace(gate_, services_.storage);
#define NX_PLAY_PROXY(proxy, type, field, writes, external)                                        \
  proxy->Bind<&type::field, offsetof(type, field) + sizeof(type::field), writes, external>()
#define NX_TWOD_PROXY(field, writes)                                                               \
  NX_PLAY_PROXY(twod_proxy_, NexoraTwoDApiV1, field, writes, false)
    NX_TWOD_PROXY(set_sprite, true);
    NX_TWOD_PROXY(get_sprite, false);
    NX_TWOD_PROXY(set_collider, true);
    NX_TWOD_PROXY(get_collider, false);
    NX_TWOD_PROXY(set_camera, true);
    NX_TWOD_PROXY(get_camera, false);
    NX_TWOD_PROXY(play_animation, true);
    NX_TWOD_PROXY(pause_animation, true);
    NX_TWOD_PROXY(advance_animation, true);
    NX_TWOD_PROXY(rebuild_queries, true);
    NX_TWOD_PROXY(overlap, false);
    NX_TWOD_PROXY(sweep, false);
    NX_TWOD_PROXY(paint_tiles, true);
    NX_TWOD_PROXY(set_chunk_loaded, true);
    NX_TWOD_PROXY(get_tile, false);
    NX_TWOD_PROXY(create_instances, true);
    NX_TWOD_PROXY(destroy_instances, true);
    NX_TWOD_PROXY(read_instances, false);
    NX_TWOD_PROXY(write_instances, true);
#undef NX_TWOD_PROXY
#define NX_INPUT_PROXY(field, writes)                                                              \
  NX_PLAY_PROXY(input_proxy_, NexoraGameplayInputApiV1, field, writes, false)
    NX_INPUT_PROXY(read_action, false);
    NX_INPUT_PROXY(bind, true);
    NX_INPUT_PROXY(clear_bindings, true);
    NX_INPUT_PROXY(read_device, false);
    NX_INPUT_PROXY(read_pointer, false);
#undef NX_INPUT_PROXY
#define NX_UI_PROXY(field, writes)                                                                 \
  NX_PLAY_PROXY(ui_proxy_, NexoraGameplayUiApiV1, field, writes, false)
    NX_UI_PROXY(begin_frame, true);
    NX_UI_PROXY(panel, true);
    NX_UI_PROXY(text, true);
    NX_UI_PROXY(button, true);
    NX_UI_PROXY(end_frame, true);
    NX_UI_PROXY(get_font, false);
    NX_UI_PROXY(has_glyph, false);
    NX_UI_PROXY(register_font, true);
#undef NX_UI_PROXY
#define NX_AUDIO_PROXY(field, writes)                                                              \
  NX_PLAY_PROXY(audio_proxy_, NexoraGameplayAudioApiV1, field, writes, false)
    NX_AUDIO_PROXY(play, true);
    NX_AUDIO_PROXY(stop, true);
    NX_AUDIO_PROXY(set_paused, true);
    NX_AUDIO_PROXY(set_gain, true);
    NX_AUDIO_PROXY(set_bus_gain, true);
    NX_AUDIO_PROXY(is_playing, false);
    NX_AUDIO_PROXY(output_available, false);
    NX_AUDIO_PROXY(stop_all, true);
    NX_AUDIO_PROXY(register_wave, true);
    NX_AUDIO_PROXY(remove_asset, true);
#undef NX_AUDIO_PROXY
    NX_PLAY_PROXY(storage_proxy_, NexoraStorageApiV1, save, true, true);
    NX_PLAY_PROXY(storage_proxy_, NexoraStorageApiV1, remove, true, true);
    NX_PLAY_PROXY(storage_proxy_, NexoraStorageApiV1, load, false, true);
    NX_PLAY_PROXY(storage_proxy_, NexoraStorageApiV1, exists, false, true);
#undef NX_PLAY_PROXY
    services_.input = input_proxy_->Get();
    services_.ui = ui_proxy_->Get();
    services_.audio = audio_proxy_->Get();
    services_.storage = storage_proxy_->Get();
  }
  PlayTwoDServices(runtime::World &world, const PlayTwoDBinding &binding, bool native,
                   PlayServiceGate *gate)
      : gate_(gate ? *gate : local_gate_), assets_(binding.assets->Assets()),
        scene_(std::make_unique<twod::SpriteScene>(world, binding.scene)),
        pool_(std::make_unique<twod::InstancePool>(4096)),
        bridge_(std::make_unique<twod::GameplayBridge>(
            world, *scene_, [assets = assets_](auto id) { return assets->Clip(id); }, nullptr,
            pool_.get())),
        native_(native) {
    services_.input = input_->Api();
    services_.audio = audio_->Api();
    services_.ui.struct_size = sizeof(services_.ui);
    services_.ui.abi_version = 1;
    if (ui_->GetApi(&services_.ui) != 0)
      throw std::runtime_error("Play UI unavailable");
    const auto scope = binding.assets->Project().ToString();
    storage_ = std::make_unique<runtime::GameplayStorage>(runtime::UserGameplaySaveDirectory(scope),
                                                          scope);
    services_.storage.struct_size = sizeof(services_.storage);
    services_.storage.abi_version = 1;
    if (storage_->GetApi(services_.storage) != 0)
      throw std::runtime_error("Play storage unavailable");
    for (std::uint32_t user = 0; user < 8; ++user) {
      const auto bind = [&](std::uint32_t action, std::uint32_t device, std::uint32_t control) {
        NexoraGameplayInputBindingV1 value{sizeof(value), 1, device, 0, control, 1, 0, 0};
        if (services_.input.bind(services_.input.context, user, action, &value) != 0)
          throw std::runtime_error("Play UI input binding failed");
      };
      bind(253, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_TAB);
      bind(253, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_DOWN_ARROW);
      bind(254, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_UP_ARROW);
      bind(255, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_ENTER);
      bind(256, NEXORA_INPUT_DEVICE_KEYBOARD, NEXORA_INPUT_KEY_ESCAPE);
      bind(253, NEXORA_INPUT_DEVICE_GAMEPAD, 13);
      bind(254, NEXORA_INPUT_DEVICE_GAMEPAD, 12);
      bind(255, NEXORA_INPUT_DEVICE_GAMEPAD, 0);
      bind(256, NEXORA_INPUT_DEVICE_GAMEPAD, 1);
    }
    RevokeInput();
    if (native_)
      UpdateAudio(0);
    BindProxies();
  }
  PlayServiceGate local_gate_;
  PlayServiceGate &gate_;
  std::shared_ptr<const TwoDPlayAssets> assets_;
  std::unique_ptr<twod::SpriteScene> scene_;
  std::unique_ptr<twod::InstancePool> pool_;
  std::unique_ptr<twod::GameplayBridge> bridge_;
  std::unique_ptr<runtime::GameplayInput> input_{std::make_unique<runtime::GameplayInput>()};
  PlayShortcutFilter play_keys_;
  std::shared_ptr<runtime::NativeGameplayGamepad> gamepads_{
      std::make_shared<runtime::NativeGameplayGamepad>()};
  std::shared_ptr<runtime::GameplayAudio> audio_{std::make_shared<runtime::GameplayAudio>()};
  std::unique_ptr<twod::GameplayUi> ui_{std::make_unique<twod::GameplayUi>()};
  twod::GameplayUiFrame ui_frame_;
  std::shared_ptr<runtime::GameplayStorage> storage_;
  runtime::GameplayServices services_;
  std::optional<PlayApiProxy<NexoraTwoDApiV1>> twod_proxy_;
  std::optional<PlayApiProxy<NexoraGameplayInputApiV1>> input_proxy_;
  std::optional<PlayApiProxy<NexoraGameplayUiApiV1>> ui_proxy_;
  std::optional<PlayApiProxy<NexoraGameplayAudioApiV1>> audio_proxy_;
  std::optional<PlayApiProxy<NexoraStorageApiV1>> storage_proxy_;
  std::unordered_map<twod::AssetId, std::uint64_t, runtime::AssetUuidHash> textures_;
  std::unordered_map<std::uint64_t, std::uint64_t> ui_textures_;
  Nexora::Presentation::SceneViewport viewport_{};
  std::chrono::steady_clock::time_point retry_audio_{};
  double audio_frames_{};
  std::int32_t pointer_x_{}, pointer_y_{};
  bool pointer_down_{}, native_{}, audio_open_{}, used_{};
  std::shared_ptr<const std::uint8_t> reload_owner_{
      std::make_shared<const std::uint8_t>(std::uint8_t{})};
  bool owns_textures_{true};
  const std::shared_ptr<const std::uint8_t> generation_{
      std::make_shared<const std::uint8_t>(std::uint8_t{})};
  std::shared_ptr<const std::uint8_t> parent_generation_;
};
#endif
} // namespace nexora::editor::preview
