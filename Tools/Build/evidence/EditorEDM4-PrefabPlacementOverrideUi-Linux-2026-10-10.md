# Graphical live prefab instance review and Revert — Linux, 2026-10-10

Beads `nexora-pmb.1.25`; integration foundation71b2a45f composes owning
source inspection GUI PR510, owning Core review PR511 and atomic Revert PR512.
Only this feature's graphical binding is proposed for eventual main acceptance.

The bound-scene Inspector's explicit Review request carries an owning project,
file token, selected generation key, retained source revision and nested scope.
The application prepares and retains the immutable Core review; the UI owns only
bounded copied rows and a session serial. Clipped rows show retained/local values
and source revision, with stable node/field UUID and complete nested scope tooltips.
Control bytes are escaped and visible truncation never alters the owning report.
Captured facts require explicit refresh; drawing performs no source IO.

Revert requires a writable selected document and explicit centered confirmation.
Cancel/Escape preserve state. Application and Core independently recheck current
scope, source/target, writer and stopped Play/export/recovery/external state.
Stale, read-only, foreign, missing, structural or revoked authorization cannot
write. One owning snapshot Undo/Redo restores all retained/local properties;
bindings, unknown bytes, selection, clipboard and save baseline remain intact.
No file is written until explicit Scene Save, and source archives never change.

Actual 1x/2x controls cover copied reports, explicit Confirm/Cancel, readonly
review/disabled authoring, structural differences, stale edits, hidden frames,
focus revocation, changed selection, permission revocation and close modals.
Actual initial1280x720 Xvfb/lavapipe clicks review retained1/current2, confirm
Revert and exact source-instance Scene Save, one Undo, one Redo, final Undo and
readonly reopen. Every step compares complete persisted scene bytes and source
archive conservation; readonly launch/close compares all project files. Vulkan
validation errors are rejected. The fixed centered480x160 confirmation avoids
narrow top-position layout; pixel readiness distinguishes Confirm padding from
the blue title bar and underlying Scene controls without repeating authoring.

Graphical Development configure/build passed. Final three independent cold
native launches passed12.60s,12.53s,12.61s. Full `ctest --preset linux-development`
**247/247 passed,638.60s**, zero failures or skips, including actual1x/2x
override UI. Minimal Monolithic `linux-shipping` configure/build passed (five
actual steps). Graphical shell, Cryptography, Slang, Zig, Showcase and native
ProjectPlayer were enabled. Failed exploratory popup-coordinate probes are
excluded from this final acceptance.

Targeted graphical selection, structural reconciliation and source apply/rebase
remain open. Complete Editor milestones stay **0/8**. Linux evidence does not
claim other OS or physical-host execution; C++ consumers rebuild, stable C ABI
and module dependencies remain unchanged.
