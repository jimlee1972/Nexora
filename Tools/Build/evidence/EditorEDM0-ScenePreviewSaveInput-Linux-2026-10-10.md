# Native Scene Preview rotation Save input — Linux cloud evidence

The native Scene Preview fixture now delivers the Save immediately after rotation mouse release
as one physical Ctrl/S down sequence and reverse release, retaining each phase for 200 ms. Mode
selection and rotation drag remain unchanged; there is no additional Save retry. The original
quaternion/in-place/preview-before-release, Undo, snapped rotation, axis/uniform scale, mirror,
scene/camera reopen and native draw checks remain intact, as do all deadlines and CTest timeout.
No C++ product input, authoring or persistence code changes.

The expanded source-rebase graphical run failed only `editor.linux_native_scene_preview`:
252/253 passed, 652.27 s; Y quaternion remained identity after Save. This full run is excluded and
Shipping did not run. A diagnostic shadow script changed only failure reporting, retaining all
original gestures and assertions. Three subsequent original cold native runs passed, demonstrating
intermittent admission. This does not establish a reproducible content or product regression.
The candidate guards the short Save pulse/release boundary; acceptance measures real workflows
rather than claiming a complete timing-cause proof.

Three entire candidate cold native workflows passed **28.71 / 28.66 / 28.76 s**.
The complete affected suite repeats native Scene Preview successfully in **28.95 s**.
The complete affected graphical composition passed **253/253 in 658.73 s**, zero skips,
after 215 Development build steps. Minimal Shipping reconfigured and built **5 steps**.
The driver copy is 08e03cf19273ed675329c9f3cb1cfa8b96e88de4 in the graphical foundation;
the isolated one-file fix 56c909e653581356665d283417447fcf40a82f27 is based on accepted
Main 5f5079cec33e8f788e0ff10cb6cf5fe2779abc79. Accepted on 2026-10-11 Asia/Taipei
(2026-10-10 UTC). Documentation tests passed **16/16 in 1.226 s** and changed-document link validation passed.

Accepted workflows use the actual graphical Editor executable and configured sysroot Xvfb/xdotool.
The shared native helper is byte-identical between Main and the affected graphical composition.
Full local graphical evidence belongs to the affected unmerged source-rebase composition;
a separate full local Main run is not claimed. Fresh isolated-head hosted checks are mandatory
before Main merge. Existing Undo/Save helper behavior is unchanged; this fix adds no gesture retry.
Physical platform acceptance and full graphical milestones remain open (**0/8**).
