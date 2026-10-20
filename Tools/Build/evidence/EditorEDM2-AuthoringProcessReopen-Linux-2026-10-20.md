# ED-M2 real ImGui authoring and fresh-process reopen — Linux, 2026-10-20

Beads delivery: `nexora-owg.3.1`. Parent target-host acceptance remains open.
Initial fixture base: main `78a0d429fc9317668baad1e623273de2646d463d`.
Delivery integrates main `8fd501c46d9736ce5295c0ee12991eeca6518e20` before full validation.

## Scope and actual services

Two independent native test processes write and reopen a Unicode project path. The write process
uses actual ImGui widgets with synthetic physical pointer/keyboard events: Hierarchy single/Ctrl
multi-selection, mixed Inspector input/Enter, Ctrl+Z/Y, Ctrl+S and a cancelled text draft. Only
fixture seeding uses direct document creation APIs. Each chord includes real modifier down/up.
Save requests go through production SceneFileSession, with the workspace opened before session
construction. A fresh reader World, project writer lease and UI context reopen the saved scene.
Both edited transforms, unchanged remaining lanes, exact unknown component type/name/raw bytes,
clean state and multi-selection Inspector values are checked. Read-only Delete/Save and stale-generation
file tokens reject without changing saved bytes. Corrupt-file Open preserves the exact current
World, file token/path, last-good source bytes and rejected corrupt input. Existing editor.selection_delete_transaction
replays 1,000 cycles independently; this test does not duplicate it.

This is headless actual ImGui and process-lifetime evidence. It does not launch NexoraEditor OS
windows, prove physical pixels/glyphs/IME or accept target-host accessibility. No runtime feature,
public ABI or persistent format is changed. Parent M2 acceptance and physical gates stay open.

## Preliminary verification

GCC14.2/Linux. The fixture compiled directly against real product libraries and the Python driver
passed independent write/read processes at both 1x and 2x. Initial shortcut injection specified a
modifier mask without physical Ctrl transitions; the strict text assertion exposed that harness
error. Sending actual Ctrl down/up corrected it; no product behavior or assertion was relaxed.

## Complete Linux delivery gate

Final source integrates main `8fd501c46d9736ce5295c0ee12991eeca6518e20`. Independent Development
Modular product configure/build passes (771 initial steps, then 315 current-main metadata/relink
steps). Editor and graphical shell ON; Slang/Zig and verification-only/native ProjectPlayer OFF.
The final 1x/2x actual fixture passes 2/2 in 0.30 s, including read-only Delete and corrupt-file Open.

Fresh preset configure/build and the entire serial Linux suite pass 291 registered tests:
279 actual passes, 12 existing native feature/environment skips, zero failures, 629.09 s. Actual
write/read workflows pass 0.15 s each. Existing 1,000-cycle transaction replay also passes in this
suite; no second replay implementation or physical/native-window result is claimed. The original
100k lifecycle passes 277.36 s at timeout multiplier 1; no assertion or deadline is changed.
All three source/CMake hashes remain unchanged through the gate; full LastTest is archived outside
the repository. Documentation 16/16, Markdown/local links/bilingual pairing and touched C++/diff
checks pass before publication. Hosted platform CI must pass separately before squash merge.

This test-only delivery changes no product SDK, persistence or module boundary. The parent scene
acceptance and all physical display/DPI/IME/accessibility and native-application gates remain open.

## Current-main integration before publication update

The original PR672 head `ff16c4ce` completed all applicable individual hosted jobs. Main then
advanced through the independent Git provider and hierarchy request optimization. The two README
append sections conflict; the normal merge preserves both complete contracts, with no authoring
implementation or assertion changes. Actual test C++/Python hashes are unchanged.

Fresh `cmake --preset linux-development`, `cmake --build --preset linux-development` and
`CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development` pass on main `43d6bb96` plus this
feature:297 registered,285 actual passes,12 existing native/environment skips,zero failures,
637.47s. Actual authoring/reopen1x passes0.15s and2x0.21s; unchanged100k hierarchy passes241.55s
with the original300s deadline (scale1). Documentation/links/bilingual/format/diff checks are
repeated before the normal fast-forward publication update. The updated PR head must complete
its own hosted checks; prior-head success is not substituted. Physical/native-window acceptance
and the parent scene-authoring goal remain open.
