#pragma once
#include "Nexora/Editor/PlayInputBindings.h"
#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Window/Window.h"
#include <array>
#include <span>

namespace nexora::editor::preview {
// Owner-thread held input. Only copied snapshots cross the gameplay ABI. Focus transitions clear
// held state; the acquisition frame is discarded so clicking to capture does not fire in-game.
class PlayInputState final {
public:
  bool SetBindings(const PlayInputBindings &bindings) noexcept {
    if (!bindings.Valid())
      return false;
    if (bindings_ != bindings) {
      bindings_ = bindings;
      keys_.fill(false);
      buttons_ = 0;
      snapshot_.move_x = snapshot_.move_y = 0;
      snapshot_.buttons = 0;
      acquired_ = focused_;
    }
    return true;
  }
  void SetFocused(bool focused) noexcept {
    if (focused_ != focused) {
      keys_.fill(false);
      buttons_ = 0;
      acquired_ = focused;
    }
    focused_ = focused;
    snapshot_.move_x = snapshot_.move_y = 0;
    snapshot_.buttons = 0;
  }
  void Process(std::span<const Nexora::Window::WindowEvent> events) noexcept {
    using namespace Nexora::Window;
    if (++snapshot_.sequence == 0)
      ++snapshot_.sequence;
    if (!focused_ || acquired_) {
      acquired_ = false;
      return;
    }
    for (const auto &event : events) {
      if (event.type == WindowEventType::FocusChanged && !event.value0) {
        SetFocused(false);
        return;
      }
      if (event.type == WindowEventType::Key) {
        const auto key = static_cast<Key>(event.value0);
        if (key == Key::Escape && event.value1) {
          SetFocused(false);
          return;
        }
        if (event.value0 > 0 && static_cast<std::size_t>(event.value0) < keys_.size())
          keys_[static_cast<std::size_t>(event.value0)] = event.value1 != 0;
      } else if (event.type == WindowEventType::PointerButton && event.value0 >= 0 &&
                 event.value0 < 2) {
        const auto bit = 2U << event.value0;
        if (event.value1)
          buttons_ |= bit;
        else
          buttons_ &= ~bit;
      }
    }
    const auto down = [&](PlayInputControl control) {
      if (control == PlayInputControl::None)
        return false;
      if (control == PlayInputControl::MouseLeft || control == PlayInputControl::MouseRight)
        return (buttons_ & (control == PlayInputControl::MouseLeft ? 2U : 4U)) != 0;
      Key key = Key::Unknown;
      if (control >= PlayInputControl::A && control <= PlayInputControl::Z)
        key = static_cast<Key>(static_cast<int>(Key::A) + static_cast<int>(control) -
                               static_cast<int>(PlayInputControl::A));
      else {
        switch (control) {
        case PlayInputControl::Left:
          key = Key::LeftArrow;
          break;
        case PlayInputControl::Right:
          key = Key::RightArrow;
          break;
        case PlayInputControl::Up:
          key = Key::UpArrow;
          break;
        case PlayInputControl::Down:
          key = Key::DownArrow;
          break;
        case PlayInputControl::Space:
          key = Key::Space;
          break;
        case PlayInputControl::LeftShift:
          key = Key::LeftShift;
          break;
        case PlayInputControl::RightShift:
          key = Key::RightShift;
          break;
        case PlayInputControl::LeftControl:
          key = Key::LeftControl;
          break;
        case PlayInputControl::RightControl:
          key = Key::RightControl;
          break;
        default:
          break;
        }
      }
      return keys_[static_cast<std::size_t>(key)];
    };
    const auto action = [&](std::size_t index) {
      return down(bindings_.controls[index][0]) || down(bindings_.controls[index][1]);
    };
    snapshot_.move_x = static_cast<double>(action(1)) - static_cast<double>(action(0));
    snapshot_.move_y = static_cast<double>(action(2)) - static_cast<double>(action(3));
    snapshot_.buttons = 0;
    for (std::size_t index = 4; index < bindings_.controls.size(); ++index)
      if (action(index))
        snapshot_.buttons |= 1U << (index - 4);
  }
  [[nodiscard]] NexoraInputSnapshot Snapshot() const noexcept { return snapshot_; }

private:
  bool focused_{}, acquired_{};
  std::array<bool, static_cast<std::size_t>(Nexora::Window::Key::F12) + 1> keys_{};
  std::uint32_t buttons_{};
  NexoraInputSnapshot snapshot_{};
  PlayInputBindings bindings_{};
};
} // namespace nexora::editor::preview
