# ED-WF-08 Source animation foundation

Date: 2026-11-13 (UTC). Supporting task: `nexora-whr.8.1`.

## Result and boundary

Source-bound Sprites can play source UUID frames through the existing gameplay Clip animation
services. Every source frame resolves its own trim, Pivot and Atlas region while retaining World,
style and sort anchors. Legacy Clip schema-1 bytes and C/Zig table layouts remain compatible.
Source Clips use a separately versioned schema 2; C++ consumers rebuild for appended optional fields.
See the [format and ownership contract](../../../../Engine/TwoD/README.md#source-animation-clips).

Full Linux Development passes **523/523**, zero failures/skips,
**1869.01s**. Actual Shipping native animation and embedded ND2D admission, Monolithic
consumers and TwoD-OFF/testing-OFF builds also pass. The [machine receipt](ED-WF-08-SourceAnimation-Cloud.json)
records every Development result, all serial exit codes, frozen source hashes, caches, retained
attempts and native artifact hashes. These Linux software Vulkan/virtual-display results do not
claim physical-display, Windows/macOS or mobile execution. Hosted CI is a separate exact-head gate.

This is the source animation foundation. Live source reimport, 32 real Zig reloads with failed
candidate rollback, main Editor configured package Play and combined Editor/Player acceptance
remain open. ED-WF-07/08 remain open; workflow **5/9**, graphical Editor **0/8**, TwoD **9/11** and
physical/clean-host gates are unchanged.

## Implemented contracts

- A Clip exclusively uses legacy Atlas/region frames or source UUID frames. Source mode requires
  zero Atlas/region fields and nonzero, non-self source UUIDs; malformed/mixed/truncated data reject.
  Existing frame/event/duration/payload limits remain enforced. An independently compiled old
  encoder supplies the frozen 98-byte legacy baseline, SHA-256
  `460c26e3764d42200e822af28fb239af2e081ad3269cc17c535ef8dc5727174a`.
- Import, cook, asynchronous catalog admission and immutable Editor snapshots require the exact
  sorted unique source dependency list and validated source/Atlas closure. Explicit and embedded
  Player startup admit Atlas, then sources, then Clip, retaining conflict checks before callbacks.
- Source-bound and legacy Sprites require matching Clip modes. Explicit Advance preserves style
  and transforms; count-only and short event buffers never consume the animation clock. The
  unchanged animation result reports region zero; the source API returns the selected UUID.
- Source playback retains its exact immutable Clip owner and validated Animator copy. Replacing
  the registry Clip changes future Play calls; existing playback continues its original frames.
  Active and retained retired Clips protect all their source dependencies until the last owner
  releases. Source/Atlas publication stays on the owner thread.
- ND2D validation, Undo/Redo/reopen and Studio source-frame append/preview use the owning catalog.
  New-document Studio CLI startup imports Atlas, source, then Clip; actual native save/read-only
  reopen retains embedded source and Clip bytes without modifying input assets.
  Missing dependencies reject atomically. Manifest compilation supports source frames and keeps
  legacy manifests deterministic. Source pool publication uses existing atomic batch APIs.

## Executed acceptance

The CPU regression verifies independent legacy bytes, schema/mode/UUID rejection, canonical cooked
edges, asynchronous failed admission, exact event boundaries, pause/speed/end behavior, style edits,
probe atomicity and retained Clip dependency protection. Editor capture retains source/Atlas/Clip
owners after the generation store is destroyed. ND2D history and actual ImGui controls retain
source UUIDs without mutating authoring state during preview.

The canonical SDK Zig module runs from a moved, source-deleted package. Its explicit preloaded
catalog is deliberately in reverse dependency order. Both Development and Shipping report
30 native frames, 60 GPU instances, two visible Sprites, balanced ownership and stable pool handles.
Five independently calculated asymmetric pixel phases cover trim/Pivot, X/Y/both flips, negative
scale and animation selecting a second source/Atlas: region 901 to 73 and Atlas 4×4 to 8×8.
The retained pixel-oracle label `repacked` means this second preloaded animation frame; it does
not mean live reimport/repack. Package hashes and authoring-source deletion remain asserted.

The actual Shipping embedded-source acceptance additionally uses the Development authored fixture
and compiler with `AuthoredSourceAcceptance.py --native --shipping`; it verifies Clip admission,
relocation and native pixels. It does not claim a Shipping Editor executable.

```bash
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development --output-on-failure
cmake --preset linux-shipping -DNEXORA_SHIPPING_PROFILE=Full -DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_EDITOR_SDK=ON -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_ZIG_EXECUTABLE=/workspace/work/engine-goal-20261112/tools/lib/python3.12/site-packages/ziglang/zig -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-shipping
python3 Tests/TwoD/SpriteSourcePlayerAcceptance.py --app build/linux-shipping/Apps/TwoDPlayer/NexoraTwoDPlayer --zig /workspace/work/engine-goal-20261112/tools/lib/python3.12/site-packages/ziglang/zig --cmake /workspace/work/engine-goal-20261112/tools/lib/python3.12/site-packages/cmake/data/bin/cmake --compiler build/linux-development/Tools/TwoD/NexoraTwoDAssetCompiler --catalog --animation --native --shipping --evidence-dir build/linux-shipping/artifacts/source-animation-player
cmake --preset linux-development -B build/source-animation-monolithic -DNEXORA_LINK_MODE=Monolithic -DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_SLANG=OFF -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF
cmake --build build/source-animation-monolithic --target NexoraSourceAnimationTests NexoraTwoDPlayAssetTests NexoraTwoDAuthoringTests NexoraTwoDPanelTests
ctest --test-dir build/source-animation-monolithic -R '^twod\.(source_animation|play_assets|authoring_contracts|graphical_controls)$' --output-on-failure
cmake --preset linux-development -B build/source-animation-feature-off -DNEXORA_ENABLE_2D=OFF -DBUILD_TESTING=OFF -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_SLANG=OFF -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build build/source-animation-feature-off --target NexoraEditor NexoraProjectPlayer
python3 Tools/Build/ValidateApiM6.py .
python3 Tools/Build/TestDocumentationCI.py
```

The committed CTest registration supplies actual SDK/CMake/compiler paths for Development native
animation. The machine receipt hashes the serial runner and additional Shipping authored command,
including their exact absolute paths. No machine-specific presets or generated outputs are committed.

## Retained failures

Initial fixture failures used a different FNV seed, invisible/wrong-position pixel fixtures or an
invalid zero-size C style. The asynchronous catalog fixture initially omitted ABI version 1.
Those failed logs remain separate; fixture corrections did not weaken product admission or oracles.
An initial 92-case partial ended 91 passes/one fixture failure, exit 130. A later partial ended
252 passes/zero completed failures, exit 130 when review found embedded source Clips were admitted
before their sources. The actual old Player reproduced that admission failure in 0.94s. Moving
source admission before Clip retained conflict checks; corrected CPU/native/export/Studio tests
passed 7/7 in 9.30s before the final full gate.

A complete earlier 522-case gate ended 520 passes/two failures, zero skips, exit 8. Its Xvfb
negative control assumed one pipe read contained a full readiness line. The shared correction,
inherited from Build candidate `1fa40784`, accumulates fragments under the unchanged ten-second
deadline and asserts actual fragmented transport, default reset and retained display lifetimes.
The other failure was a Vulkan mesh residency submit. Its failed frame/streams and diagnostic
probes remain retained; open `nexora-ko9` tracks its unconfirmed cause. No renderer or residency
assertion changed. Later strict passes do not establish a root-cause fix.

The final implementation integrates main `2e900aa2` and Build candidate `1fa40784`. An interrupted
old-base rerun (three completed passes, exit 130) remains historical evidence. The new complete
523-case result above is the acceptance gate for this combined source generation.
Build PR #779 was subsequently squash merged as `2f0c2834` after all 41 exact-head hosted jobs
passed. Its tree `a6786d275f12f520591ca5d543fb2e71460e9700` equals the tested `1fa40784` base;
the machine receipt verifies both Git tree identities. Source branch alignment preserves the
validated feature bytes.

Another actual Studio startup probe rejected a valid source Clip because the existing CLI imported
Clip before the supplied source. Its pre-fix binary/source hashes and rejection log remain retained.
The then-running full gate was intentionally interrupted after 59 completed passes, zero completed
failures, actual CTest/runner exit 130. The final order is Atlas, source, Clip. The existing native
authored-source acceptance now imports into a new Studio document, saves the exact embedded source
and Clip bytes, read-only reopens it and checks every input asset remains unchanged. A new complete
gate follows this repair; the 59-case partial is historical evidence.
