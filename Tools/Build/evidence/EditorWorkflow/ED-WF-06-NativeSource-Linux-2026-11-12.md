# Real Zig Sprite source flips and Atlas repacks

Supporting task: `nexora-whr.6.4`, continuing merged catalog PR #770 (`cde8e8e5`).
This candidate adds acceptance coverage through the existing SDK/C ABI and common resolver;
no engine implementation, descriptor layout, schema, legacy consumer or existing assertion changes.
Full ED-WF-06 acceptance remains pending the complete gate, exact-head CI and corresponding merge.

The first hosted head passes all PR/SDK jobs and the new native fixture, but push Linux desktop
fails the existing legacy held-key case. The [fixture follow-up](ED-WF-06-X11Fixture-Linux-2026-11-12.md)
and [receipt](ED-WF-06-X11Fixture-Cloud.json) preserve that 509-pass/one-skip/one-failure gate,
establish the fallback's existing buffered-pair precondition without changing production Window
behavior, and record a fresh 486/486 full gate (1392.69 s) plus rebuilt strict Shipping native.
All five captures remain byte-identical. The original complete gate below is retained separately;
repaired-head hosted CI and merge are still required.

## Actual consumer and pixels

`twod.sprite_source_repack_player` builds `SpriteSourceRepackPlayer.zig` through the canonical
game-project CMake/SDK producer. Its initial raw Atlas and Sprite source are deleted before module
compilation. The game obtains source UUID `11:12` from the preloaded file catalog, then executes
after export, Unicode relocation and deletion of its authored directory. A corrupt cooked source
still rejects before Start. Package hashes remain unchanged after both CPU and native execution.

The source retains canvas 6x6, trim `(1,1,2,2)`, Pivot `(3,1)`, PPU 1 and Atlas UUID `8:9` / region 1.
The local quad `[-2,0] x [2,4]` mirrors around the source Pivot when flips change, while authored
world transforms stay fixed. One scene Sprite and a pool instance with negative X scale exercise
the independently negotiated source API. Every update reads back source identity, authored style,
pool transform and the original handle. No instance is recreated for the repack.

Five native captures check hand-authored quadrant colors and empty positions, including the
mirrored trim geometry. A half-pixel world offset aligns texel centers with readback pixel centers.

| Phase | Scene top-left/right, bottom-left/right | Negative-X pool | Geometry |
| --- | --- | --- | --- |
| Base | Red, green, blue, yellow | Green, red, yellow, blue | Original trim |
| Flip X | Green, red, yellow, blue | Red, green, blue, yellow | X mirrored about Pivot |
| Flip Y | Blue, yellow, red, green | Yellow, blue, green, red | Y mirrored about Pivot |
| Flip XY | Yellow, blue, green, red | Blue, yellow, red, green | Both axes mirrored |
| Repacked XY | Black, white, magenta, cyan | White, black, cyan, magenta | Unchanged from Flip XY |

The replacement Atlas keeps UUID/region identity and region size, changes texture extent from
4x4 to 8x8 and moves the packed rectangle from `(1,1)` to `(4,3)`. Different quadrant colors and
filler texels reject stale texture generations or stale UVs. Source metadata, descriptor flags,
world transforms and handles remain unchanged during this Atlas-only replacement. Existing
`twod.sprite_source` also retains its real `PackAtlas` producer regression and immutable-owner tests.

## Executed focused and Shipping checks

Development and strict Monolithic Minimal Shipping both pass the complete five-phase native
fixture using Vulkan under Xvfb, alongside CPU admission/extraction. Each native run requires
30 actual frames, all five captures, two visible instances and balanced module allocations.
The Shipping inventory has `development_test_copy=False`; its normal dependency policy is used.
These are actual Linux software-Vulkan windows, not physical-device or clean-host acceptance.

