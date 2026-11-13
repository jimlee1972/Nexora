# ED-WF-07 Source Build integration with module-free Play

Date: 2026-11-13 (UTC). Task: `nexora-whr.7.7`; native validation repairs:
`nexora-whr.8.1.1` and `nexora-whr.7.7.1`.

This supplements the [original execution](ED-WF-07-SourceBuild-Linux-2026-11-13.md).
That original receipt retains the validation of `0f9fe509` on `ac15a642`, including its failed
attempts. It is historical evidence. This receipt validates integration with independently merged
PR #778, main `2e900aa23d646a0896af8bc5bab03c37880d8ca8`.

## Result and scope

All 521 Development tests passed with zero failures or skips in 1955.56 seconds. This includes
the incoming real module-free preview, existing module-backed preview, actual Source Build/SDK
producer and relocated native Player. The README conflict preserves both independently authored
sections; the merged application keeps authored preview and the source-package Play refusal.
Configured packaged source preview, Scene View, source animation/pool/reload and complete
ED-WF-07/08 acceptance remain open. Workflow **5/9**, Editor **0/8**, TwoD **9/11** are unchanged.

The [machine receipt](ED-WF-07-SourceBuild-MainIntegration-Cloud.json) records all 521 results,
18 frozen source/registration hashes, exact gate exits/log hashes, and new Development/Shipping
native artifact inventories. These are Linux software Vulkan/virtual-display results, with no
physical-display or local Windows/macOS/mobile claim. GitHub CI is a separate exact-head gate.

## Exact local validation

The task's existing environment supplies GCC, Ninja, Slang, Xvfb/xdotool and the physical Zig
executable. Existing Development cache enables TwoD, graphical Editor, Editor SDK, native Player,
Slang and Zig. No machine-specific preset or generated build output is committed.

```bash
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development --output-on-failure
```

Shipping Full was configured with `NEXORA_ENABLE_EDITOR_SDK=ON`, `NEXORA_ENABLE_SLANG=ON`,
`NEXORA_ENABLE_ZIG_GAMEPLAY=ON`, `NEXORA_BUILD_PROJECT_PLAYER=ON`,
`NEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON` and the physical `NEXORA_ZIG_EXECUTABLE`, then built with
`cmake --build --preset linux-shipping`. Actual native acceptance used:

```bash
python3 Tests/Editor/LinuxGameSpriteBuild.py \
  --editor build/linux-development/Apps/Editor/NexoraEditor \
  --player build/linux-shipping/Apps/ProjectPlayer/NexoraProjectPlayer \
  --fixture build/linux-development/Tests/Editor/NexoraGameSpriteBuildTests \
  --xvfb /workspace/work/engine-goal-20261112/sysroot/usr/bin/Xvfb \
  --xdotool /workspace/work/engine-goal-20261112/sysroot/usr/bin/xdotool \
  --cmake /workspace/work/engine-goal-20261112/tools/lib/python3.12/site-packages/cmake/data/bin/cmake \
  --ninja /workspace/work/engine-goal-20261112/tools/bin/ninja \
  --zig /workspace/work/engine-goal-20261112/tools/lib/python3.12/site-packages/ziglang/zig
```

The retained runner/log hashes identify the exact absolute tool arguments used in this session.
This uses the Development Editor producer and actual Shipping Player/profile.

The existing `build/inspector-monolithic` cache was reconfigured from the Linux Development preset
with `NEXORA_LINK_MODE=Monolithic`, graphical Editor ON, Slang/Zig gameplay OFF. Building
`NexoraGameSpriteBuildTests` and running `ctest --test-dir build/inspector-monolithic
-R '^editor.game_sprite_build$' --output-on-failure` passed 1/1 in 0.03 seconds.

The existing `build/inspector-feature-off` cache was reconfigured with TwoD OFF, testing OFF,
graphical Editor/native Project Player ON and Slang/Zig gameplay OFF. Both `NexoraEditor` and
`NexoraProjectPlayer` built successfully. All eleven serial configure/build/test/native gates exited 0.

## Native evidence

Both final native runs rebuild the game through the actual Build producer and canonical SDK,
delete authored sources, relocate the package under a Unicode path and launch `Game` without
arguments. The existing complete 8192-pixel Sprite crop/Pivot/mirror oracle passes. Independently
built source-free SceneOnly output supplies an exact 6400-pixel underlying 3D RGB comparison;
both new runs match byte for byte. The receipt records their ROI hashes. Ordinary WM close,
gameplay callbacks, allocation balance, source/runtime hashes and native drain remain asserted.

The final Development and Shipping evidence directories are recorded in the JSON receipt,
including native screenshots, lifecycle reports and exact hashes. Original attempts remain present.

## Retained failures and native transport repairs

The first complete new-main gate finished 520/521 passes, zero skips, 1897.21 seconds, exit 8.
`editor.linux_build_process_console` did not observe its one raw 60 ms `Ctrl+Alt+B` opening chord.
Its original fixture, native input/streams and complete gate are retained separately. The local test
now sends one physical chord, acknowledges the existing copied Control/Alt states, waits for real
console control bounds while B remains down, then acknowledges release. One shared 12-second
deadline covers the gesture. There is no relaunch/retry, product hotkey or generic driver change.
The actual original focused acceptance passed in 12.33 seconds; the new complete gate passed the
same case in 12.48 seconds, with Run/Cancel/failure/zero-exit and authoring-preservation assertions.

The lifetime negative control previously assumed one pipe read contains Xvfb's complete readiness
line. An actual server with one-byte reads reproduces that assumption's failure. The test now
accumulates the same one-process readiness line within the unchanged 10-second deadline and adds
the real fragmented-transport negative control. Default-reset and both retained-display property
oracles remain intact. This complete gate passed it in 0.73 seconds.

An earlier, separate source-animation gate recorded a Vulkan residency submit failure. Its failed
frame/streams and seven diagnostic-only probes, original unchanged-binary probe and controlled
Showcase concurrency are retained; the cause remains unconfirmed in open `nexora-ko9`. No renderer
or residency assertion was changed. The original strict test passed this complete new-main gate
in 3.84 seconds. A focused pass does not replace the failed gate or claim a root-cause fix.
