#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nexora::editor {
// UI-independent controls for the initial copied Play input snapshot. Editor release/Play
// shortcuts (Escape/F5/F6/F10) are deliberately absent. No native key codes cross this value type.
enum class PlayInputControl : std::uint8_t {
  None,
  A,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,
  Left,
  Right,
  Up,
  Down,
  Space,
  LeftShift,
  RightShift,
  LeftControl,
  RightControl,
  MouseLeft,
  MouseRight,
  Count
};
enum class PlayInputAction : std::uint8_t {
  Left,
  Right,
  Forward,
  Backward,
  Action,
  Primary,
  Secondary,
  Sprint,
  Modifier,
  Count
};
struct PlayInputBindings final {
  using Control = PlayInputControl;
  static constexpr std::size_t kActions = static_cast<std::size_t>(PlayInputAction::Count);
  std::array<std::array<Control, 2>, kActions> controls{
      {{Control::A, Control::Left},
       {Control::D, Control::Right},
       {Control::W, Control::Up},
       {Control::S, Control::Down},
       {Control::Space, Control::None},
       {Control::MouseLeft, Control::None},
       {Control::MouseRight, Control::None},
       {Control::LeftShift, Control::RightShift},
       {Control::LeftControl, Control::RightControl}}};
  // None may be repeated. Every concrete control has at most one owner. Movement accepts
  // keyboard controls only; unbound actions are allowed. Invalid candidates are never published.
  [[nodiscard]] bool Valid() const noexcept {
    std::array<bool, static_cast<std::size_t>(Control::Count)> used{};
    for (std::size_t action = 0; action < controls.size(); ++action)
      for (const auto control : controls[action]) {
        const auto value = static_cast<std::size_t>(control);
        if (value >= used.size() ||
            (action < 4 && (control == Control::MouseLeft || control == Control::MouseRight)))
          return false;
        if (control == Control::None)
          continue;
        if (used[value])
          return false;
        used[value] = true;
      }
    return true;
  }
  bool operator==(const PlayInputBindings &) const = default;
};
} // namespace nexora::editor
