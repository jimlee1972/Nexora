# ED-M4 managed scene external-save protection — Linux, 2026-10-08

Scope: supporting task `nexora-pmb.3.1`. Ordinary managed scene Save previously reused Save As on
the associated path and could silently replace an external edit. SceneFileSession now owns a
bounded exact-byte disk baseline and requires an explicit, revision-checked Replace choice after
external changes. Full ED-M4 prefab/additive/migration/crash/source-control acceptance remains open.

## Implementation and acceptance

- Bind/Open/successful Save establish an owning baseline; New clears it. Missing-at-bind permits
  the first save, while deletion of an existing source rejects ordinary Save. Failed Bind/Open
  preserve the previous good document, generation, history, association and baseline.
- Disk equality compares raw bytes, not timestamps or collision-prone hashes. The portable test
  replaces every `Camera` occurrence with `DiskAA` at identical byte length, restores the previous
  file modification time, and still observes `NeedsOverwrite`. The external bytes, selected entity,
  authored 720-degree Euler value, dirty scene and Redo branch remain intact.
- Replacement confirmation binds one noncopyable session, project/document generation and exact
  resolved destination to a monotonically increasing revision. A second external edit returns a new
  token; previous/replayed tokens, another session/path and Bind/New/Content relocation reject.
  An occupied atomic-save temporary file preserves both scene versions and permits retry with the
  same still-current approval. Successful replacement retains Undo/Redo and refreshes the baseline.
- Same-UUID Content rename retains the disk baseline, cancels old-path approval and detects the
  external revision at the new path. A confirmed Save, document Undo/Redo and Content rename Undo
  keep the stable association and do not recreate the old source while it is moved.
- Exact 64 MiB source admission succeeds; a sparse 64 MiB + 1-byte source rejects before reading
  or replacement. Nonregular/unavailable/oversized sources fail closed. Each retained baseline and
  pending revision owns at most 64 MiB (128 MiB retained); one comparison/read scratch snapshot adds
  at most 64 MiB (192 MiB source-byte peak, excluding allocator and SceneDocument/World storage).
  Source bytes are read only on file operations, with no per-frame source reads.
- Real ImGui controls at 1x and 2x exercise ordinary Ctrl+S conflict Cancel/Replace, disk changes
  while the modal is open, Save-before-Open with independent current-save and Open destinations,
  and successful-save-only close continuation. Untitled Save-before-New to an existing explicit
  destination uses the actual application retry-composition helper, preserving that filename even
  though CurrentPath is absent. Read-only controls and document replacement reject stale requests.
- Save and Save As independently recheck pending recovery/read-only/live-token policy before IO.
  The UI owns requests and displays conflict reasons; it does no source IO. Existing explicit
  noninteractive `SaveAs(..., true)` without a confirmation retains caller-owned replacement
  semantics; interactive application callers always carry the reviewed confirmation token.

## Validation

