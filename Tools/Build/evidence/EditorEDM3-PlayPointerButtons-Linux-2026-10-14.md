# ED-M3: Five-button Play pointer input

Date: 2026-10-14. Beads: `nexora-rd7.2.9`. Base: `ed698e7e` (merged PR597).

## Delivered supporting scope

The stopped Game binding editor exposes middle/back/forward alongside left/right. New buttons
are unbound by default; existing keyboard/mouse defaults, action bits and schema-1 profile bytes
remain unchanged. Movement stays keyboard-only. Concrete controls have one owner; invalid profiles
preserve the applied mapping. `MouseMiddle`, `MouseBack` and `MouseForward` are explicit textual
names; older readers reject unfamiliar names while preserving the file.

Native pointer indices are left/right/middle/back/forward0/1/2/3/4. Win32 XBUTTON1/2 and X11
buttons8/9 normalize to3/4; the existing Cocoa button-number path already matches. Win32 vertical
and horizontal wheel fields are corrected to match X11/Cocoa and the UI reader: horizontal
`value0`, vertical `value1`. Wheel events are separate from held buttons.

The existing owner-thread held state now covers all five buttons. Acquisition, blur, Escape,
selected-user routing, Pause/Stop and profile replacement keep their reset rules. Modules receive
the existing copied action bits, never native codes. No gameplay C/Zig wire, module dependency,
shader or scene schema changes. UI coordinate evidence records only actually visible combo items,
so real wheel scrolling/selection tests cannot click clipped measurement rows.

## Regression and focused evidence

- With only the new C-ABI fixture built, the old Editor/Window fails actual X11 middle-button
  input: `Captured pointer button 2 did not drive gameplay`, CTest exit8, **1.85s**.
- Reinstating the old X11 side-button indices while retaining valid bindings fails on button8,
  CTest exit8, **2.42s**. The source was restored byte-for-byte; paired JSON records its hash.
- Restored Window/input/profile/UI/native checks pass **5/5,5.05s**. Portable checks run32
  lifecycles with all five buttons, copied masks, simultaneous holds, invalid indices, movement
  rejection, acquisition, Escape, blur and profile reset. Persistence covers exact legacy bytes,
  three new names, read-only and invalid-candidate preservation.
- Actual rendered input choices scroll and select through Window wheel/pointer events at1x/2x,
  including synthetic macOS Cmd behavior. The UI suite passes **0.19s**, without authoring or
  widget-side persistence. This is not native macOS device evidence.
- Strengthened real X11 gameplay acceptance passes **5.58s**: uncaptured middle/back/forward are
  neutral; captured holds move native mesh pixels, release stops movement, held back+Escape stops,
  and the real module observes exact copied action masks2/4/8. Pause/Step/Stop, Console/diagnostics,
  authored scene bytes and persisted input bytes remain covered.

The Windows Window contract now injects five button press/release pairs and signed vertical/
horizontal wheel messages into its actual Win32 window. It must pass hosted Windows CI; Linux
does not compile or execute that branch. Include ordering keeps `windows.h` before `imm.h`.

## Complete gates

The first complete gate passed271/272 in802.44s; the sole failure was the unchanged native prefab
isolation fixture's selected-Revert conservation check. The new pointer native case5.54s and UI
case0.22s passed. Exact clean baseline `ed698e7e` was built independently (28 actions). Diagnostic
copies only printed/retained generated data: complete baseline position-first, unchanged-feature
position-first and baseline name-first flows all passed with correct selected/unselected values,
stable identities and opaque bytes. The initial failure was not reproduced; no prefab behavior
repair is claimed. The repository fixture now adds only bounded expected/actual diagnostic context
(2,048-byte payload prefix and conservation flags), without changing assertions, input timing or
selection. The subsequent complete gate passes as recorded below; the original non-reproduced failure is
retained rather than claimed fixed.

Final Linux Development configure/build and full CTest pass **272/272,861.10s**, no skips.
The final build reports no work. Shipping passes21 actions (Minimal/Monolithic, Editor/ImGui/SDK
off; not graphical Shipping acceptance). Prefab isolation88.36s, native pointer5.58s and UI0.20s
pass. Documentation16/16,12 formatted C++ files,15 source/config hashes and275 local documentation
links pass. Paired JSON records the exact inputs and prior failure.
All final-head hosted CI still gates merge; exact merged-tree and branch deletion proof will be
recorded in the PR and Beads. Root README remains unchanged. Physical mouse/device acceptance,
gamepads, pointer look, simultaneous device owners and full ED-M3 acceptance remain open.