```sh
python3 Tests/TwoD/SpriteSourcePlayerAcceptance.py --app /workspace/Nexora/build/linux-development/Apps/TwoDPlayer/NexoraTwoDPlayer --compiler /workspace/Nexora/build/linux-development/Tools/TwoD/NexoraTwoDAssetCompiler --zig /workspace/work/editor-completion/tools/lib/python3.12/site-packages/ziglang/zig --cmake /workspace/work/editor-completion/tools/bin/cmake --catalog --repack --native --evidence-dir /workspace/work/editor-completion/source-repack-focused
python3 Tests/TwoD/SpriteSourcePlayerAcceptance.py --app /workspace/Nexora/build/linux-shipping/Apps/TwoDPlayer/NexoraTwoDPlayer --compiler /workspace/Nexora/build/linux-development/Tools/TwoD/NexoraTwoDAssetCompiler --zig /workspace/work/editor-completion/tools/lib/python3.12/site-packages/ziglang/zig --cmake /workspace/work/editor-completion/tools/bin/cmake --catalog --repack --shipping --native --evidence-dir /workspace/work/editor-completion/source-repack-shipping
```

Fresh Development/Shipping configure/build pass. Additional `NEXORA_ENABLE_2D=OFF`,
`BUILD_TESTING=OFF` builds of `NexoraHost` and `NexoraEditorCore` pass with Editor/SDK/Cryptography
enabled and graphical shell/native backends/Zig/Slang disabled. Thus the actual Editor core is
compiled without testing or a TwoD module. Full unfiltered Development CTest passes **486/486,
zero failures/skips, 1392.80 seconds, exit 0**, with two workers and no failed-case retries.
Exact-head hosted CI remains required before merge.
The new native case runs serially; original fixture deadlines and both earlier modes are unchanged.

The new case passes in **52.1159 s**. Original embedded mode passes in 41.4683 s, original catalog
mode in 37.3103 s and layout restart in 30.4592 s. All three frozen test-source hashes match.
The [machine receipt](ED-WF-06-NativeSource-Cloud.json) records the complete inventory, commands,
binary/JUnit/log hashes, feature-OFF settings and observed native sample grids. Full-traversal
Development and independent strict Shipping captures are byte-identical for all five phases:
[base](ED-WF-06-NativeSource/base.png), [Flip X](ED-WF-06-NativeSource/flip-x.png),
[Flip Y](ED-WF-06-NativeSource/flip-y.png), [Flip XY](ED-WF-06-NativeSource/flip-xy.png) and
[repacked XY](ED-WF-06-NativeSource/repacked.png).

```sh
cmake --preset linux-development
cmake --build --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
cmake -S /workspace/Nexora -B /workspace/work/editor-completion/repack-feature-off -G Ninja -DCMAKE_BUILD_TYPE=Development -DNEXORA_LINK_MODE=Modular -DBUILD_TESTING=OFF -DNEXORA_ENABLE_2D=OFF -DNEXORA_ENABLE_EDITOR_SDK=ON -DNEXORA_ENABLE_EDITOR=ON -DNEXORA_ENABLE_CRYPTOGRAPHY=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF -DNEXORA_ENABLE_NATIVE_BACKENDS=OFF -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_ENABLE_SLANG=OFF
cmake --build /workspace/work/editor-completion/repack-feature-off --target NexoraHost NexoraEditorCore --parallel 4
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development --output-junit /workspace/work/editor-completion/repack-full-junit.xml
```

## Retained initial failures

- The first manual invocation supplied a relative compiler path despite the helper changing its
  working directory to the source workspace. Absolute paths, as already supplied by CTest, repair
  that invocation without changing the helper's path contract.
- The first new fixture placed its Y-mirrored quad below the camera: status PASS and balanced
  allocations, but zero visible instances. Its expectation omitted the documented Pivot-relative
  geometry mirror. Correcting fixture positions and hand-authored pixel locations preserves the
  noncentral Pivot and checks the required movement. No engine behavior or assertion was weakened.
- Extra feature-OFF setup initially selected an optional 2D game target and an Editor core with the
  Editor switch disabled, then omitted the required Cryptography dependency. Those configurations
  fail before compilation. The final explicit configuration builds the general Host and actual
  Editor core with TwoD/testing still disabled; target-selection diagnostics are retained.

Inspector assignment, authoring Undo/save/reopen/Build and source-animation/reload remain separate.
Workflow acceptance stays 4/9, graphical Editor 0/8 and 2D 9/11 until the parent work order is accepted.