Managed Linux cloud, GCC 14.2, CMake 3.31.6, Ninja, graphical shell ON and Slang ON; native window
tests use Xvfb with Mesa/lavapipe through the session's external toolchain. SDK/tool configuration
and generated output remain outside tracked source.

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_SLANG=ON
cmake --preset linux-development
cmake --build --preset linux-development -j 4
ctest --preset linux-development -R 'editor\.(external_scene_save_contract|scene_file_contract|scene_file_input)$'
ctest --preset linux-development
```

- Final configure/build: passed with graphical shell and Slang still ON.
- Focused portable/external-save/1x–2x scene-file input gate: **3/3 passed**, zero skips, **0.56 s**.
- Full Linux Development gate: **139/139 passed**, zero skips, **209.49 s**.
- Existing native Linux scene-file/restart workflow: **37.53 s**, passed; display acceptance:
  **19.57 s**, passed; native Scene preview: **27.96 s**, passed.
- Touched C++ files formatted with the repository clang-format policy; `git diff --check` passed.
- No module dependency/linkage boundary change. Shipping was not required for this Editor-only
  source change. C++ Editor/EditorImGui clients must rebuild for added confirmation arguments,
  result/request fields and session ownership state; stable C/Zig and scene wire formats are unchanged.
- Repository-root README remains unchanged; synchronized English/Traditional Chinese roadmap
  supporting bullets preserve the unchecked full milestone.

This is an operation-time revision check, not a filesystem lock or a guarantee against a concurrent
external change between comparison and replacement. Physical desktop crash drills, Windows/macOS
host acceptance, full semantic diff/merge/source-control UI, and whole ED-M4 acceptance did not run.

## Integration with merged process-memory support

The reviewed feature commit `c9059bce4ffe024feddcc71f8b0ee38f7c286baa` was merged with remote main
`b966f895a30cf71485051edaad9fb80faa68c14f` using a normal merge, preserving both histories. Integration
merge commit: `2a50ede18de5f446fc0bf056613f7c089c1f1130`. No conflicts occurred. The existing atomic
Inspector and process-memory Core/Profile/UI targets and application RSS sampling remain intact,
alongside the external-save test and owning retry composition. SceneFiles, the retry helper and
external-save/input test sources are unchanged from the reviewed feature commit.

The final integrated configure and 199-step build passed with graphical shell ON and Slang ON.
The first integrated full run reported **142/143 passed**, zero skips, **215.13 s**, with one existing
native Scene preview failure: Z-axis drag did not move the object, and the cached handle coordinate
differed from the subsequently visible handles. Other worktree validation/build activity was present;
that observation does not establish the failure's cause. The original failure log was retained.
No source, test, timing or acceptance threshold was changed. The failing native preview then passed
in isolation (**1/1**, **28.71 s**). Following coordination to finish other worktree suites and pause
heavy builds, the complete final integrated gate passed **143/143**, zero skips, **209.94 s**;
native Scene preview passed **27.82 s** in that full run.

```sh
cmake --preset linux-development
cmake --build --preset linux-development -j 4
ctest --preset linux-development -R '^editor.linux_native_scene_preview$'
ctest --preset linux-development
```

External logs retain the original integration result, isolated retry and final complete run separately
as `external-scene-integrated-tests.log`, `external-scene-integrated-native-preview-retry.log`, and
`external-scene-integrated-tests-final.log`. Final formatting and `git diff --check` passed. No new
linkage boundary was introduced; the upstream memory change's Shipping validation remains recorded
in its own evidence. Repository-root README stays unchanged relative to the integrated main base.

## Windows CI portability correction and final integration

PR #445's original [Build 1987 Windows mimalloc job](https://github.com/jimlee1972/Nexora/actions/runs/37847816778/job/113553408693)
reported `editor.external_scene_save_contract` throwing "No mapping for the Unicode character
exists in the target multi-byte code page." The fixture retained `Content/場景.scene`, but four
temporary-path checks used `path.string() + ".tmp"`. On Windows this converts the native Unicode
path to the local narrow code page before constructing a path, which cannot represent the fixture
on that CI host. Production atomic-save IO already preserved native paths. The original CI result
is retained as failure evidence; it is not a local Windows execution result.

Fix commit `f12c1114` constructs the test's temporary destination from the native path and appends
the ASCII suffix directly, retaining the Chinese filename, occupied-temp rejection, retry and
history assertions. The Editor's two failed-load diagnostic consumers also share one message
explicitly encoded with `generic_u8string()`, avoiding the same code-page conversion for a Unicode
project root. No save, approval, access-gate or atomic-replacement policy changes.

Remote main `f21620387e5b27dfb9cce9b341100bb516cc8708` (including Runtime Console admission) was
integrated without conflicts using normal merge commit `a07735efcfb005505935c3d0d7cd684c2de69c39`.
On that exact source head, graphical shell ON and Slang ON were confirmed in the CMake cache:

```sh
cmake --preset linux-development
cmake --build --preset linux-development -j 4
ctest --preset linux-development -R 'editor\.(external_scene_save_contract|scene_file_contract|scene_file_input)$'
ctest --preset linux-development
```

- Configure and 206-step build passed, including the changed Editor application and test.
- Focused portable and 1x/2x UI tests: **3/3 passed**, zero skips, **0.55 s**.
- Complete serialized Linux gate: **144/144 passed** on its first run, zero skips, **210.59 s**.
  Native scene files passed **37.57 s**, display acceptance **19.47 s**, and native Scene preview
  **27.78 s**. No acceptance threshold or timing was changed.
- Separate external logs: `external-scene-windows-fix-configure.log`,
  `external-scene-windows-fix-build.log`, `external-scene-windows-fix-focused.log`, and
  `external-scene-windows-fix-tests.log`.
- Touched C++ formatting and `git diff --check` passed. No linkage boundary changed; Shipping is
  unchanged. Both roadmap supporting bullets still link to this evidence and full ED-M4 stays open.

This correction was validated locally on Linux. Windows validation of the corrected source must
come from the subsequent GitHub CI run; no local Windows pass is claimed.
