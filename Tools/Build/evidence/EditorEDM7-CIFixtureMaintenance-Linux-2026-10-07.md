# ED cloud CI fixture lifetime: Linux evidence

Date: 2026-10-07. Parent: `2fc986855e1a4095f6b81c003442f1af652975ba`.

PR #399's final-head push workflow 37566983358 failed the existing documentation-routing
>300-file test during TemporaryDirectory cleanup: `OSError: [Errno 39] Directory not empty: '.git'`.
Its final-head PR workflow 37566988102 passed routing and all 18 checks. This change addresses a
potential asynchronous writer during cleanup; it does not claim the original error was reproduced.

The routing fixtures now set `maintenance.auto=false` and `gc.auto=0` in their own repository config
immediately after init, before their first commit. Short-lived test repositories do not need
automatic housekeeping. Cleanup remains strict, exceptions are not suppressed, routing assertions
are unchanged, and production/user/global Git configuration is untouched. No workflow graph,
build dependency, module boundary, public API, or CTest registration changes.

Local Git 2.52.0 was observed through GIT_TRACE2_EVENT with a task-local inherited configuration
containing `gc.auto=1` and `gc.autoDetach=true`. One original suite run (16/16) made 22 commits and
launched 22 `git maintenance run --auto --quiet --detach` child processes. Twenty further original
suite runs passed all 320 test executions, with 440 commits and 440 maintenance children.
The exact original cleanup error did not reproduce. Twenty modified suite runs under the same
inherited configuration passed all 320 executions: 440 commits, **zero maintenance children**.
An independent trace reader checked actual commit/child-start events rather than configuration
values. The task-local config/traces/logs remain uncommitted; no home/global config was edited.

The ordinary modified routing suite passed **16/16** (0.897 seconds). Pinned documentation packages
were installed into uncommitted `work/documentation-packages` from the existing requirement file;
no new dependency or version change was introduced.

Final required gate (isolated tools/sysroot/socket/Vulkan setup matches
[recovery presence](EditorEDM7-RecoveryPresence-Linux-2026-10-07.md)):

```bash
python3 Tools/Build/TestDocumentationCI.py
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development
```

Configure/build passed. Full Linux CTest: **123/123 passed**, none skipped, **202.94 seconds**,
with graphical shell, Slang, lavapipe and Khronos core/synchronization validation enabled.
`git diff --check` and changed Markdown/bilingual pairing validation passed. No linkage boundary
changed; no additional Shipping result is claimed. Hosted checks must pass before merge.

Source SHA-256:

- `Tools/Build/TestDocumentationCI.py`: `edf950b3b4b64c2e1d50d3c40a706db0f92ee1f3ba784d1b7585b3fe48dc24bb`

This is CI fixture lifetime hardening, not new Editor graphical feature acceptance. Full ED
graphical milestone acceptance remains **0/8**; the broader feature roadmap remains open.
