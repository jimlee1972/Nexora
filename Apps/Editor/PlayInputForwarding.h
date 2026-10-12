#pragma once

#include "Nexora/Runtime/EditorSdk.h"
#include "PlayGameplayModule.h"

namespace nexora::editor::preview {
// Forward each native batch once, including deferred/zero-extent frames. No tick or GUI frame is
// needed to revoke capture or release held controls; native client-pixel coordinates stay intact.
inline void ForwardPlayInput(runtime::PlaySession &play, PlayGameplayModule &gameplay,
                             bool canvas_focused,
                             std::span<const Nexora::Window::WindowEvent> events,
                             const PlayInputBindings &bindings = {}, std::uint32_t user = 0) {
  const bool selected = gameplay.SetInputUser(user);
  play.SetInputFocus(selected && canvas_focused && play.State() == runtime::PlayState::Playing);
  gameplay.SetInputFocus(play.AcceptsInput());
  static_cast<void>(gameplay.SetInputBindings(bindings));
  gameplay.ProcessInput(events);
}
} // namespace nexora::editor::preview
