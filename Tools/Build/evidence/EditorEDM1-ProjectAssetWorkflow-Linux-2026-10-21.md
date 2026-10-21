# Native project and asset workflow acceptance

Finite cloud child: `nexora-xke.1.1` (M1-03); the target-host parent remains open.

## Actual workflow

The new `editor.linux_project_asset_workflow` test seeds a temporary Content tree, then uses
actual X11 key/pointer events and the real Editor project selector to create a schema-2 project.
It waits for asynchronous initial OBJ import; filters out a decoy source; selects/inspects the
imported triangle; adds its persistent asset reference to an authored scene node and submits the
native Scene; drags the source onto a real Content folder; enters that folder; rewrites OBJ
positions externally and invokes the actual context-menu Reimport. A malformed subsequent OBJ
fails without replacing last-good geometry/artifact; restoring valid bytes permits retry.

The actual Undo Content button reverses the move while retaining the latest source bytes,
persistent UUID and reimported geometry/artifact. The original identity sidecar bytes must move
with the source and return exactly on Undo, excluding deterministic-ID regeneration as evidence. Save uses the real host command. After graceful
shutdown, a second process opens read-only and resolves the saved scene mesh with the same UUID
and updated geometry. The disabled Reimport item cannot enqueue mutation; every project file's
SHA-256 and the complete relative-path set remain identical after this launch. Unrelated binary
source data is retained exactly. Both launches reject Vulkan validation errors and require native
Scene draw submission plus authored mesh geometry admission.

The observer only copies existing UI/model state into an owning string on the authoring thread.
It is opt-in in test-enabled graphical builds, never invokes a command or changes permissions,
and never retains a native/model/plugin pointer. The fixture drives normal focus, shortcuts,
worker publication, role checks, source filesystem operations and Undo. Rendered coordinates must
stabilize before input; selectors are not driven with guessed positions or startup sleeps.

## Validation and limits

The final current-implementation Xvfb/lavapipe native workflow passed with writer
`scene_draws=669` and read-only `scene_draws=108`; both resolved one authored mesh.
Sidecar bytes, artifact stamp, scene-node count and clean reopened state are explicitly asserted. This verifies software-GPU submission and data,
not physical GPU visual quality, production performance, physical display, DPI/IME or devices.
An earlier single-click folder navigation and an empty text insertion in the fixture were
corrected to actual double-click and Backspace gestures; those failed attempts are excluded.
The final fixture explicitly checks filter narrowing and clearing rather than inferring search
success from asset selection.

Accepted implementation `b80f6a31` integrates verified main `f06ee982`. Required commands:

```bash
cmake --preset linux-development
cmake --build --preset linux-development -j1
CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
```

All316 registered tests pass:309 actual passes,7 environment skips, zero failures,717.00s.
All552 frozen compiled-source/header/inl/CMake/native-script/CI hashes match afterward. The local
native-tools environment selects lavapipe25.0.7 and enables five additional software-Vulkan
checks compared with the earlier default-loader twelve-skip profile. Remaining skips are the
seven Slang-dependent Vulkan recording cases; no physical GPU acceptance is implied.
Deep hierarchy57.14s,100k224.35s and overview56.90s retain their original deadlines and scale1;
these are shared cloud functional observations, not a production baseline.

Minimal linux-shipping configure/build passes102 steps with Slang/Zig unavailable and Editor/SDK
stripped; zero tests are registered. The first fresh offline configure could not fetch Vulkan
headers; the accepted configure reuses this repository's matching pinned local dependency source.
Sixteen documentation-routing tests pass2.237s. The acceptance change after the frozen implementation
updates evidence/roadmaps only and receives current-version metadata checks before publication.

The new native workflow is conditionally registered when Xvfb/xdotool are discoverable. It ran
directly with explicit tool paths in the accepted local native environment; the full preset's
316-test registration is recorded separately. Hosted display CI requires native registration,
then its existing full Editor CTest pass executes it. No milestone is accepted by this slice.
Cancellation, initial import failure and external-change policies retain their existing focused
contracts/tests; this finite scenario does not claim exhaustive M1 workflow or physical acceptance.
