# ED-WF-08 Source animation compiler portability

Date: 2026-11-13 (UTC). Supporting task: `nexora-whr.8.1`, PR #780.

## Corrected validation

The corrected source generation passes **523/523** Linux Development tests,
zero failures/skips, **1972.09s**. All twelve serial configure/build/test/native gates
pass, including actual Shipping animation and embedded-source Player, Monolithic four-test
consumers and TwoD-OFF/testing-OFF graphical Editor/native Player builds. The
[machine receipt](ED-WF-08-SourceAnimation-Portability-Cloud.json) records the complete inventory,
26 source hashes, compiler probes and new native artifact hashes. The
[original feature execution](ED-WF-08-SourceAnimation-Linux-2026-11-13.md) describes Clip schema 2,
ownership, source dependency closure, Studio and native pixel oracles.

The first submitted head `1ec968b6` passed its GCC 14 cloud local gates but failed hosted compiler
checks. Its committed receipt remains unchanged and historical. GCC 15.2 diagnoses the legacy
conditional expressions passing an unnamed Clip into `Animator::Play`; MSVC diagnoses the new
test's phase signedness and size_t event capacities. No failed CI check was bypassed or retried.

The two benchmark consumers now select their supplied Clip directly or construct a named fallback
inside the fallback branch. `Animator::Play` retains its existing value-copy contract. The fallback
is destroyed after use and is not allocated when a Clip is supplied. The test uses an explicit
uint32 phase comparison and a bounded uint32 capacity shared with its two-element event array.
No warning option, assertion, gameplay codec or C/Zig layout changes.

All 23 original source/registration files apart from the changed CPU test retain their exact
`1ec968b6` hashes; the two additional source files are the existing benchmark consumers. The
machine receipt checks that the original feature JSON is byte-for-byte identical to its committed
version. Source reimport,32 source-aware real Zig reloads,configured packaged main Play and complete
ED-WF-07/08 remain open. Workflow **5/9**, graphical Editor **0/8**, TwoD **9/11** and physical/clean-host
acceptance remain unchanged.

## Compiler evidence

The archived CI CMake logs identify GNU **15.2.0**, with `-Wall -Wextra -Wpedantic -Werror`.
A task-owned Ubuntu GCC **15.2.0-4ubuntu4** reproduces both original benchmark failures and compiles
all three corrected files successfully. All **17** affected C++ translation units additionally
compile under GCC 15.2 with their actual Development flags. Sources/commands/log hashes are retained.
The full configure/build/CTest and native/linkage matrix above uses the task's existing GCC 14.2
environment; this is not a claim of a complete GCC 15 engine build or local MSVC execution.

The initial assumption that CI used GCC 13 was incorrect. A task-owned Debian GCC 13.3 compiles
both original and corrected forms. Its initial failed reproduction assertion remains setup history;
the later compatibility receipt records all three final forms passing. Actual compiler identification
and the distinct GNU 15 reproduction are preserved. Both compiler distributions were downloaded
from official Debian/Ubuntu HTTPS package hosts and extracted into task-owned directories. Package
versions,byte counts and SHA-256 hashes are recorded; no repository presets or warning rules change.

## Executed regression

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

The additional Shipping `AuthoredSourceAcceptance.py --native --shipping` command uses the actual
Shipping TwoDPlayer and Development producer/fixture/compiler, exactly as the hashed supplementary
runner records. Both final animation runs retain 30 native frames,60 GPU instances,two visible
Sprites,balanced allocation and all five asymmetric pixel phases. `repacked` remains a label for
the second preloaded source/Atlas animation frame; it does not mean live reimport. Native screenshots,
reports,package hashes and source-deleted relocation remain asserted.

The five focused animation/benchmark/manifest tests passed 5/5 in 8.81s before the full rerun.
Earlier fixture failures,interrupted gates and the original complete failed 522-case gate remain
in the original record. Unconfirmed Vulkan residency issue `nexora-ko9` stays open; this repair
changes no renderer or residency assertion. Hosted CI belongs to the new immutable PR head and
remains a separate delivery gate,recorded in the PR after all applicable jobs succeed.
