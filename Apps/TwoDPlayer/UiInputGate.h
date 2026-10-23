#pragma once
#include "Nexora/Runtime/GameplayInput.h"
#include "Nexora/TwoD/GameplayUi.h"
namespace nexora::player {
// Owner-loop state. A held control restored by a full gamepad sample is not a fresh confirmation.
class UiInputGate final {
public:
  // Restore routing BEFORE a complete native sample. Cleared/ignored actions are not proof
  // that a held controller confirmation was physically released.
  template <class Poll>
  static bool PollGamepads(runtime::GameplayInput &input, bool foreground, Poll &&poll) {
    if (!input.SetFocused(foreground))
      return false;
    return poll();
  }
  void Apply(twod::GameplayUiInput &input, bool interrupted, bool neutral) noexcept {
    needs_neutral_ |= interrupted || !input.focused;
    const bool suppress = needs_neutral_;
    if (input.focused && neutral)
      needs_neutral_ = false;
    if (suppress) {
      input.navigate = 0;
      input.activate = input.cancel = false;
      input.pointer_down = input.pointer_pressed = input.pointer_released = false;
      input.focused = false; // Clear retained focus/capture; rearm on a later frame.
    }
  }

private:
  bool needs_neutral_{};
};
} // namespace nexora::player
