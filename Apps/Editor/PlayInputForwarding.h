#pragma once

#include "Nexora/Runtime/EditorSdk.h"
#include "PlayGameplayModule.h"

namespace nexora::editor::preview {
// Forward each native batch once, including deferred/zero-extent frames. No tick or GUI frame is
// needed to revoke capture or release held controls; native client-pixel coordinates stay intact.
inline void ForwardPlayInput(runtime::PlaySession &play, PlayGameplayModule &gameplay,
                             bool canvas_focused,
                             std::span<const Nexora::Window::WindowEvent> events) {
  play.SetInputFocus(canvas_focused && play.State() == runtime::PlayState::Playing);
  gameplay.SetInputFocus(play.AcceptsInput());
  gameplay.ProcessInput(events);
}
} // namespace nexora::editor::preview
