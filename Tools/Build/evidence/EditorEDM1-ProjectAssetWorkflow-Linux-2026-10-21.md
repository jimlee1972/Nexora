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
persistent UUID and reimported geometry/artifact. Save uses the real host command. After graceful
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

The local Xvfb/lavapipe native workflow passed with writer `scene_draws=346` and read-only
`scene_draws=27`; both resolved one authored mesh. This verifies software-GPU submission and data,
not physical GPU visual quality, production performance, physical display, DPI/IME or devices.
An earlier single-click folder navigation and an empty text insertion in the fixture were
corrected to actual double-click and Backspace gestures; those failed attempts are excluded.
The final fixture explicitly checks filter narrowing and clearing rather than inferring search
success from asset selection.

Required full Linux configure/build/CTest and final current-head native rerun are pending in this
implementation checkpoint. No milestone is accepted by this checkpoint. Hosted display CI
requires the conditional test registration, then its existing full Editor CTest pass executes it.
Cancellation, initial import failure and external-change policies retain their existing focused
contracts/tests; this finite scenario does not claim exhaustive M1 workflow or physical acceptance.
