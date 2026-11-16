# Main Editor Atlas slicing and Clip editing

Status: local authoring and native matrix validated. Delivery requires every applicable hosted
CI check at the exact PR head before squash merge. This accepts the scoped `nexora-3hz` slice;
ED-WF-07/08, physical/clean-host acceptance and the parent milestones remain separate.

Base: `bebe757497e5638165c75c3f4eef90492adc1204` (main, PR #797).
The [machine receipt](ED-WF-AtlasClip-Linux-2026-11-16.json) records SHA256 for the15 delivery
sources, native receipts and logs. The original13 implementation/build/test sources stayed fixed
through the final matrix; the14th source acknowledges reimport test input and the15th retains CI
artifacts. Existing C/Zig ABI, asset schemas and module dependencies are unchanged.

## Delivered behavior

Main Content opens owning Atlas/Clip drafts. Atlas grid/individual slicing edits rectangle IDs
and coordinates while preserving image pixels, extent, filtering and UUID. Clip editing appends,
rebinds, reorders and removes region- or Sprite-source frames, durations, events and loop state.
Draft Undo/Redo and the isolated Animator preview are independent of Scene Undo. A bounded CPU
pixel preview allocates no native textures and performs no filesystem reads during GUI rendering.

`SaveTwoDAssetFile` privately prepares typed import, retained Content/Undo publication and the
dependency graph before replacing the primary source. Current project/Content/imported/disk/sidecar
baselines and writer/stopped-Play admission reject stale, foreign, external, read-only or malformed
requests. Atlas edits preserve dependent Sprite trim and legacy Clip closure; source Clips require
Ready Sprite/Atlas dependencies. Failed/loading last-good owners cannot authorize new closure;
unrelated failed imports remain usable alongside valid edits. Identity sidecars and Scene/Runtime
stay fixed. Saved/rejected/uncertain outcomes are distinct. Clip changes retain the existing
restart-Play policy.

Review regressions cover failed/loading retained dependencies, UUID-disambiguated source filenames
sharing `###`, invalid-draft preview rejection and the modal's visible title within a720px viewport.
Actual1x/2x controls cover Grid/rectangle creation, duration/event/order/source edits, draft history,
exactly-once requests, transient focus loss, stale versions, read-only and unchanged Scene/Runtime.

## Local gates

| Gate | Result |
| --- | --- |
| `linux-development` configure and full build | Passed |
| Complete unfiltered Development CTest | 556/558 passed,2 failures, zero skips,4770.08s |
| Explicit Development native recovery | 2/2 passed,2137.27s; details below |
| Graphical Monolithic selected-target build | Passed |
| Monolithic CPU/model/UI selection | 15/15 passed,15.12s |
| Monolithic native authoring/services/reload/reimport | 4/4 passed,2184.58s |
| `linux-shipping`, Full profile with Editor SDK | Configure/full build passed |
| Shipping Player, edited assets and SDK services | Both native flows passed; five exact SDK phases |
| Actual TwoD-OFF/testing-OFF Editor and Player | Configure/build and both explicit refusals passed |

The unfiltered Development command was run to completion. Its two failed cases were then explicitly
recovered on unchanged product sources with the acknowledged-input driver and one software Vulkan
worker. The receipt retains the original556/558 result and the separate2/2 recovery; **this is not a
single all-green558-case invocation**. All other556 cases passed without skips. Exact-head hosted
CI remains the delivery gate for the final committed sources and workflow.

The Monolithic CPU/model/UI selection covers gameplay/source animation/session/generation,
transactional reload, Player generations, authored Play, packaged services, game Build, Windows
Build evidence, and the new Atlas/Clip model/UI cases. It is not a full Monolithic repository suite.
Shipping intentionally disables the graphical Editor: Development authoring drives the actual
Shipping Player; the graphical Monolithic gate separately validates static Editor linkage.

## Same edited assets in Editor, Build and independent Player

Actual X11 input moves Atlas region7 from `(2,2,8,8)` to `(10,2,8,8)` in an asymmetric32x32 image.
The independent World-space `(0,3,0)` Scene probe changes from red `(228,8,8)` to green `(8,228,8)`
and retains green after read-only reopen. Saved Clip frames are `(8,0.2,"last")`,
`(7,0.375,"impact")`. Cancel, matching UUID sidecars, unchanged Scene bytes and whole-project
read-only byte equality pass.

Main SceneOnly Build exports those **same GUI-saved** Atlas/Clip assets and an explicit Clip root.
The exact edited Clip bytes occur in the runtime closure. The test relocates the package into a
Unicode directory, archives and deletes the entire source project, then starts the independent
Game. Development, Monolithic and Shipping all retain the same authored Atlas/Clip SHA256 and
frame values. Player pixels are green `(79,248,79)`, one Sprite is visible and native rendering
drains normally. Runtime package hashes remain unchanged. The receipt includes each Player report,
source hashes, capture hashes and artifact location; raw input/screenshots/ZIP/stdout/stderr remain
with the build artifacts.

## Canonical SDK reload and live reimport

| Native configuration | Actual replacements | Rejected candidates | Live reimport | Time (reload / reimport) |
| --- | --- | --- | --- | --- |
| Modular Development | 33 | 7 | 32 cycles /70 publications | 1646.88s /490.36s |
| Monolithic Development | 33 | 7 | 32 cycles /70 publications | 1616.49s /471.99s |

These are actual saved/watcher-triggered canonical SDK Zig compilations. Acceptance requires
published Runtime admission, first dispatched Update and independently predicted native pixels,
not provisional candidate-hook logs. The80-byte state payload, increasing ticks and identical
scene/entity/pool identity survive replacements. Compile/Prepare/Restore/Reloaded/binding/Capture
failures retain the live generation. Real retired-library finalizers cover generations0..32;
retired callbacks are denied, obsolete results are discarded and Stop cancels in-flight compilation.
Five exact animation/pool/Additive/unload/Replace phases preserve independent3D and authoring pixels.

Reimport alternates Atlas pixels and source pivot/geometry through actual Content controls.
Both configurations complete32 cycles/70 publications, retain last-good content for corrupt and
missing sources, preserve the half-frame animation clock and pool handle, verify a fresh SDK Build,
and reopen the saved project successfully. Authored bytes stay within the explicitly saved fixture
changes. Shipping independently passes the same five native SDK scene phases after source deletion.

TwoD-OFF uses a fresh extraction of the successful Shipping edited-project ZIP. Its real Editor
refuses Build with `Game animation roots require TwoD.`; its real Player refuses the edited package
with `Game Sprite admission requires TwoD support`. Both exit nonzero, publish no output and leave
all extracted project/runtime bytes unchanged.

## Retained failures and input correction

Initial tools/ICD/library setup and incomplete or interrupted runs remain in the task workspace.
The first integrated full run passed Showcase but GUI Build records used the sandbox's unwritable
home. Task-local `XDG_CONFIG_HOME`, alongside `XDG_STATE_HOME`, corrected that environment issue;
the unchanged animation picker passed85.01s alone and86.24s inside the complete run.

A later Showcase exposure-restoration attempt missed its unchanged5s pixel bound. Its exposed and
restored PNGs were identical despite two successful `e` commands. The cause is undetermined;
the following complete run passed the same-source Showcase case119.71s. Original captures/input
and failure logs are retained. No Showcase assertion, image oracle or deadline was weakened.

The complete run's SDK reload timed out at generation3 in compiler stage4 after2 replacements and
6 expected rejections. The compiler-timeout cause is not proven. Isolated one-worker recovery and
the subsequent Monolithic run both completed33 replacements under the unchanged90s publication,
2400s reload and600s reimport bounds. Earlier competing-build Monolithic failures remain distinct.

The failed reimport screenshot shows the actual query `aColor`, not `Color`, after5 cycles/10
publications. A successful xdotool exit had not acknowledged GUI key processing. The test now
requires existing copied pointer/button and Ctrl/A-down/up observations before typing; all32 cycles,
pixel/identity/rollback assertions and original limits remain intact. The corrected driver passes
both complete native configurations.

## Environment and evidence retention

Linux Debian13,4 CPU quota,16GiB; GCC14.2, Zig0.14.0, Slang2026.18 and task-local tools/libraries.
Native device: Mesa25.0.7 llvmpipe LLVM19.1.7 through real X11/Xvfb and Vulkan. Full Development used
`LP_NUM_THREADS=2`, serial CTest; recovery and matrix used one worker and serial CTest. No physical
GPU, Windows/macOS interactive flow or clean-host claim is made from these Linux runs.

CI retains `editor-atlas-clip` and `editor-packaged-source-{services,reload,reimport}` artifacts
alongside existing logs in both Development and dedicated Linux display collections. The machine
receipt retains source/log/native-receipt hashes and the explicit recovered full-run status.
Workflow **5/9**, graphical Editor **0/8**, TwoD **9/11**, joint ED-WF-07/08, physical/clean-host
acceptance and independently owned standalone reload work remain unchanged. Root README is unchanged.
