# ED-WF-07 main-scene Sprite source Build — Linux cloud execution

Historical validation for the original base is retained below. The latest main2e900 integration,
full521-test gate and native transport repairs are recorded in the
[main integration supplement](ED-WF-07-SourceBuild-MainIntegration-Linux-2026-11-13.md).

Date: 2026-11-13 (Asia/Taipei). Supporting task: `nexora-whr.7.7`.
Base: main `ac15a642` / Inspector PR #777. Independent module-free Play task 7.6 is preserved.

## Scope and boundaries

The actual saved-scene Game Build producer owns source/Atlas cooked closure and exact source,
artifact and mandatory sidecar baselines. It deduplicates shared sources across enabled scenes,
uses the existing final publication guard, and emits explicit game capability 3. Runtime validates
structural reference/dependency/integrity contracts without depending on optional TwoD. The typed
TwoD owner validates Atlas/source payloads and extracts remapped live World transforms. The native
runtime-only Player overlays these Sprites after existing 3D drawing and drains before owner release.

Source-free capability 2 bytes, StaticView behavior and stable C/Zig tables remain unchanged.
Appended C++ owning fields/APIs require consumers to rebuild. This slice does not advertise
gameplay Sprite/pool services or accept source animation/reload. Editor configured package Play
uses a different compositor and rejects source catalogs explicitly until its preview owner is
integrated. Main Scene View, full ED-WF-07, workflow 5/9, Editor 0/8, TwoD 9/11 and independent
physical/clean-host gates remain unchanged.

## Executed focused evidence

- Owning CPU capture checks exact raw source/sidecar drift, canonical shared multi-scene closure,
  corrupted/duplicate/missing/unsupported references, valid-hash malformed typed payloads and
  remapped additive extraction after all authoring owners/files are destroyed.
- The real Editor Build CLI produces the game through the canonical SDK Zig adapter. A separately
  exported source-free SceneOnly game supplies the native 3D baseline. Original project hashes stay
  unchanged; both source projects are deleted. The game is moved to a Unicode path and launched
  without arguments, resolving executable-relative `Game.launch`.
- Actual X11 window capture checks all 8192 Sprite pixels: 2048 each red/green/blue/yellow, confined
  to independently calculated crop/Pivot/mirror rectangles. A 6400-pixel central region exactly
  matches the independently built source-free native 3D baseline. Full screenshots are retained.
- Normal WM close checks actual Zig Start/Update/Stop/Destroy state/order and native drain, with
  unchanged runtime artifact hashes. The final complete gate below repeats these assertions.
- Independently compiling the pre-change `ac15a642` game encoder against the appended owning
  header produces identical source-free bytes: 1379 bytes, SHA-256
  `8a47cece20f4ed6c4d3a80e1ef74fe9ccae289c74e33bc05ac9581ceecbaa664`, FNV-1a-64
  `7df5c4a79b4fc8ed`. The CPU regression freezes that actual baseline.

## Retained attempts and corrections

Initial target selection failed because the reused cache had Project Player disabled. Existing
Player/native options were enabled explicitly; no successful Player build was claimed beforehand.
The first C++ compile caught a nonexistent Runtime `TypeId` alias; framing now uses its actual
uint64 type. A first test registration used a nonexistent Vulkan option; the existing
`NEXORA_ENABLE_VULKAN_BACKEND` option now registers the native case. An unregistered case was not
counted as native acceptance.

The first source-only native case passed 2/2 in 9.12 seconds. A stronger mixed 3D/Sprite case then
failed 38.77 seconds because the fixture assumed neutral PBR must produce identical RGB channels;
the actual center was (65,63,57). That failure and screenshot remain retained. The replacement
oracle independently builds the same 3D scene without source records and requires exact unchanged
native 3D pixels, retaining every original Sprite position/color assertion. The corrected mixed
case passed 2/2 in 11.04 seconds.

The first ScriptProject execution failed 36.64 seconds at the existing tool-baseline guard because
the cloud Zig setting named a symbolic link. The cache now names the real Zig executable. Product
path/alias checks were not weakened. The corrected SDK/native case passed 2/2 in 50.44 seconds;
the final source generation additionally observes the SDK Destroy callback and bounds aggregate
capture payloads before cooking.

