# ED-M0 independent X11 held-key repair — scoped Linux evidence

This report records a new, independently reviewable repair of the currently public X11 backend.
It does not review or complete the unavailable historical 05a5 patch/worktree. Existing parent
ownership/history remain intact. The focused controls below passed; complete integrated and
physical target-host acceptance remain separate. Editor milestone remains 0/8.

## Production scope and limits

`Engine/Window/src/X11Window.cpp` requests detectable auto-repeat on its own client Display.
Per-window held-key tracking emits physical transitions once while every repeated native press
continues through the existing XIM/UTF-8 text path. Focus loss and destruction erase held state.
The request result and returned support flag must both succeed before detectable repeat is
considered active. Only clients without active detectable repeat use the legacy fallback, which
recognizes a strict genuine adjacent release/press pair already queued
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

## Historical bc4 integrated pass

The following frozen-source result belongs to published `bc4e8cecc7273dcf4615e8881e95239ac4f9cd1b`;
it predates the detectable-repeat boundary correction below.

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

Historical bc4 configured features and six binary hashes are recorded in the
[final integrated identity table](EditorEDM0-GlyphCoverage-Linux-2026-10-22.md#final-integrated-binary-identities).


## Detectable-repeat boundary correction

The historical bc4 implementation requested detectable repeat but discarded its result/support,
then applied the legacy equal-timestamp release/press heuristic unconditionally. A client with
active detectable repeat has no legacy fake releases to remove. Genuine rapid physical taps can
share the same server millisecond, so this incorrectly collapsed 20 real B taps to one physical
press/release while preserving all 20 text commits. The correction records request success AND
support and gates the legacy heuristic on detectable repeat being inactive. Public interfaces,
repeat rates, text translation, focus reset, close lifetime and frame contracts are unchanged.

The same newly built probe and observer run against the preserved bc4 Window library and the
corrected Window library on owned Xvfb servers. The normal-mode burst confirms consumer SIGSTOP
with bounded waitpid/WUNTRACED, queues 20 genuine XTEST press/release pairs, XSyncs before resume,
and always SIGCONTs in finally. No SendEvent or timestamp rewriting is used. Both runs recorded
zero synthetic B events and confirmed the stop. Normal mode must observe at least one genuine
adjacent release/press pair with equal server timestamps, all 20 physical pairs and 20 text commits.
The legacy control retains every previous N-hold, M-focus, capability, close and global-repeat
oracle. Its ambiguous rapid-tap burst is not applicable; the legacy test still passes without
CTest skipping. Limits remain 45 s CTest, 12 s probe and 1 MiB per collected stream.

| Library / mode | Raw B press / release | Equal-time adjacent B pairs | Physical B down / up / text | Result |
| --- | --- | --- | --- | --- |
| negative-old-library / normal | 20 / 20 | 19 | 1 / 1 / 20 | FAIL: rapid_burst_ok only |
| negative-old-library / legacy | not applicable | not applicable | 0 / 0 / 0 | PASS |
| positive-fixed-library / normal | 20 / 20 | 19 | 20 / 20 / 20 | PASS |
| positive-fixed-library / legacy | not applicable | not applicable | 0 / 0 / 0 | PASS |

Old-library wrapper exits **1**, with only normal `rapid_burst_ok` false; corrected-library wrapper
exits **0**. All four attempts retain N physical **1/1**, text **25**, M **2/1**, global repeat
**1/1**, and passing text, native-repeat, focus, lifecycle and capability oracles. Normal raw N is
25/1; injected legacy raw N is 25/25. The new focused matcher/native CTest gate passed **2/2,
zero skips, 1.92 s**. This detects the boundary regression using the same new probe ELF on both
libraries; the earlier standalone proof is retained as discovery evidence, not substituted for
this confirmed-stop control.

| Artifact | SHA256 |
| --- | --- |
| Preserved bc4 Window library | `868bef269e139c23bd043a8c5b52db1a4ae949fe84aea2de29ffe71a691ea62c` |
| Corrected Window library | `d4d9394a62f937fd2ea56fe6aa0e91ced01f15db82ea2aa0644bdd1ed84a6427` |
| Same new probe on both libraries | `846e869e0fc9e341411cde60280fe7ee677e216b9cf6a2321f8dc276ddf4ee62` |
| Same unchanged observer on both libraries | `d2310c507d1aad6055bd4d5f567846efa1f8d9561688dcf3a1b8ad92ab04d183` |

Artifact root: `/workspace/.task-state/imgui/x11-detectable-boundary`. `candidate-native-status.json` records wrapper status and binary hashes.
The actual reports retained for this table are:

- `negative-old-library/attempt-2jgxd8_e/normal/report.json` SHA256 `1ca7bbb7b0223a115a46dd5d8fd9f1e24f9f8121463493eac0c211a4dea3ec01`.
- `negative-old-library/attempt-2jgxd8_e/legacy/report.json` SHA256 `ed3a549f5e02e563831435afb8d76e5476f606da959debccb026d34a2d97c95e`.
- `positive-fixed-library/attempt-q71fq1to/normal/report.json` SHA256 `0195a3f683cad545fa00b503133d167cfc921d14f67097f780ade1945fc212e0`.
- `positive-fixed-library/attempt-q71fq1to/legacy/report.json` SHA256 `6a877d84876caba9597cf3357946fc5bb519999f291e2a686316e0eea80660bb`.

Discovery report `attempt2/report.json` SHA256:
`7cc3677daf38066e07abb953906a78f3cc18a2b4bf5d0c4e89fcc2131cebdb66`.
Its standalone stop request did not separately acknowledge the stopped state; the same-new-probe
controls above supply that stronger stop acknowledgment and retain all previous oracles.

<!-- detectable-input-build:start -->
Latest-source Shipping build: **PASS**, status 0; changed `X11Window.cpp.o` compiled.
Wall duration was not separately recorded and is not inferred. Actual status:
`/workspace/.task-state/imgui/x11-detectable-boundary/shipping-status.json`; retained log:
`/workspace/.task-state/imgui/x11-detectable-boundary/shipping-build.log`.
<!-- detectable-input-build:end -->

<!-- detectable-input-result:start -->
Recorded UTC: `2026-10-23T01:58:13.772803+00:00`.
Corrected detectable-repeat configure/build/CTest: **PASS 368/368, zero failures/skips, 1646.65 s**.
Source: `be8f20a1e65f344f73809bdf3e30b6f3d4b085a7 plus three-file detectable-repeat correction; source manifest f8245e0f90c68d0fa6319bd3c072d51dcdc3ea1e6402b1b02d5344cef072e7ec`. All 5,535 frozen source/asset/configuration
hashes match both the completed gate comparison and this finalizer check. Source-manifest
SHA256: `f8245e0f90c68d0fa6319bd3c072d51dcdc3ea1e6402b1b02d5344cef072e7ec`. Archive prefix: `/workspace/.task-state/imgui/combined-detectable-input-main-standard`.
The historical bc4 pass and earlier incomplete/failed attempts remain separate records.
Exact-head CI is a separate merge gate; target-host runtime, physical display/DPI/installed-IME
and overall Editor 0/8 acceptance remain open.
<!-- detectable-input-result:end -->

## Final integrated binary identity check

After integrating upstream main and rebuilding, all six executable/library hashes were re-read
from the actual build. The corrected Window library and new probe still match the two hashes
in the table above. Editor ELF `809aed95f99f3bf159a2058309616977dddc43f23d387d7b7c5f3c31999c5a20`,
EditorImGui library `d792a0e46b7846416026b73dca7291d6d5d5236abf6f40c48c315e7862cc68d7`,
FontGlyph ELF `44fa5cf4f09a60ad85ecb6429248402e4b7d76b073473fb51253849de3c3f11a` and
observer `d2310c507d1aad6055bd4d5f567846efa1f8d9561688dcf3a1b8ad92ab04d183` remain unchanged.
The retained identity record is `/workspace/.task-state/imgui/combined-detectable-input-main-final-identities.json`.
Configuration remains Modular Development with testing, Editor, 2D, graphical shell, native/Vulkan
backends and Window/Presentation enabled; hierarchy timeout scale is 1. This identity comparison
is distinct from the complete-suite outcome recorded above.

## Latest main integration

Upstream `50c723888b9aff8246047a9bd0f881cdac9c93e0` independently fixes native Windows Alt+F4
handling (PR700). It was merged into this branch before final validation; its code and acceptance
record remain attributed to that change. The earlier corrected-source complete attempt
`combined-detectable-input-standard` was intentionally interrupted during test 283 after 282
passed, to include this newer main. CTest status -2, wrapper 667.615 s; all 5,535 frozen hashes
remained unchanged. That archive is retained and is not a complete pass. The final integrated
source uses the distinct `combined-detectable-input-main-standard` archive above.
