# Unchanged snapshot rotation hint conservation — Linux cloud evidence

Accepted on 2026-10-11 Asia/Taipei (2026-10-10 UTC).

Canonical Scene PrepareSave intentionally omits authored Euler hints whose saved quaternion no
longer matches current Runtime. Atomic snapshots previously replaced owned nodes, or every node
for ordinary snapshots, with a parsed canonical node and lost this latent in-memory authored
state even when only name or scale changed.

The private publication path now retains the previous hint only when both the exact Runtime
quaternion and generated canonical Euler records are unchanged. Explicit quaternion changes or
Euler-record insertion/removal use candidate metadata. Unowned placement nodes remain unchanged.
The complete existing history carries before/after node state, and canonical no-ops preserve Redo.
There is no new public API, ABI, IO, persistence format, authority or structural allowance.
Entity lookup uses bounded maps rather than repeated linear scans. Owner-thread entity borrows
end before publication and never escape. Existing 4096-node/8 MiB transaction bounds remain.

## Public behavior and regression evidence

Three independent public-behavior probes linked against the prior accepted Core library failed
with `Unchanged snapshot rotation lost latent authored Euler hint`: ordinary snapshot, placement
revision transaction and an actual source-only Name rebase. The corrected public fixture passes
all three modes, including root Y720 and unbound Y1440, rotated away to exact Z180 then returned
to identity to observe hint revival. Each mode checks twenty exact Undo/Redo cycles, generation
keys, baseline and exact project files.

Additional boundaries verify explicit X180 changes discard the previous latent root hint,
explicit canonical authored-Euler record removal is honored, exact Undo restores authored X540,
Redo restores removal, a canonical no-op preserves pending Redo, and unbound hints remain intact.
Focused acceptance passed in **0.04 s**; the complete suite repeats this test.

Complete graphical Development passed **246/246 in 607.96 s**, zero skips.
Cold **Minimal Shipping built 74 steps**. The complete suite repeated the new hint
fixture successfully in **0.05 s**, and native project-upgrade acceptance passed in **18.01 s**.

~~~sh
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
~~~

Development enables graphical Editor, Cryptography, Slang, Zig gameplay, Showcase and native
ProjectPlayer with unchanged pinned ImGui/Vulkan dependency caches. The native project-upgrade
test-driver fix is a separate foundation copy at 3b01e5c5e51a5a9643bc27e82b60a3dd5775e06e.
Minimal Shipping excludes Editor and proves profile/link compatibility only. Documentation tests passed **16/16 in 0.670 s**
and changed-document link validation passed. Hosted published-head checks remain mandatory
before acceptance on Main after prerequisite prefab/source-rebase commits land.

Latent hints remain in-process metadata; this does not persist them through Save/reopen while the
Runtime quaternion differs. GUI integration, structural/source-reference reconciliation, physical
platform checks and complete graphical milestones remain separate (**0/8**).