The first full gate was intentionally interrupted after one completed pass of 520 registered
cases, with zero completed failures and exit 130. Review found the new source-free Player path
would still capture World poses every frame. The final immutable asset owner returns an empty
frame immediately when it owns no sources, preserving legacy 3D frame cost. The partial log,
frozen hashes and exact prior source generation remain in `source-build-pre-empty-fast-path`;
they are not counted as a full pass. A fresh complete configure/build/test gate follows.

A second partial full gate was intentionally stopped after 28 completed passes, zero completed
failures and exit 130. The unsupported configured package Play check needed to run before its
throwing context constructor, keeping the native Editor alive with a visible status. Exact prior
source/logs remain in `source-build-pre-play-guard`. The new GUI probe first failed 44.15 seconds
because its missing managed Main.scene created a dirty starter scene, entering the existing
saved-scene decision before Play admission. That attempt remains retained. A clean saved fixture
now observes the real rejection at stopped state 0, twelve actual native presents and unchanged
read-only project bytes; corrected CPU/native cases passed 2/2 in 51.22 seconds. No dialog policy,
product path guard, original Sprite/3D pixel assertion or deadline was weakened.

## Final frozen-source validation

The final frozen 16 source/registration files pass:

| Gate | Actual result |
| --- | --- |
| Complete Development configure/build/CTest | **520/520**, zero failures/skips, **1819.95s** |
| Actual source Build/native GUI/relocated Player | 50.35s within that full inventory |
| Owning CPU source Build case | 0.03s within that full inventory |
| Existing shared Game Build / script preparation | 156.61s / 101.48s, both pass |
| Full Shipping configure/build | Exit 0; optional native Player enabled |
| Strict Shipping-runtime native acceptance | Exit 0; actual Shipping Player, Development Editor producer |
| Actual Monolithic owning consumer | 1/1, 0.02s (total 0.03s) |
| Actual 2D-OFF/testing-OFF graphical Editor and native Player build | Exit 0 |
| Actual 2D-OFF Player source refusal / legacy admission | 3 expected refusals / 2 successful admissions |
| Stable C API / documentation routing | 145 exports / 16 of 16 |
| Changed Markdown/local links/three language pairs; touched C++ formatting | Pass; root README unchanged |

The Development cache explicitly enables TwoD, graphical shell, Slang, Zig and native Project
Player. The physical Zig executable replaces the earlier symbolic-link setting. Final gate commands:

```sh
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development --output-on-failure
cmake --preset linux-shipping -DNEXORA_SHIPPING_PROFILE=Full -DNEXORA_ENABLE_EDITOR_SDK=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_ZIG_EXECUTABLE=/workspace/work/engine-goal-20261112/tools/lib/python3.12/site-packages/ziglang/zig -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-shipping
cmake --preset linux-development -B build/inspector-monolithic -DNEXORA_LINK_MODE=Monolithic -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_ENABLE_SLANG=OFF
cmake --build build/inspector-monolithic --target NexoraGameSpriteBuildTests
ctest --test-dir build/inspector-monolithic -R '^editor.game_sprite_build$' --output-on-failure
cmake --preset linux-development -B build/inspector-feature-off -DNEXORA_ENABLE_2D=OFF -DBUILD_TESTING=OFF -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_ENABLE_SLANG=OFF -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build build/inspector-feature-off --target NexoraEditor NexoraProjectPlayer
python3 Tools/Build/ValidateApiM6.py .
python3 Tools/Build/TestDocumentationCI.py
```

The native fixture's committed CTest command supplies the real Editor/Player/fixture paths,
Xvfb/xdotool and physical CMake/Ninja/Zig toolchain. The manual Shipping invocation uses the same
script and actual Shipping Player/profile from `build/linux-shipping`, with the actual Development
Editor producer and fixture; no Shipping Editor executable is claimed. All native attempts remain
under the respective build's `artifacts/editor-sprite-sources/build-*` directory.

The [complete receipt](ED-WF-07-SourceBuild-Cloud.json) contains every executed test, all final
source/cache/log hashes, distinct retained native generations and the exact native-vs-baseline
3D region hashes. Unsupported configured package Play is tracked separately as open
`nexora-whr.7.8`; its user-visible refusal keeps the actual stopped Editor alive.
Hosted results belong to the own delivery PR's immutable head and are recorded there after all
applicable checks succeed; local Linux results do not claim Windows/macOS native execution.
