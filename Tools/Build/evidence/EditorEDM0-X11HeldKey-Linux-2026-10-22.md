# ED-M0 independent X11 held-key repair — scoped Linux evidence

This report records a new, independently reviewable repair of the currently public X11 backend.
It does not review or complete the unavailable historical 05a5 patch/worktree. Existing parent
ownership/history remain intact. The focused controls below passed; complete integrated and
physical target-host acceptance remain separate. Editor milestone remains 0/8.

## Production scope and limits

`Engine/Window/src/X11Window.cpp` requests detectable auto-repeat on its own client Display.
Per-window held-key tracking emits physical transitions once while every repeated native press
continues through the existing XIM/UTF-8 text path. Focus loss and destruction erase held state.
The legacy fallback recognizes only a strict genuine adjacent release/press pair already queued
in Xlib: matching type, window, keycode and timestamp, with explicit SendEvent input rejected.
It never retains a release awaiting a future arrival. Split arrivals are forwarded normally;
equal timestamps are a legacy repeat heuristic, not a universal physical-device classifier.

No global repeat shutdown, repeat-rate change, text suppression, ImGui trickling change or Editor
command-routing change is used. Window public API and render-frame contracts remain in scope
without interface changes. This bounded repair is not a claim that all historical Editor input
stalls have been attributed or fixed. Pure matcher tests cover SendEvent/type/window/key/time
mismatches, in addition to the real native queue path.

## Controlled native evidence

The same real xdotool/XTEST held N gesture runs against a dedicated Xvfb server. A test-only
`LD_PRELOAD` observer forwards native calls in normal mode. Legacy mode rejects the detectable
repeat True request on the **same backend Display**, confirms it is off there, and records the
raw stream consumed by the backend. This is injected unavailability, not evidence from a
physically unsupported server. The observer and mode are not part of production Window API.

Probe SHA256: `fd7a878f74dca35aa2396a020af56f9ee744c9c02f3d60f91134148755211ca5`.
Shim SHA256: `d2310c507d1aad6055bd4d5f567846efa1f8d9561688dcf3a1b8ad92ab04d183`.
These are unchanged across old/fixed and focus-mutant/restored controls. Actual `/proc` mapped
Window library paths/hashes supplement the functional counts; they do not replace them.

| Backend / mode | N down / up / text | Raw genuine N press / release | M down / up | Global repeat before / after | Actual report result |
| --- | --- | --- | --- | --- | --- |
| old-backend-negative / normal | 25 / 25 / 25 | 25 / 25 | 2 / 1 | 1 / 1 | FAIL (expected negative control) |
| old-backend-negative / legacy | 25 / 25 / 25 | 25 / 25 | 2 / 1 | 1 / 1 | FAIL (expected negative control) |
| fixed-backend-positive / normal | 1 / 1 / 25 | 25 / 1 | 2 / 1 | 1 / 1 | PASS |
| fixed-backend-positive / legacy | 1 / 1 / 25 | 25 / 25 | 2 / 1 | 1 / 1 | PASS |
| focus-mutant-negative / normal | 1 / 1 / 25 | 25 / 1 | 1 / 1 | 1 / 1 | FAIL (expected negative control) |
| focus-mutant-negative / legacy | 1 / 1 / 25 | 25 / 25 | 1 / 1 | 1 / 1 | FAIL (expected negative control) |
| focus-restored-positive / normal | 1 / 1 / 25 | 25 / 1 | 2 / 1 | 1 / 1 | PASS |
| focus-restored-positive / legacy | 1 / 1 / 25 | 25 / 25 | 2 / 1 | 1 / 1 | PASS |

The public old backend emits 25 physical press/release pairs in both modes; the repaired backend
emits one pair while preserving exactly 25 text commits for 25 genuine native presses. Normal
mode receives one raw genuine release; injected legacy mode receives 25. This proves the legacy
path was exercised and distinguishes normalization from lost text. Global repeat remains on.

For focus clearing, M is held while focus moves to a second window, released there, then pressed
again after refocusing the original window. Its expected original-window count is two downs and
one final up. Erasing the focus-clear action produces one down/one up and fails this oracle;
restoring source produces two downs/one up and passes. The restored source SHA256 is
`b560c5e4411f80ab6255b236d3ff63733dbe31c7b2d6938923d0d80999bb1020`.
No mutant is the final production source.

Actual mapped Window library hashes:

- old-backend-negative / normal: `fb998113dfa9f0961bf0d8528ddffd778a98536a8d76444ffd96887fbc00abab`.
- old-backend-negative / legacy: `fb998113dfa9f0961bf0d8528ddffd778a98536a8d76444ffd96887fbc00abab`.
- fixed-backend-positive / normal: `868bef269e139c23bd043a8c5b52db1a4ae949fe84aea2de29ffe71a691ea62c`.
- fixed-backend-positive / legacy: `868bef269e139c23bd043a8c5b52db1a4ae949fe84aea2de29ffe71a691ea62c`.
- focus-mutant-negative / normal: `482196531f4a41fe57eb7cf4e539482e01a0642076ab94f3615d3bc5878acaed`.
- focus-mutant-negative / legacy: `482196531f4a41fe57eb7cf4e539482e01a0642076ab94f3615d3bc5878acaed`.
- focus-restored-positive / normal: `868bef269e139c23bd043a8c5b52db1a4ae949fe84aea2de29ffe71a691ea62c`.
- focus-restored-positive / legacy: `868bef269e139c23bd043a8c5b52db1a4ae949fe84aea2de29ffe71a691ea62c`.

## Focused, Shipping and headless validation

- `window.x11_key_repeat_matcher` and `window.x11_held_key_native`: 2/2 PASS, 0 skips,
  1.92 s total (`refined-focused.log`; native 1.91 s).
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: PASS;
  build 10.917677 s and the changed `X11Window.cpp` object actually compiled. Editor remains
  excluded by Shipping policy; this is Window compile evidence, not Editor font/runtime evidence.
- Fresh `cmake --preset linux-headless -B build/linux-x11-headless
  -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF -DBUILD_TESTING=OFF`, followed by
  `cmake --build build/linux-x11-headless --target NexoraDedicatedServer`: PASS,
  26 build steps, 8.395593 s. Window/Presentation/ImGui targets are absent. This validates the
  server build boundary; it is not a complete headless CTest run.

## Complete integrated result

The earlier X11 integration run was intentionally interrupted after 282 passed tests, by SIGINT during
test 283. CTest returned -2 after 709.026 s; all 5,535 archived source/asset/configuration hashes
remained unchanged. This is **INTERRUPTED**, not a complete-suite pass. Retained status/log/
hashes use `x11-current/complete-standard-*`.

The applied combined source passed the focused matcher/native/font group 3/3 in 2.32 s
(native 1.92 s; font 0.38 s). The refreshed Monolithic font build passed in 10 build steps,
and its actual font test passed 1/1 in 0.21 s (`combined-monolithic-*.log`). These focused
results do not override the subsequent observed complete-run failures.

<!-- combined-font-input-first-attempt:start -->
The first combined run (`combined-font-input-standard-*`) completed with configure exit 0
(1.695 s), build exit 0 (2.834 s), and CTest exit 8: **366/368 PASS, 2 FAIL, 0 skips**.
CTest reported 1657.99 s (wrapper 1658.007 s). The failures were test 298
`editor.linux_native_scene_preview` (45.14 s timeout after the transform/12-Undo sequence) and test 311
`editor.linux_native_play_pointer_buttons` (5.12 s). All 5,535 frozen source/asset/configuration hashes
remained unchanged after the run. These failures remain recorded; they are not relabeled as
passes or attributed to the X11/font production changes. Retained source/binary identities are
in `combined-font-input-failures/source-binary-manifest.json`.

The applied Preview fixture candidate removes its additional fixed 80 ms wait per key phase
while retaining actual GUI edge acknowledgements, every assertion, and the original 45 s bound.
The PointerButtons fixture candidate retains three equal-pose frames in the original first
150 ms window, requires at least two after the observed neutral input, requires every
post-neutral pose to remain equal, and retains the actual second 150 ms pixel/draw checks.
These fixture-observation changes retain the production backend/font binaries. The repaired
focused native tests passed **2/2, zero skips, 37.76 s**: Preview 30.52 s under its unchanged 45 s
bound, and PointerButtons 7.23 s. The Editor executable SHA256 remained
`a584bc2be484e009f231b43316358d67598f8abf59b9def5eaba61ba8526a095`.
Preview source SHA256: `a82d18d2ecff01fe3ad2e09dbe184f991c7a1f037ba12274732896d982d0fe01`;
GameView source SHA256: `575c8fe70866145a3ea9a140c2bf1fe13bf2b8e969f92b70eba18089627a4a8f`.
Raw focused log: `combined-repaired-focused-ctest.log`. The retained Pointer release trace
and eight mutation/continuation cases passed 9/9 offline checks; these do not replace native
acceptance. The complete final-prefix run remains separately pending.
<!-- combined-font-input-first-attempt:end -->

