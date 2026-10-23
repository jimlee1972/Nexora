# ED-M0 bounded host-font coverage — Linux validation

The implementation is applied and compiled, and `editor.font_glyphs` passed in 0.21 s after baseline correction (the initial run was 0.15 s).
The observations below retain the first compiled binary and the baseline-corrected repeats. The complete Linux suite and focused native results are recorded below; physical-host acceptance remains open.
ED-M0 remains open, the Editor milestone remains 0/8, and physical-display/DPI/installed-IME
acceptance is incomplete.

## Implemented contract

The host selects bounded installed or explicitly configured scalable font sources, owns font
plans/ranges across DPI builds and reports actual atlas misses through `FindGlyphNoFallback`.
TrueType `glyf` and CFF1/Type2 use recognized structural, payload, depth and charstring bounds
before the pinned rasterizer. This is a compatibility preflight for trusted host/configured
sources, not a general hostile-font sanitizer or universal OpenType implementation. Bitmap/color/
SVG-only data without supported scalable outlines are rejected. Windows may probe `seguiemj.ttf`
before `seguisym.ttf`; no Windows execution, color rendering or available sample ink is inferred.

The fixed profile unions pinned SimplifiedCommon, 5401 generated Traditional common scalars,
Bopomofo/punctuation and U+1F600/U+1F642: 7182 unique scalars under the hard 8192 cap. Generated
Traditional data use standard-library CP950 strict decoding; their canonical scalar-set SHA256 is
`85e69c33a16e5c0879fa743c72bac671fed9133ad53fa73275d2492ac9af2006`.
The conditional Windows comparison to `MultiByteToWideChar` remains unexecuted.

A TTC's 10 declared faces are not the same as the 4-admitted-face cap. TC selection checks family
and outline coverage; it does not hardcode face 0 or accept face 3 merely from its index.
Limits are distinct: 16 candidate paths, 32 MiB per font file, 64 MiB admitted source bytes,
8192 requested scalars, conservative pre-Build packing admission, completed atlas at most
4096×4096 and one RGBA upload at most 64 MiB. Pinned ImGui copies input font data per atlas even
with `FontDataOwnedByAtlas=false`. Source cache, per-atlas copies, temporary work, Alpha8/RGBA,
GPU and retired resources can coexist. **64 MiB is not total pipeline peak memory.** A failed or
oversized atlas uses Latin-only fallback before upload; existing generation/domain/completion
contracts remain in use and require the broader regressions.

Coverage subtraction uses actual `FindGlyphNoFallback` coverage from a process-once independent
default atlas, rather than assuming U+0020–U+00FF are all present. Probe time includes that first
baseline-atlas build. A synthetic U+0080 alias regression checks a coverage hole and compares
actual cropped ASCII raster hashes at every tested DPI; it is a control-code fixture, not a
control-font support promise. All 9 authored font fixture files plus the 5401-scalar profile regenerate identically;
the final regenerated source hashes below come from the validated repository.

Original synthetic TrueType and CFF fixtures verify real lookup, UV and nonempty/distinct raster,
missing members, DPI generation/raster round trips, Unicode paths/owned bytes, TC selection and
recognized-invalid/bitmap/profile/packing rejection. These authored shapes are pipeline fixtures,
not readable Latin, Chinese or emoji typography. Legitimate mapped blanks may have advance with
no ink; the visible emoji samples receive no such exception.

## Recorded runs and remaining checks

<!-- glyph-validation-results:start -->

Portable `editor.font_glyphs`: **PASS**, 0.21 s.
Baseline-corrected binary; focused portable regression passed. Frozen-source, built-artifact and raw-log identities are recorded below; the final PR identifies the commit.

