# ED Project Play input bindings: Linux evidence

Date: 2026-10-08. Source parent: `403b42fc` (session Play rebinding).

Apply remains session-only. Apply and save validates/publishes a session draft and emits an owning
project UUID/native-root/profile request consumed once by the application. The owner rechecks
current project and stopped Play, then delegates access/recovery/profile/path validation and
atomic publication to ProjectWorkspace. Failed saves preserve the previous file and applied
session profile; errors remain visible and explicit retry is available. Session Apply clears stale
save-success text. Scope/focus/close/conflicting gates discard pending save requests.

`.nexora/play-input.ini` uses schema-1 ASCII named action/control records. Deterministic saves
share the atomic replacement helper; a fixed 1,025-byte read enforces the 1,024-byte budget.
Reordered actions, CRLF and missing final LF are accepted. Unknown/missing/duplicate records,
extra data, NUL, unknown controls, duplicate concrete controls and mouse-axis bindings reject.
Verified missing legacy settings use defaults without creating metadata. Corrupt/unsafe settings
remain untouched until explicit replacement. Read-only loads work; read/write reject pending
recovery, unsafe metadata/file aliases and non-regular destinations. Occupied stages are preserved.

Startup/project activation loads validated settings into an owning scoped UI setter. Missing state
uses defaults; load errors preserve files and appear visibly. Startup recovery defers and retries
loading once resolved. Saved bindings reach the existing native/deferred copied gameplay input;
reading them loads no executable code. Ordinary shutdown never rewrites binding settings. New
workspace/UI APIs are additive with unchanged existing class layouts, module dependencies and C ABI.

Validation coverage:

- `editor.play_input_settings`: UTF-8 project path, owning/read-only round trip, invalid save before
  staging, compatible ordering/line endings, missing/unknown/duplicate/corrupt/budget rejection,
  explicit corrupt replacement, occupied file/directory/valid/dangling stages, directory/alias
  destinations, metadata-directory aliases, recovery rejection/resolution and lease/workspace
  reopen. Missing legacy read-only metadata is not created.
- `editor.game_input_bindings`: real 1x/2x/macOS input, session-only Apply, one-shot owning scoped
  Apply/save with no widget IO, owner save and recreated UI restoration, duplicate/read-only save
  rejection and stale project-request cancellation, plus existing draft/focus/modal/Play coverage.
- `editor.linux_native_project_input`: two real native Editor processes under Xvfb/lavapipe. The
  writer loads project B movement and rejects old D; read-only restart loads persisted bindings
  and gameplay-library selection, drives B through the real module, and retains settings/scene bytes.
- Existing Play input/state/module callback gates remain included in focused/full validation.

Device-specific profiles, expanded keys/devices, gamepad, pointer look, multiple users and complete
ED-M3/accessibility/physical-host acceptance remain open.

Commands use `/workspace/.nexora/env.sh`, graphical shell ON and, for native/full tests, lavapipe
with Khronos core/synchronization validation and pinned documentation dependencies:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development -R 'editor.play_input_settings|editor.game_input_bindings|editor.play_input_state|editor.play_gameplay_module|editor.linux_native_project_input'
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Focused tests **5/5 passed**, **6.83 s**. The final full Linux gate passed **156/156**,
**364.39 s**, with zero failures/skips. Development configure/build and Minimal Monolithic Shipping
configure/build passed. Changed Markdown/bilingual documentation validation and `git diff --check`
passed. Owning contracts and both roadmap languages are synchronized; generated tools, logs and
build output remain uncommitted.