## Subsequent integrated review failure

The next run (`combined-font-input-final-standard-*`) completed **367/368 PASS, 1 FAIL,
0 skips, 1684.33 s** (CTest exit 8; wrapper 1684.34 s). Preview passed in 32.20 s under
its unchanged 45 s limit; PointerButtons passed in 7.59 s. Only
`editor.linux_prefab_placement_overrides` failed (25.09 s), awaiting the actual Review owning
result after reopening read-only. The old fixture deleted its scratch directory on failure;
no retained native click trace establishes the exact missed-control cause.

The console process handle disconnected after CTest status persistence. Before any source
changes, the maintained source freeze allowed recovery of LastTest/LastTestsFailed and an
explicit comparison of every archived source hash: **5,535 files, zero changes** (recovery
2026-10-23T00:14:01.824570+00:00). This is a failed suite, never a pass. The new bounded Review
fixture repair and complete rerun require separate acceptance; see
[owning-review follow-up](EditorEDM0-PrefabInput-Linux-2026-10-22.md#owning-review-follow-up).

<!-- combined-font-input-result:start -->
Recorded UTC: `2026-10-23T00:49:39.327601+00:00`.
Final combined configure/build/CTest: **PASS 368/368, zero failures/skips, 1639.24 s**.
Source: `combined font/MSVC/X11/native-review source based on 7c294bf3cea7e3478a1619590def273974fbea4f; frozen source-manifest SHA256 below`. All 5,535 frozen source/asset/configuration
hashes match both the completed gate comparison and this finalizer check. Source-manifest
SHA256: `e6076b5db815d8922fc7a5d55b5ef3e9f7c207c4c4b288bbafbdf10393a391c8`. Archive prefix: `/workspace/.task-state/imgui/combined-font-input-review-standard`.
This Linux result does not establish Windows compilation/runtime or physical-display/DPI/installed-IME
or overall Editor 0/8 acceptance. The interrupted old 368 attempt and historical PR699
366 results remain separate records.
<!-- combined-font-input-result:end -->

Final source identity, configured features, binary identities, source-hash comparison and
retained full logs accompany acceptance. The historical font PR699 results remain separate:
complete 366/366
with no failure/skip in 1570.09 s, Monolithic glyph test 0.21 s, native preview 43.43 s. They must
not be substituted for this changed source's 368-test integrated result.

## Retained artifact and reproduction record

The external artifact root is `/workspace/.task-state/imgui/x11-current/`. Exact control argv,
exit statuses and ELF hashes are in `old-fixed-control-status.json` and
`focus-control-status.json`; build argv/status/timings are in `shipping-headless-status.json`.
Each attempt retains its `report.json`, native `input.jsonl`, stdout/stderr and Xvfb log, on
success as well as failure. The table above is rendered from those reports, not inferred from
terminal summaries. Raw logs must be retained/published with final review evidence.

Relevant source/tests: `X11Window.cpp`, private `X11KeyRepeat.h`,
`X11KeyRepeatMatcherTests.cpp`, `X11HeldKeyProbe.cpp`, `X11RepeatCapabilityControl.cpp` and
`LinuxX11HeldKeyTests.py`. This report's generator executes no build or native test.

## Acceptance still open

Physical Linux display/input/readability and repeated focus/keyboard recovery, Windows DPI and
installed-IME composition/candidate positioning, and macOS target-host behavior remain open.
Xvfb/XTEST and injected capability denial do not replace those observations. ED-M0 and the
overall Editor milestone remain 0/8; ED-M7 screen-reader scope is unchanged.

The combined source also includes the font compiler compatibility repair: old revision `7c294`
failed MSVC C4459 shadowing and C4996 CRT-environment compilation; the applied change renames
the shadowing parameter and uses owned secure CRT environment copies. See the
[font follow-up](EditorEDM0-GlyphCoverage-Linux-2026-10-22.md#follow-up-msvc-compilation-and-combined-validation).
This does not turn Linux results into Windows compilation/runtime acceptance.

Final configured features and six binary hashes are recorded in the
[final integrated identity table](EditorEDM0-GlyphCoverage-Linux-2026-10-22.md#final-integrated-binary-identities).