| Check | Actual status |
| --- | --- |
| Source / binary identities and exact argv | Frozen-source manifest and built artifacts recorded below; final PR supplies commit identity |
| Official monochrome configured source repeat | Observed after baseline correction; U+1F642 ink 73, raw missing 53 |
| Fixture/profile regeneration | 10 files reproduced identically; exact updated hashes recorded below |
| Complete unfiltered Linux suite | PASS 366/366, zero failures/skips, 1570.09 s |
| Additional Monolithic Development font build/test | PASS; actual font test 1/1, 0.21 s |
| Shipping engine configure/build | PASS; Editor/font targets excluded, not font linkage evidence |
| Actual native workflows and original deadlines | Five focused tests PASS; preview 43.14 s under unchanged 45 s bound; full native set also passes |
| Existing renderer generation/domain/completion regressions | PASS in focused and complete suite |
| Windows conditional exact CP950 comparison | Not run on Windows |
| Windows/macOS source and runtime checks | Not run |
| Physical Linux/Windows DPI/installed IME/keyboard recovery | Open; Editor milestone 0/8 |

| Diagnostic observation | Sources / profile / raw missing | Atlas / admitted source bytes | Probe / build | Wall / peak RSS |
| --- | --- | --- | --- | --- |
| initial-linux-default | 2 / 7182 / 54 | 4096×256 / 20244504 bytes | 676703 / 347252 µs | 1.043 s / 58433536 bytes |
| initial-linux-configured-emoji | 3 / 7182 / 53 | 4096×256 / 22227100 bytes | 509338 / 363286 µs | Not recorded / Not recorded |
| baseline-corrected-linux-default | 2 / 7182 / 54 | 4096×256 / 20244504 bytes | 679116 / 354185 µs | Not recorded / Not recorded |
| baseline-corrected-linux-configured-emoji | 3 / 7182 / 53 | 4096×256 / 22227100 bytes | 501341 / 357521 µs | Not recorded / Not recorded |

**initial-linux-default:** Installed Linux defaults; Noto Sans CJK TC face 3 + DejaVu Sans face 0.
First compiled binary; retain as initial observation and repeat after baseline correction.
Raw missing categories: Cc=1, Cf=25, Zl=1, Zp=1, Cn=24, Lo=1, So=1.
Actual sample ink-pixel counts: U+0041=49, U+4E2D=37, U+1F600=88, U+1F642=0.

**initial-linux-configured-emoji:** Explicit official Noto Emoji face 0 + installed Noto Sans CJK TC face 3 + DejaVu Sans face 0.
Same first compiled binary; retain as initial observation and repeat after baseline correction.
Raw missing categories: Cc=1, Cf=25, Zl=1, Zp=1, Cn=24, Lo=1.
Actual sample ink-pixel counts: U+0041=49, U+4E2D=37, U+1F600=84, U+1F642=73.

**baseline-corrected-linux-default:** Installed Linux defaults; Noto Sans CJK TC face 3 + DejaVu Sans face 0.
Baseline-corrected repeat; sample ink and raw misses unchanged. Built-artifact and raw-log identities are recorded below.
Raw missing categories: Cc=1, Cf=25, Zl=1, Zp=1, Cn=24, Lo=1, So=1.
Actual sample ink-pixel counts: U+0041=49, U+4E2D=37, U+1F600=88, U+1F642=0.

**baseline-corrected-linux-configured-emoji:** Explicit official Noto Emoji face 0 + installed Noto Sans CJK TC face 3 + DejaVu Sans face 0.
Baseline-corrected repeat; sample ink and raw misses unchanged. Built-artifact and raw-log identities are recorded below.
Raw missing categories: Cc=1, Cf=25, Zl=1, Zp=1, Cn=24, Lo=1.
Actual sample ink-pixel counts: U+0041=49, U+4E2D=37, U+1F600=84, U+1F642=73.

The configured source was obtained outside the repository under OFL-1.1 from
`google/fonts` commit `8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5`. It is not bundled by this change.

