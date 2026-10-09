# Editor CI queue cleanup — Linux cloud acceptance, 2026-10-09

The Build change-detection job reuses its existing actions-write permission to clean superseded
heads on six exact session-owned Editor branches. It retains the four legacy ED-M0 branches.
Only queued/running build.yml runs in this repository, on the inspected branch and a different
head SHA, qualify. The active run and both current-head push/PR runs remain eligible for full CI.
The branch ref is read again before each cancellation; advancement stops that branch's cleanup.
API failures produce warnings and never change build or merge acceptance.

## Executed evidence

- Eight executable Node guard cases passed, including branch advancement before/after a write,
  current-head/main/foreign/unlisted exclusions, same-repository PR routing and missing refs.
- All 16 documentation-routing tests passed with the isolated pinned CommonMark dependencies.
- Actual hosted cleanup jobs 113981951105 and 113982248742 succeeded. Their logs recorded
  cancellations of superseded export runs 37974486541/37974435447 and tool runs
  37974793019/37974724278. No current-head, main or foreign run was selected.
- Latest-main integration includes accepted native Player and cooperative plugin lifecycle.
  Full graphical Development passed **208/208**, zero skips, in **406.18 seconds**.
- Minimal Shipping configure/build passed. This was a cached rebuild with five actual steps.

The complete Development configuration enabled the graphical shell, Slang, Zig gameplay,
Showcase, project Player and native Player presentation. Commands:

~~~sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_BUILD_SHOWCASE=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON \
  -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
node Tools/Build/tests/editor-ci-cleanup.test.cjs
python3 Tools/Build/TestDocumentationCI.py
~~~

## Native display test corrections

The shared Xvfb launchers retain the server with -noreset. A real X11 root-property test proves
state survives the last short-lived client disconnect; the ordinary-reset negative control loses
that state. Scene-file acceptance restores Scene focus after adopted Open and explicitly clicks
the actual path widget before typing in the modal. Original edit/save/Undo/restart/read-only and
source-preservation assertions and deadlines remain intact.

An earlier 206-test run failed at the initial Save As destination assertion; a diagnostic isolated
execution passed in 38.53 seconds. The explicit-widget-focus correction passed its focused test
in 39.95 seconds, and the final integrated 208-test run passed that test in 40.12 seconds. These
observations do not establish a universal timing or physical-display guarantee.

This is CI queue and test-harness hardening. Production scale/soak and full graphical milestones
remain open. Windows/macOS validation belongs to hosted CI; no such execution occurred in this
Linux cloud environment.
