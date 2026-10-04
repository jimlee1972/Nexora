#pragma once
#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Window/Window.h"
#include <array>
#include <span>

namespace nexora::editor::preview {
// Owner-thread held input. Only copied snapshots cross the gameplay ABI. Focus transitions clear
// held state; the acquisition frame is discarded so clicking to capture does not fire in-game.
class PlayInputState final {
public:
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
    const auto down = [&](Key key) { return keys_[static_cast<std::size_t>(key)]; };
    snapshot_.move_x = static_cast<double>(down(Key::D) || down(Key::RightArrow)) -
                       static_cast<double>(down(Key::A) || down(Key::LeftArrow));
    snapshot_.move_y = static_cast<double>(down(Key::W) || down(Key::UpArrow)) -
                       static_cast<double>(down(Key::S) || down(Key::DownArrow));
    snapshot_.buttons = buttons_ | (down(Key::Space) ? 1U : 0U) |
                        ((down(Key::LeftShift) || down(Key::RightShift)) ? 8U : 0U) |
                        ((down(Key::LeftControl) || down(Key::RightControl)) ? 16U : 0U);
  }
  [[nodiscard]] NexoraInputSnapshot Snapshot() const noexcept { return snapshot_; }

private:
  bool focused_{}, acquired_{};
  std::array<bool, static_cast<std::size_t>(Nexora::Window::Key::F12) + 1> keys_{};
  std::uint32_t buttons_{};
  NexoraInputSnapshot snapshot_{};
};
} // namespace nexora::editor::preview