| External file | Bytes | SHA256 | Original URL |
| --- | --- | --- | --- |
| `NotoEmoji[wght].ttf` | 1982596 | `de6c18832938afc99caf132b39d6a30a19bac7f2e812e28db2535b4608d27551` | https://raw.githubusercontent.com/google/fonts/8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5/ofl/notoemoji/NotoEmoji%5Bwght%5D.ttf |
| `OFL.txt` | 4330 | `500bb1ccf43df7bbb522112f9133a52b16e1c35e809632f5d8609b179152de5b` | https://raw.githubusercontent.com/google/fonts/8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5/ofl/notoemoji/OFL.txt |

These diagnostic observations do not establish full profile coverage, physical readability
or color-font support. Complete-suite/native results are recorded separately below.

<!-- glyph-validation-results:end -->

The missing categories are reported literally: Cc control, Cf format, Zl line separator, Zp
paragraph separator, Cn unassigned, Lo other-letter (here U+FFA0 filler), and So symbol (here
U+1F642 in the default run). Do not relabel every miss as a missing visible Chinese character,
ignore the missing default smiley, or claim complete profile coverage from a diagnostic exit code.
The configured source's 53 raw misses likewise remain recorded rather than converted to 0.

`NEXORA_EDITOR_FONT_DIAGNOSTICS=1` provides normal-host stderr profile/missing/fallback/sample
availability, source bytes, atlas extent and separate probe/build counters. Private copied getters
add actual identities/raster data. This is diagnostic observability, not a new GUI font-settings
flow. Probe/build times exclude other startup work. A diagnostic process's peak RSS and wall time
do not replace native Editor measurements, and source/atlas bytes are not total RSS.

## Exact source and reproduction record

The final PR identifies the committed tree. The source manifest, built artifacts, installed source
hashes, exact commands and retained log prefixes below identify the locally validated source.
Host: Linux 6.18.55 x86_64, GCC 14.2, CMake 4.4.4, Ninja 1.13.2.kitware, Xvfb/Mesa lavapipe.
The frozen Development gate starts on 2026-10-22 UTC; its status file records exact durations.

```bash
cmake --preset linux-development
cmake --build --preset linux-development --target NexoraFontGlyphTests
ctest --preset linux-development -R '^editor\.font_glyphs$' --output-on-failure
build/linux-development/Tests/EditorImGui/NexoraFontGlyphTests --system
cmake --build --preset linux-development
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
cmake --preset linux-development -B build/linux-font-monolithic -DNEXORA_LINK_MODE=Monolithic -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DBUILD_TESTING=ON -DNEXORA_ENABLE_NATIVE_BACKENDS=ON -DNEXORA_ENABLE_VULKAN_BACKEND=ON
cmake --build build/linux-font-monolithic --target NexoraFontGlyphTests
ctest --test-dir build/linux-font-monolithic -R '^editor\.font_glyphs$' --output-on-failure
```

The listed configure/build/test commands ran with exit 0. The system command is diagnostic only,
and ran both with default sources and with NEXORA_EDITOR_FONT_PATHS pointing to the external OFL
Noto Emoji file recorded above. The ordinary Development command assumes its existing
cache enables graphical/testing; a fresh configuration must explicitly pass
`-DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DBUILD_TESTING=ON`. Native/Vulkan are explicit in the
fresh Monolithic command for reproduction. Archive actual graphical/testing/native/Vulkan/2D and
link-mode cache values. Shipping forces Editor OFF and therefore cannot validate font linkage;
the additional Monolithic Development test exercises static Dear ImGui ownership without changing
Shipping policy, CI or presets.

Native preview previously measured 44.11 s under its unchanged 45 s bound on an earlier input tree.
Cold source/atlas costs must be measured in the actual native writer/reader workflows, not inferred
from the diagnostic process. Do not remove Traditional coverage, weaken data/pixel assertions,
change lifecycle cycles or widen deadlines to manufacture acceptance.

## Physical acceptance remains separate

