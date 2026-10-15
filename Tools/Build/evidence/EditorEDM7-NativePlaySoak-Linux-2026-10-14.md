# ED-M7: Scoped native Play lifecycle gate

Date: 2026-10-14. Beads: `nexora-0ua.1.12`. Base: `85c5a98da6d81be490ea0b288f6d37fd63f86974`.
[Machine-readable samples, budgets and source hashes](EditorEDM7-NativePlaySoak-Linux-2026-10-14.json).
[Owning Editor contract](../../../Apps/Editor/README.md#scoped-native-play-lifecycle-gate).

The previous native Game fixtures each exercised one session; repeated input/module checks were
portable. This independent gate runs 32 real Play sessions inside one Editor process on Linux
X11/Xvfb with Vulkan/lavapipe. The dataset is one triangle asset, a camera and mesh entity, two
disjoint visible native Scene/Game canvases at 1x DPI, and a dynamically loaded C-ABI module.
Each module allocates and touches 4 MiB through the host, then validates every touched page before
returning the exact allocation during Destroy. No production C++ behavior, ABI or module dependency
changes are introduced.

Each cycle observes fresh neutral Create/Start and the original clone pose; uncaptured input stays
neutral, captured pointer motion changes Game pixels once, empty batches stay stable and Escape
releases capture. Pause stops Fixed callbacks; manual Step invokes exactly one neutral Fixed callback
without moving pixels; Resume restarts callbacks without replay. The fixture then holds an actual
captured back button while Stop executes. Stop/Destroy must see neutral copied input while their
provider and World still work. All 128 lifecycle records must follow create/start/stop/destroy.
Native authoring Scene pixels are conserved during Play motion and after Stop, while scene/OBJ/meta
bytes remain exact. Every new Play clone starts at the original pose. No apply-back/save is requested.

The whole case is serial with a 240-second CI liveness deadline. Observed transitions have bounded
3-second deadlines; startup first waits up to 12 seconds for the actual native Scene viewport.
A Game-only first draft sent F5 before the first native frame and failed in 3.27s; the visible source
canvas supplies the readiness handshake. No startup sleep or repeated toggle substitutes for it.
Fixture streams each have a 4 MiB output budget and Vulkan validation errors reject acceptance.

RSS is sampled from `/proc/<pid>/status` after each Stop and 100ms of event processing. Four warmup
cycles precede a baseline equal to the larger of samples3/4. The peak of samples5–32 must grow no
more than 32 MiB above it. All raw samples are retained; RSS includes shared/native mappings and does
not identify live allocation ownership. This envelope does not prove freedom from every leak.

Three separate fresh-process native runs passed: initial 32-cycle 81.27s; restored 32-cycle 81.68s;
and strengthened 32-cycle 82.40s with source-pixel conservation. The strengthened run warmed at
158,154,752 bytes, peaked at 162,512,896 and ended at 158,826,496: peak growth 4,358,144 bytes against
the 33,554,432-byte envelope. Zero Vulkan validation errors were observed.

A controlled production fault temporarily removed input revocation before module unload. The gate
failed cycle 1 in 2.86s: Stop and Destroy both reported neutral0, while touched pages remained valid.
The provider header was restored byte-for-byte, and the restored 32-cycle test passed. No fault or
acceptance relaxation remains in the patch.

The prior pointer evidence JSON had incorrect handwritten host prefix sizes. A compiled C11 probe
of the canonical header reports original 136 / old2D 144 / pointer-host 152 bytes on 64-bit, with the
pointer snapshot 56 bytes. Correct that report; the implementation and C/Zig layout/guard tests
already derive field-end offsets and were correct. This correction changes no ABI behavior.

**Full Linux Development configure/build and 305/305 tests passed in 1073.60s, zero skips.**
Commands: `cmake --preset linux-development`, `cmake --build --preset linux-development`,
`ctest --preset linux-development --output-on-failure`. Real Zig 0.14.0 and Showcase are enabled.
The full-gate soak passed in 82.32s; its full 32 RSS samples are in the JSON record.
`cmake --preset linux-shipping` / `cmake --build --preset linux-shipping` also passed (9 actions,
Monolithic, real Zig/Showcase ON, BUILD_TESTING OFF excludes the test fixture). Documentation 16/16,
changed Markdown/bilingual pairs, touched C++ formatting and 4 source hashes passed; root README
and production provider bytes remain unchanged. All final-head hosted CI gates merge. Physical GPU/VRAM, production asset scale,
project/document/plugin/device-loss lifecycles, long-duration physical-host soak and complete
ED-M3/ED-M7 milestone acceptance remain open.
