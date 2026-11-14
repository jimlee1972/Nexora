# ED-WF-08 native Clip fixture compiler coverage

Date: 2026-11-13 (UTC). Supporting task: `nexora-whr.8.1`, PR #780.

The final named-Clip fixture generation passes **523/523** Linux
Development tests, zero failures/skips, **2242.06s**, and all twelve configure/build/test/native
gates. Shipping Full actual source-animation and embedded-source Player, Monolithic four-test consumers,
and TwoD-OFF/testing-OFF graphical Editor/native Player builds pass. API145, DocCI16, formatting21 C/C++
plus1 Zig, syntax4 Python and17 Markdown/three language pairs are checked separately.
The [machine receipt](ED-WF-08-SourceAnimation-NativeContext-Cloud.json) records27 source/build hashes,
all test results, actual commands, native artifacts and compiler evidence.

The preceding submitted head `a97a7cbf` passed523 local GCC14 tests but its hosted Xvfb job
[132140421093](https://github.com/jimlee1972/Nexora/actions/runs/43499158539/job/132140421093)
failed while compiling an existing native macro variant of `PlayContextTests.cpp`. GCC15.2 reports
`-Werror=dangling-pointer` for an unnamed Clip passed by value to `TwoDDocument::ReplaceClip`.
A task-owned GCC15.2 reproduces CPU-variant success and native-variant failure on the exact old file.
A named const Clip value passes both variants. The actual CPU/native focused tests pass2/2 in2.08s.
No product behavior, assertion, warning setting, codec or C/Zig layout changes.

The earlier17 affected-source compiler probes did not include this inherited test's native variant.
Coverage now includes **all309 actual TwoD-consuming compile command variants across306 source files**,
including duplicate CPU/native registrations, using GCC15.2 and their configured Development flags.
Every command, source/log hash and result is retained. This is a compile matrix, not a full GCC15
engine link/test claim; full local engine/native gates use GCC14.2. Hosted Windows/MSVC and other
platform CI remain separate exact-head delivery gates. Failed heads are not retried or merged.

One subsequent full attempt ran two repository suites concurrently while the compiler matrix was
running. Both existing Prefab graphical cases reached the unchanged180s limit after hundreds of
real X11 actions. Those failed/intentional130 partials and exact input/fixture hashes are retained.
The unchanged Prefab test then passes in isolation; the final full suites run sequentially with
all assertions/timeouts unchanged. This scheduling diagnosis does not close `nexora-ko9`.

Both the initial `1ec968b6` feature JSON and the `a97a7cbf` portability JSON remain byte-for-byte
historical committed receipts. See the [first portability supplement](ED-WF-08-SourceAnimation-Portability-Linux-2026-11-13.md)
and [feature execution](ED-WF-08-SourceAnimation-Linux-2026-11-13.md) for earlier compiler, fixture,
interrupted and native failures. Unconfirmed Vulkan residency issue `nexora-ko9` remains open.

## Executed acceptance

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development --output-on-failure
cmake --preset linux-shipping -DNEXORA_SHIPPING_PROFILE=Full -DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_EDITOR_SDK=ON -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_ZIG_EXECUTABLE=/workspace/work/engine-goal-20261112/tools/lib/python3.12/site-packages/ziglang/zig -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-shipping
ctest --test-dir build/source-animation-monolithic -R '^twod\.(source_animation|play_assets|authoring_contracts|graphical_controls)$' --output-on-failure
python3 Tools/Build/ValidateApiM6.py .
python3 Tools/Build/TestDocumentationCI.py
```

The hashed runner and log inventory provide complete native/Monolithic/OFF commands. Development
and Shipping animation reports each retain30 native frames,60 GPU instances,two visible Sprites,
balanced allocations and five pixel phases. The `repacked` phase is a second preloaded animation
frame, not live reimport. The extra Shipping embedded-source check uses the actual Shipping Player
and Development producer/compiler, retaining source-deleted relocation and package/input hashes.

Production source reimport across32 real Zig replacements, configured packaged main Editor Play
and complete ED-WF-07/08 remain open. Workflow **5/9**, Editor **0/8**, TwoD **9/11** and all physical/
clean-host acceptance counts remain unchanged. The independent generation-staging work is a
prerequisite with its own evidence, not an acceptance claim for these parent integrations.