Linux physical GPU/display readability, real typing/docking/wheel/focus loss, resize/minimize/
restore and keyboard-only recovery/failure/retry remain open. Windows 100/125/150/200%, repeated
cross-monitor moves, installed-IME composition/update/commit/cancel, caret movement and candidate
placement at multiple DPI remain open. A cloud glyph pass does not close these rows or ED-M7
screen-reader support. Retain operator artifacts through the target-host evidence schema rather
than substitute synthetic glyphs or virtual displays.

## Focused native and baseline regression evidence

The baseline-corrected focused gate passed all five tests with no skip in 68.57 s:
`editor.imgui_contract` 9.74 s, `editor.imgui_context_lifetime` 1.60 s,
`editor.linux_native_scene_preview` 43.14 s under its unchanged 45 s deadline,
`editor.pointer_dpi` 5.29 s and `editor.native_surface_lifetime` 8.78 s.
Raw log: `glyph-native-focused.log` under the external task-state directory.
These Xvfb/lavapipe results do not accept physical readability or IME behavior.

A controlled regression restored only the old `scalar > 0xFF` planning filter, rebuilt
`NexoraEditorImGui` and ran the same portable font-test ELF:
`44fa5cf4f09a60ad85ecb6429248402e4b7d76b073473fb51253849de3c3f11a`.
The old-filter library failed with exit 1 and
“Actual missing default-range glyph was excluded instead of merged from the source”.
Restoring the corrected source and rebuilding the library passed with exit 0;
the test ELF hash was unchanged. The synthetic U+0080 mapping is a deliberate fixture
alias to exercise this gap, not a claim about control-code rendering.
The old-filter negative and restored positive logs and status are retained as
`glyph-baseline-old-filter-negative.log`, `glyph-baseline-restored-positive.log`
and `glyph-baseline-negative-status.json`. The negative experiment was completed and
all source restored before the complete-suite source freeze.

## Historical complete Linux gate (PR699)

`cmake --preset linux-development`, `cmake --build --preset linux-development` and
`ctest --preset linux-development` all pass: **366/366, zero failures/skips, 1570.09 s**.
Configure takes 1.669 s and the full build 82.665 s.
All 5,530 frozen source/asset/configuration files match after the suite;
source manifest SHA256 is `208a331f5cf87f8f7220efe724d8d5aee32c566adc84f09e162f0bd3fa57cd95`.
The raw complete logs/status/LastTest and source manifest use the immutable external archive
prefix `glyph-complete-standard`. Markdown changes after the gate do not alter these frozen source files.

Ordinary Development uses Modular linkage; the additional fresh
`linux-font-monolithic` Development directory explicitly enables graphical shell, native
backends, Vulkan and testing, and passes the actual font test. Both configurations enable 2D.
Shipping configure/build also pass with Monolithic linkage, Editor OFF and testing OFF by policy;
that engine pass is not font-boundary acceptance. No Shipping or CI policy was changed.

The first font build failed because the math-operator define came after imgui.h and a segment
count inferred a signed type. Those errors were corrected before the focused and complete gates.
Initial observations and the deliberate old-filter negative remain historical evidence,
not failed runs relabeled as passes. Fresh final-head CI and verified own-PR merge remain separate
delivery gates; physical Linux/Windows/macOS behavior and the overall 0/8 milestone stay open.

| Built artifact | SHA256 |
| --- | --- |
| `Tests/EditorImGui/NexoraFontGlyphTests` | `44fa5cf4f09a60ad85ecb6429248402e4b7d76b073473fb51253849de3c3f11a` |
| `Engine/EditorImGui/libNexoraEditorImGui.so` | `5c83803c279e6d0b5fdddb2884c23333e4212ec582fc245676e85e0e1d90bdd9` |
| `Engine/EditorImGui/libNexoraDearImGui.so` | `aed85b0d2a4408c5016e07cf724cf6f4b12da2af1d3fc3b25ad450f5f5a55948` |
| `Apps/Editor/NexoraEditor` | `a584bc2be484e009f231b43316358d67598f8abf59b9def5eaba61ba8526a095` |

Installed source identities (font files remain outside the repository):

