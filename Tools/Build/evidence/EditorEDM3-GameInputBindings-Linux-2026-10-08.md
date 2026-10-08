# ED Game input binding drafts: Linux evidence

Date: 2026-10-08. Source parent: `eeaaf284` (Content keyboard navigation).

The stopped Game panel now offers two finite keyboard/mouse control slots for each of four
movement directions and five copied button actions. Defaults preserve WASD/arrows and
Space/mouse/Shift/Ctrl. The portable owning profile permits None/unbound actions and rejects
unknown controls, duplicate concrete controls (including within one action), and mouse-axis
bindings. Escape/F5/F6/F10 are excluded from the finite control set. There is no C gameplay ABI,
module dependency or existing public class-layout change.

UI drafts support Apply, Cancel/Escape and Reset defaults. Only successful Apply replaces the
whole copied profile. Invalid Apply retains its draft/error and the previous profile; Reset
changes only the draft. Read-only projects permit session-only settings. Changing/detaching the
project root/UUID resets them; scene replacement and new Play within the same project retain them.
The modal blocks authoring/File/Play commands and cancels Scene/Inspector drafts. Blur (including
loss/regain without a frame), recovery, close/file/rename/Play-review or externally started Play
cancels edits. Playing and Paused disable opening. Bounded modal geometry and equal-width table
columns avoid auto-fit hit-position drift at 1x/2x.

The application copies `GameInputBindings()` into both rendered and deferred/zero-extent
`ForwardPlayInput` batches before gameplay input processing. PlayInputState translates Window
keys/buttons to portable controls, then derives axes and five bits through the selected slots.
Successful profile replacement clears held controls/output and discards an acquired focused batch;
unchanged profiles retain held input and invalid profiles preserve the previous mapping/output.
Sequence remains monotonic, reserved remains zero, and callbacks receive an owning frame snapshot.
No binding widget performs IO or loads gameplay code, and no profile is persisted or transmitted.

Validation coverage:

- `editor.game_input_bindings`: real key/pointer events at 1x/2x DPI and macOS modifier semantics;
  draft publication, valid/duplicate Apply, Cancel/Escape, Reset/Cancel, owning copies, read-only,
  authoring/File/Play gates, blur/regain, recovery, close, Playing/Paused and project detach.
- `editor.play_input_state`: custom letters, alternate arrows, remapped keyboard/mouse bits,
  duplicate/unknown/mouse-axis rejection, unbound actions, unchanged mapping, successful replacement
  and acquisition clearing, plus existing opposed-axis/focus/held-state coverage.
- `editor.play_gameplay_module`: actual copied gameplay capture callback with a custom key,
  repeated same-frame capture and key-up without GUI rendering; existing pause/failure/unload tests.
- Full Linux native tests preserve the default gameplay workflow; they do not constitute a new
  physical-host rebinding or complete expanded-device acceptance.

This is initial Editor-session keyboard/mouse rebinding, not arbitrary physical-key capture,
project/device profile persistence, gamepad, pointer look, multiple users, hot reload, or complete
ED-M3/accessibility acceptance. These remain open with Windows/macOS physical-host validation.

Commands use `/workspace/.nexora/env.sh`, graphical shell ON and, for the full gate, lavapipe with
Khronos core/synchronization validation plus pinned documentation dependencies:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development -R 'editor.game_input_bindings|editor.play_input_state|editor.play_gameplay_module|editor.scene_file_input'
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Focused tests **4/4 passed**, **0.52 s**. The final full Linux gate passed **154/154**,
**358.42 s**, with zero failures/skips. Development configure/build and Minimal Monolithic Shipping
configure/build passed. Changed Markdown/bilingual documentation validation and `git diff --check`
passed. Both roadmap languages and owning Editor/EditorImGui/application contracts are synchronized.
Generated tools, logs and build output remain uncommitted.
