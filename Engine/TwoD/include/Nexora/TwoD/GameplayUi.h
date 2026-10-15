#pragma once
#include "Nexora/Presentation/Surface.h"
#include "Nexora/TwoD/Api.h"
#include "nexora/gameplay_ui.h"
#include <memory>
#include <vector>
namespace nexora::twod {
struct GameplayUiGlyph final {
  std::uint32_t codepoint{};
  float advance{}, x{}, y{}, width{}, height{}, u0{}, v0{}, u1{}, v1{};
};
// Supplied rasterized font metrics at pixel_size. No synthetic fallback glyphs.
struct GameplayUiFont final {
  std::uint64_t texture_id{};
  std::uint32_t width{}, height{};
  float pixel_size{}, line_height{};
  std::vector<std::byte> rgba;
  std::vector<GameplayUiGlyph> glyphs;
};
struct GameplayUiInput final {
  float pointer_x{}, pointer_y{};
  bool pointer_down{}, pointer_pressed{}, pointer_released{};
  int navigate{}; // -1 previous, +1 next, 0 unchanged; keyboard or gamepad
  bool activate{}, cancel{}, focused{true};
};
struct NEXORA_2D_API GameplayUiFrame final {
  std::vector<Nexora::Presentation::UiVertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<Nexora::Presentation::UiDrawCommand> commands;
  std::vector<Nexora::Presentation::UiTextureUpload> uploads;
  std::vector<std::shared_ptr<const GameplayUiFont>> font_owners;
  [[nodiscard]] Nexora::Presentation::UiDrawData DrawData() const noexcept;
};
class NEXORA_2D_API GameplayUi final {
public:
  // Reserve a surface-local immutable texture ID distinct from Sprite atlases/fonts.
  explicit GameplayUi(std::uint64_t white_texture_id = UINT64_MAX - 2);
  ~GameplayUi();
  GameplayUi(const GameplayUi &) = delete;
  GameplayUi &operator=(const GameplayUi &) = delete;
  [[nodiscard]] int32_t GetApi(NexoraGameplayUiApiV1 *) noexcept;
  [[nodiscard]] bool RegisterFont(GameplayUiFont); // copies input ownership, append-only handles
  void BeginInput(GameplayUiInput); // once before module UI commands; clears focus on loss
  void CancelFrame();               // host recovery after a failed/stopped module callback
  [[nodiscard]] GameplayUiFrame Frame() const; // owning snapshot; empty before end_frame
private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace nexora::twod