| Source | Bytes | SHA256 |
| --- | --- | --- |
| `/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc` | 19484784 | `b76b0433203017ca80401b2ee0dd69350349871c4b19d504c34dbdd80541690a` |
| `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf` | 759720 | `57f73e11f51999432bf7ab22ce55b6f945d5eca1bf824404cfa9ec2e3718c84e` |
| `/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf` | 11260748 | `dac5c27651082d6c53dab5081f50d2022ddf6877d730fa919cf6a4fc2af22de0` |

Reproduced original fixtures and generated profile:

| File | SHA256 |
| --- | --- |
| `Tests/EditorImGui/Fixtures/NexoraAtlasAdmission.ttf` | `d483ecd5bbc407d06b4e31b768ebec382bcd6c82d79709ca83377bf2d117c449` |
| `Tests/EditorImGui/Fixtures/NexoraBitmapOnlyContainer.ttf` | `f8504a9fd7d23a03996749b9a4e86b3b120f234cbb6432033dad4c52baab45cb` |
| `Tests/EditorImGui/Fixtures/NexoraInvalidCff.otf` | `5bf1087a079ca9933f08ff146c882bfd50dd8d44a7808cc12834542572ec8121` |
| `Tests/EditorImGui/Fixtures/NexoraInvalidCmap.ttf` | `7757f30e04ef5d6de18b6fceecab4032c5bd0225add074c771114c0fed45b062` |
| `Tests/EditorImGui/Fixtures/NexoraInvalidCompoundDepth.ttf` | `5d5254a418ec2d4b2aa0b0c109dec5a4caaf03558275ec85e7c5e976785264ed` |
| `Tests/EditorImGui/Fixtures/NexoraInvalidContours.ttf` | `5fb60d07c9b42f351c936fef5bdec1fb8df6a2f311d1762e36c4d1a959d19bc7` |
| `Tests/EditorImGui/Fixtures/NexoraSyntheticCff.otf` | `0fc469297a933fa1cc70e4f5f75fa4d30e5454864c4774590c17b315e974c1c7` |
| `Tests/EditorImGui/Fixtures/NexoraSyntheticFaces.ttc` | `911638b83a7d3312ac1d10bf493e7071fb2539c18f6a9bd7138545104b8bc3db` |
| `Tests/EditorImGui/Fixtures/NexoraSyntheticGlyphTest.ttf` | `6930b3f0e6193a9949e7598d286642455536c2591073bda194cfed67f4b2ea75` |
| `Engine/EditorImGui/src/TraditionalCommonCp950Scalars.inc` | `155fe83f83127974f1ed85690542ace5baa9aaff6fe197fe9f7d1f5f616874bd` |

## Final opt-in diagnostics and fresh-process measurement

After the complete gate, the real host diagnostics branch was enabled with
`NEXORA_EDITOR_FONT_DIAGNOSTICS=1` for each separate font-test process.
Each process exits 0 and its stderr record agrees with actual atlas lookup.
The configured process additionally sets `NEXORA_EDITOR_FONT_PATHS` to the external
OFL font path recorded above. OS file caching is uncontrolled; these are fresh
process plans, not cold-disk benchmarks or native Editor GPU/RSS measurements.

| Source mode | Probe / build µs | Process wall s | Child peak RSS bytes | Raw missing |
| --- | --- | --- | --- | --- |
| default | 493912 / 354134 | 0.866126 | 58540032 | 54 |
| configured | 506167 / 355032 | 0.874168 | 60477440 | 53 |

Raw diagnostic/status prefixes are `glyph-diagnostics-final-default` and
`glyph-diagnostics-final-configured`. The latter has actual ink for both emoji
samples; neither raw missing count is relabeled as zero. These observations do
not turn independent 64 MiB source/atlas limits into a total-memory guarantee.

Selected unchanged-deadline workflows from the complete suite:

