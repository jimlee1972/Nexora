# Explicit main-game animation root capture — Linux, 2026-11-14

Supporting task: `nexora-whr.8.3.1`. Full Linux Development configure/build/CTest passes
**532/532**, zero failures/skips, **2039.81 s**. All **13** sequential local gates pass.
The [machine receipt](ED-WF-08-GameClipRoots-Cloud.json) records all 16 frozen source/build/test
hashes, gate logs, actual GCC 15 commands, native receipts and compatibility provenance.

## Behavior and ownership

Schema-3 `project.build.json` persists up to 4096 explicit unique nonzero `animation_clips` UUIDs
beside the schema-2 fields. Save/reopen and unrelated settings edits preserve the roots. Clearing
them recovers the prior writer. The current graphical settings window has no Clip picker.
The saved Build capture owns selected Clips and their exact imported source/Atlas closure;
unselected Clips remain outside the output. Mandatory sidecars/raw bytes/artifact identities join
the existing final source guard. Edits without reimport, missing identities and wrong-type roots
reject before publication. Source-free settings and capability-2/3 package bytes stay unchanged.

Game capability 4 carries a canonical explicit root list after the scene catalog. Runtime verifies
NXAB integrity and exact typed graph structure without an optional TwoD dependency. TwoD then
validates payload UUIDs/dependencies, all legacy regions and complete source/trim closure under
the existing 4096-asset/64 MiB budget. `ResolveClip` returns immutable shared ownership. CPU tests
advance a retained source Clip after all producer/catalog/source owners disappear, check root-only
and legacy-only packages, reject orphan/duplicate/cyclic/missing/wrong-type graphs and malformed
payloads, and mutate correctly checksummed root counts/order/identity/capability on the wire.

## Executed validation

- `cmake --preset linux-development`, `cmake --build --preset linux-development`,
  `ctest --preset linux-development --output-on-failure`: **532/532**, **2039.81 s**.
- Ten actual affected Development compile commands replayed with GNU 15.2, preserving warnings.
- `cmake --preset linux-shipping -DNEXORA_SHIPPING_PROFILE=Full ...`
  and `cmake --build --preset linux-shipping`: Full Monolithic runtime. The producer is the Development Editor with the Shipping runtime profile;
  project policy forcibly disables the Editor in Shipping.
  The Editor and testing remain off in this Shipping configuration.
- Actual main Build/compiled SDK module, Unicode relocation, authored-source deletion and zero-arg
  native Player in Development and Shipping. Both independently check 8192 asymmetric Sprite
  pixels, negative scale/Pivot/trim, unchanged central 3D pixels, stable captures, lifecycle order,
  immutable project/runtime inventories and native drain. The output verifies capability 4,
  two scenes/nine assets; an unrelated imported Clip is excluded. Development additionally runs
  actual configured read-only graphical Editor Play with two admitted Atlas textures.
- Fresh Monolithic affected Runtime/settings/Build CPU consumers: **3/3**.
- TwoD-OFF/testing-OFF graphical Editor and native ProjectPlayer build; the actual OFF Player
  rejects the retained capability-4 package before callbacks with an explicit TwoD-support error.
- The pre-change source-capable Runtime and current Runtime independently encode the same fixed
  capability-3 fixture as **1712 bytes**, FNV-1a **602d2d4a2f78135b**. The existing capability-2
  golden fixture remains **1379 bytes**, **7df5c4a79b4fc8ed**. These are compatibility/corruption
  checks, not authenticity signatures. The stable C/Zig ABI and asset codecs are unchanged.

The initial focused CPU/native/compiler checks preceded final bounds/comments, compatibility
golden and UI handoff guards. An intentionally interrupted full attempt completed 55 passes, zero
completed failures, exit 130; its complete 14-source generation/logs remain in `pre-ui-root-bound`
and are not a full pass. Real 1x/2x UI controls additionally prove root retention through shortcut,
Save, Reload and script Build, with oversized handoff rejection preserving the previous ready draft.
The first optional Shipping native invocation used a nonexistent Shipping Editor path and
failed before Build; its logs/receipt remain in `pre-shipping-producer-scope`. Project CMake forces
Editor OFF in Shipping. Corrected optional acceptance uses the Development Editor producer and
Shipping Player/runtime profile with the same 16 product sources; the valid full532 result is
retained. Those earlier archived receipts are historical. Final acceptance uses the frozen 16-file
generation through the complete gate chain. No failed native oracle, warning, deadline or input
condition was weakened. The Shipping standalone receipt directory was moved into ignored build
evidence after execution; original command paths remain historical observations in its receipt.

## Scope and remaining work

Native frames here exercise asset admission and existing static authored Sprite rendering. The
compiled main module updates through its existing host. It does not negotiate main 2D/source/pool
services or animate those rendered Sprite references. CPU Animator advancement demonstrates
retained Clip ownership. Main gameplay services, live source publication, graphical Clip selection,
32 SDK replacements through complete production loops and complete Editor strict reload acceptance
remain open. Workflow **5/9**, graphical Editor **0/8**, TwoD **9/11** and physical/clean-host
acceptance are unchanged. Fresh exact-commit hosted CI is required before delivery; this document
records Linux cloud execution, with no claim of local Windows/macOS execution.
