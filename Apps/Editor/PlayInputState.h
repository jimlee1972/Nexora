#pragma once
#include "Nexora/Editor/PlayInputBindings.h"
#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Window/Window.h"
#include <algorithm>
#include <array>
#include <span>

namespace nexora::editor::preview {
// Owner-thread held input. Only copied snapshots cross the gameplay ABI. Focus transitions clear
// held state; the acquisition frame is discarded so clicking to capture does not fire in-game.
class PlayInputState final {
public:
  static constexpr double kMaximumPointerDelta = 65536.0;
  static constexpr double kMaximumWheelUnits = 122880.0;
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
      ResetPointer(true);
    }
    return true;
  }
  void SetFocused(bool focused) noexcept {
    if (focused_ != focused) {
      keys_.fill(false);
      buttons_ = 0;
      acquired_ = focused;
      ResetPointer(true);
    }
    focused_ = focused;
    snapshot_.move_x = snapshot_.move_y = 0;
    snapshot_.buttons = 0;
    ResetPointer(false);
  }
  bool Process(std::span<const Nexora::Window::WindowEvent> events) noexcept {
    using namespace Nexora::Window;
    ResetPointer(false);
    if (++snapshot_.sequence == 0)
      ++snapshot_.sequence;
    if (!focused_)
      return false;
    // Release always wins, including the deliberately discarded acquisition batch. Otherwise a
    // blur/Escape received while acquiring leaves the owner focused for later key batches.
    bool domain_changed = false;
    for (const auto &event : events) {
      if ((event.type == WindowEventType::FocusChanged && !event.value0) ||
          (event.type == WindowEventType::Key &&
           event.value0 == static_cast<std::int32_t>(Key::Escape) && event.value1)) {
        SetFocused(false);
        return false;
      }
      domain_changed |= ChangesPointerDomain(event.type);
    }
    if (domain_changed)
      pointer_anchored_ = false;
    if (acquired_) {
      acquired_ = false;
      for (const auto &event : events) {
        if (ChangesPointerDomain(event.type))
          pointer_anchored_ = false;
        else if (event.type == WindowEventType::Pointer)
          TrackPointer(event.value0, event.value1, false);
      }
      return false;
    }
    for (const auto &event : events) {
      if (ChangesPointerDomain(event.type)) {
        pointer_anchored_ = false;
      } else if (event.type == WindowEventType::Pointer) {
        TrackPointer(event.value0, event.value1, !domain_changed);
      } else if (event.type == WindowEventType::Wheel && !domain_changed) {
        wheel_x_ = std::clamp(wheel_x_ + event.value0, -kMaximumWheelUnits, kMaximumWheelUnits);
        wheel_y_ = std::clamp(wheel_y_ + event.value1, -kMaximumWheelUnits, kMaximumWheelUnits);
      } else if (event.type == WindowEventType::Key) {
        if (event.value0 > 0 && static_cast<std::size_t>(event.value0) < keys_.size())
          keys_[static_cast<std::size_t>(event.value0)] = event.value1 != 0;
      } else if (event.type == WindowEventType::PointerButton && event.value0 >= 0 &&
                 event.value0 < 5) {
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
      if (control >= PlayInputControl::MouseLeft && control < PlayInputControl::Count)
        return (buttons_ & (2U << (static_cast<unsigned>(control) -
                                   static_cast<unsigned>(PlayInputControl::MouseLeft)))) != 0;
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
    return true;
  }
  [[nodiscard]] NexoraInputSnapshot Snapshot() const noexcept { return snapshot_; }
  [[nodiscard]] NexoraPointerInputSnapshotV1 PointerSnapshot() const noexcept {
    return {sizeof(NexoraPointerInputSnapshotV1),
            NEXORA_POINTER_INPUT_ABI_VERSION,
            snapshot_.sequence,
            delta_x_,
            delta_y_,
            wheel_x_,
            wheel_y_,
            (buttons_ >> 1U) & 31U,
            0};
  }

private:
  static bool ChangesPointerDomain(Nexora::Window::WindowEventType type) noexcept {
    using Nexora::Window::WindowEventType;
    return type == WindowEventType::Resized || type == WindowEventType::DpiChanged ||
           type == WindowEventType::DisplayChanged;
  }
  void ResetPointer(bool reset_anchor) noexcept {
    delta_x_ = delta_y_ = wheel_x_ = wheel_y_ = 0;
    if (reset_anchor)
      pointer_anchored_ = false;
  }
  void TrackPointer(std::int32_t x, std::int32_t y, bool accumulate) noexcept {
    if (accumulate && pointer_anchored_) {
      const auto dx = static_cast<double>(static_cast<std::int64_t>(x) - pointer_x_);
      const auto dy = static_cast<double>(static_cast<std::int64_t>(y) - pointer_y_);
      delta_x_ = std::clamp(delta_x_ + dx, -kMaximumPointerDelta, kMaximumPointerDelta);
      delta_y_ = std::clamp(delta_y_ + dy, -kMaximumPointerDelta, kMaximumPointerDelta);
    }
    pointer_x_ = x;
    pointer_y_ = y;
    pointer_anchored_ = true;
  }
  bool pointer_anchored_{};
  std::int32_t pointer_x_{}, pointer_y_{};
  double delta_x_{}, delta_y_{}, wheel_x_{}, wheel_y_{};
  bool focused_{}, acquired_{};
  std::array<bool, static_cast<std::size_t>(Nexora::Window::Key::F12) + 1> keys_{};
  std::uint32_t buttons_{};
  NexoraInputSnapshot snapshot_{};
  PlayInputBindings bindings_{};
};
} // namespace nexora::editor::preview