| Test | Result / seconds |
| --- | --- |
| `editor.linux_native_play_soak` | Passed / 80.65 |
| `editor.linux_prefab_isolation` | Passed / 134.22 |
| `editor.linux_scene_files` | Passed / 61.45 |
| `editor.linux_native_scene_tabs` | Passed / 57.07 |
| `editor.linux_native_scene_preview` | Passed / 43.43 |
| `editor.linux_native_scene_center` | Passed / 32.23 |
| `editor.linux_build_process_console` | Passed / 11.45 |
| `editor.linux_prefab_placement_source_rebase` | Passed / 15.92 |
| `editor.linux_prefab_placement_source` | Passed / 7.64 |
| `editor.linux_native_game_twod` | Passed / 67.12 |

## Follow-up MSVC compilation and combined validation

The historical PR699 Linux 366/366 result above belongs to its frozen source and remains valid
for that source. It does not establish Windows compilation or acceptance of subsequent changes.
The old `7c294` revision failed MSVC compilation with C4459 (the `traditional` lambda parameter
shadowed the generated scalar table) and C4996 (insecure CRT environment access). The follow-up
renames the parameter and uses owned `_dupenv_s` / `_wdupenv_s` CRT copies, released with
`std::free`, while retaining the diagnostics presence policy and UTF-8/native-path conversion.
This is an applied compiler compatibility repair, not a Windows runtime/IME acceptance claim.
Fresh Windows CI compilation remains a separate required result.

The earlier X11 integration 368-test attempt was intentionally interrupted for this repair after
282 tests passed, with SIGINT during test 283: CTest status -2, 709.026 s. All 5,535 archived
source/asset/configuration hashes remained unchanged. This is **INTERRUPTED**, not a complete
pass or an attributed product failure. Its archive prefix is `x11-current/complete-standard`.

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

Physical Linux/Windows DPI/readability/installed-IME and target-host acceptance remain open;
the Editor milestone remains 0/8. Do not replace the historical 366 result with this pending run.

## Final integrated binary identities

The final Linux configuration is Modular Development with testing, graphical shell, native
backends, Vulkan and Window Presentation enabled. Exact cache/binary records are retained in
`/workspace/.task-state/imgui/combined-font-input-review-final-identities.json`.

| Binary | SHA256 |
| --- | --- |
| `NexoraEditor` | `809aed95f99f3bf159a2058309616977dddc43f23d387d7b7c5f3c31999c5a20` |
| `libNexoraEditorImGui.so` | `d792a0e46b7846416026b73dca7291d6d5d5236abf6f40c48c315e7862cc68d7` |
| `libNexoraWindow.so` | `868bef269e139c23bd043a8c5b52db1a4ae949fe84aea2de29ffe71a691ea62c` |
| `NexoraFontGlyphTests` | `44fa5cf4f09a60ad85ecb6429248402e4b7d76b073473fb51253849de3c3f11a` |
| `NexoraX11HeldKeyProbe` | `fd7a878f74dca35aa2396a020af56f9ee744c9c02f3d60f91134148755211ca5` |
| `libNexoraX11RepeatCapabilityControl.so` | `d2310c507d1aad6055bd4d5f567846efa1f8d9561688dcf3a1b8ad92ab04d183` |

Final complete-suite Preview/PointerButtons/Overrides/SelectedRevert passed in
28.73 / 7.58 / 14.36 / 25.60 s respectively, with the existing time limits and assertions.
These identities refer to the completed 368-test source; earlier controls retain their original
recorded binaries and are not relabeled.

## Detectable-repeat follow-up

The combined 368-test result and binary identities above belong to the historical bc4 source.
A subsequent X11 capability-boundary repair adds genuine rapid-tap preservation and its own
source-frozen complete gate. See the [latest X11 correction and validation](EditorEDM0-X11HeldKey-Linux-2026-10-22.md#detectable-repeat-boundary-correction)
for the changed Window/probe identities and final-source result; the historical records here
remain unchanged. Physical target-host acceptance remains open (Editor milestone 0/8).
