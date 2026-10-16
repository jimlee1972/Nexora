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
  static std::unique_ptr<PlayTwoDServices>
  Bind(runtime::World &world, const PlayTwoDBinding &binding, bool native = false) {
    const auto *scene = world.FindScene(binding.scene);
    if (world.Kind() != runtime::WorldKind::Play || !scene ||
        scene->state != runtime::SceneState::Active || !binding.assets || !binding.assets->Assets())
      return {};
    return std::unique_ptr<PlayTwoDServices>(new PlayTwoDServices(world, binding, native));
  }
  int32_t GetApi(NexoraTwoDApiV1 *output) noexcept { return bridge_.GetApi(output); }
  int32_t GetServices(NexoraGameplayServicesApiV1 *output) noexcept {
    return services_.GetApi(output);
  }
  void ForgetEntity(runtime::Id entity) { bridge_.ForgetEntity(entity); }
  [[nodiscard]] const twod::SpriteScene &Scene() const noexcept { return scene_; }
  void RetireTextures(std::vector<std::uint64_t> &output) const {
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
    try {
      (void)input_.SetFocused(false);
      ui_.BeginInput({.focused = false});
      pointer_down_ = false;
    } catch (...) {
      ui_.CancelFrame();
    }
  }
  // Only accepted Editor capture batches reach these services; acquisition clicks are discarded.
  void ProcessInput(std::span<const Nexora::Window::WindowEvent> events, bool accepted,
                    std::uint32_t user) noexcept try {
    using namespace Nexora::Window;
    (void)input_.BeginFrame();
    if (!accepted) {
      // Capture may anchor the pointer without admitting its click or any held controls.
      for (const auto &event : events)
        if (event.type == WindowEventType::Pointer) {
          pointer_x_ = event.value0;
          pointer_y_ = event.value1;
        }
      RevokeInput();
      return;
    }
    (void)input_.SetFocused(true);
    const bool domain = std::any_of(events.begin(), events.end(), [](const auto &e) {
      return e.type == WindowEventType::Resized || e.type == WindowEventType::DpiChanged ||
             e.type == WindowEventType::DisplayChanged;
    });
    if (domain) {
      (void)input_.SetFocused(false);
      (void)input_.SetFocused(true);
    }
    for (const auto &event : events) {
      if (event.type == WindowEventType::Pointer)
        (void)input_.SubmitPointer(user, event.value0, event.value1);
      else if (event.type == WindowEventType::Wheel && !domain)
        (void)input_.SubmitPointer(user, pointer_x_, pointer_y_, event.value0, event.value1);
      else if (event.type == WindowEventType::Key && event.value0 > 0 &&
               event.value0 <= static_cast<int>(Key::F12))
        (void)input_.Submit(user, NEXORA_INPUT_DEVICE_KEYBOARD, 0,
                            static_cast<std::uint32_t>(event.value0), event.value1 ? 1.F : 0.F);
      else if (event.type == WindowEventType::PointerButton && event.value0 >= 0 &&
               event.value0 < 5)
        (void)input_.Submit(user, NEXORA_INPUT_DEVICE_MOUSE, 0,
                            static_cast<std::uint32_t>(event.value0), event.value1 ? 1.F : 0.F);
      if (event.type == WindowEventType::Pointer) {
        pointer_x_ = event.value0;
        pointer_y_ = event.value1;
      }
    }
    if (native_)
      (void)gamepads_.Poll(input_, user);
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
    ui_.BeginInput(uiInput);
  } catch (...) {
    RevokeInput();
  }
  void SetViewport(Nexora::Presentation::SceneViewport value) noexcept { viewport_ = value; }
  void UpdateAudio(double seconds) {
    ui_frame_ = ui_.Frame(); // Paused redraws retain the last completed gameplay UI.
    if (!native_)
      return;
    const auto now = std::chrono::steady_clock::now();
    if (!audio_open_ && now >= retry_audio_) {
      std::string reason;
      audio_open_ = audio_.OpenDefaultOutput(reason);
      retry_audio_ = now + std::chrono::seconds(1);
      audio_frames_ = 0;
    }
    if (!audio_open_)
      return;
    audio_frames_ += std::min(seconds, 0.25) * 48000;
    while (audio_frames_ >= 1) {
      const auto count = static_cast<std::uint32_t>(std::min(audio_frames_, 4096.0));
      if (!audio_.Pump(count)) {
        audio_.CloseOutput();
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
    if (!viewport.width || !viewport.height)
      return {};
    auto overlay = ui_frame_;
    used_ |= !scene_.Sprites().empty() || pool_.Size() || !overlay.commands.empty();
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
    if (!width || !height || width > 16384 || height > 16384 || !textures)
      return {};
    const auto camera = bridge_.RenderCamera(width, height);
    if (scene_.Sprites().size() + pool_.Size() > 4096)
      return {};
    const auto &world = scene_.WorldOwner();
    const auto *scene = world.FindScene(scene_.SceneId());
    if (!scene || scene->state != runtime::SceneState::Active || scene->entities.size() > 100000)
      return {};
    auto poses = world.SceneWorldPoses(scene_.SceneId());
    if (!poses || poses->size() > 100000)
      return {};
    std::sort(poses->begin(), poses->end(),
              [](const auto &a, const auto &b) { return a.id < b.id; });
    std::vector<twod::SpriteInstance> instances;
    instances.reserve(scene_.Sprites().size() + pool_.Size());
    for (const auto &pose : *poses)
      if (const auto *sprite = scene_.Find(pose.id))
        instances.push_back({instances.size() + 1, *sprite, pose.matrix});
    pool_.ForEach([&](auto &instance) {
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
  PlayTwoDServices(runtime::World &world, const PlayTwoDBinding &binding, bool native)
      : assets_(binding.assets->Assets()), scene_(world, binding.scene), pool_(4096),
        bridge_(
            world, scene_, [assets = assets_](auto id) { return assets->Clip(id); }, nullptr,
            &pool_),
        native_(native) {
    services_.input = input_.Api();
    services_.audio = audio_.Api();
    services_.ui.struct_size = sizeof(services_.ui);
    services_.ui.abi_version = 1;
    if (ui_.GetApi(&services_.ui) != 0)
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
  }
  std::shared_ptr<const TwoDPlayAssets> assets_;
  twod::SpriteScene scene_;
  twod::InstancePool pool_;
  twod::GameplayBridge bridge_;
  runtime::GameplayInput input_;
  runtime::NativeGameplayGamepad gamepads_;
  runtime::GameplayAudio audio_;
  twod::GameplayUi ui_;
  twod::GameplayUiFrame ui_frame_;
  std::unique_ptr<runtime::GameplayStorage> storage_;
  runtime::GameplayServices services_;
  std::unordered_map<twod::AssetId, std::uint64_t, runtime::AssetUuidHash> textures_;
  std::unordered_map<std::uint64_t, std::uint64_t> ui_textures_;
  Nexora::Presentation::SceneViewport viewport_{};
  std::chrono::steady_clock::time_point retry_audio_{};
  double audio_frames_{};
  std::int32_t pointer_x_{}, pointer_y_{};
  bool pointer_down_{}, native_{}, audio_open_{}, used_{};
};
#endif
} // namespace nexora::editor::preview
