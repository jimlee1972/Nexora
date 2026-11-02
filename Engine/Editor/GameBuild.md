# Shared installed-game build contract

## Shared script compilation

`PrepareGameScript` accepts an owning `GameScriptBuildRequest` with a dedicated relative manifest,
trusted host toolchain and physical temporary parent outside the authored project. It freezes the bounded source subtree and
materializes the canonical embedded SDK in a private operation; no scene capture/cooking or engine
checkout is required. Game Build uses this same entry with its remaining combined byte/file budget,
then carries the script baselines into its existing final publication guard and external records.
There is one CMake/Zig compiler path for installed Build and future Editor Play coordination.
Game publication staging retains its output-filesystem ownership for atomic replacement. If that
staging is inside the authored project, its nested script compiler instead uses the canonical system
temporary directory and copies the verified library back; compiler sources/tools stay outside the project.

The move-only `PreparedGameScript` owns a uniquely named library and its operation. Retain it until
the loaded generation and library finalizers have unloaded. `Current` verifies exact authored bytes,
the complete source inventory, ordinary paths, and the compiled artifact's size/FNV fingerprint,
using the existing cooperative single-writer integrity model. This is not code signing or sandboxing.
The child Player admits the compiled V3 descriptor without creating gameplay. A prepared library
does not prove successful game initialization or authorize application to a stale project/Play epoch.

Preparation is synchronous worker work with owning progress/diagnostic values and no Editor borrows.
It uses the existing managed process runner and independent one-worker job pool, with cancellation
and drain before private cleanup. Ordinary filesystem/allocation/progress-observer exceptions retain
the existing propagation contract. Publication, source/project/Play epoch admission and strict Runtime
World/service transaction ownership remain the Editor's responsibility; that integration is pending.

## Game capture and publication

`GameBuildCapture.h` captures saved enabled scenes and owning imported geometry/material inputs
on the authoring thread. It retains exact descriptor/settings/scene/source/sidecar bytes, including
observed missing sidecars. It rejects changed/unreimported imported sources and identity changes.
The capture owns no workspace, SceneDocument or catalog borrow. Independent scene readers have
distinct ephemeral document generations; replay compares their serialized runtime data and stable
authored node identities instead. Limits: 16384 baselines, 256 MiB combined bytes, 1024 UTF-8 path
bytes, plus the narrower existing scene/import/package limits. Aliases/reparse points reject.

`PrepareGameBuild` consumes an owning request synchronously on a worker or CLI thread. It creates
a private operation directory under an explicitly supplied physical temporary parent. It cooks the
same game catalog, writes fixed-name launcher metadata, copies an explicit trusted native Player
and runtime inventory, and runs that consumer's CPU native-data admission over **every** enabled
scene. This verifies conversion/camera/upload admission, not device creation, GPU success or pixels.

Modes are explicit: SceneOnly excludes gameplay; ScriptProject requires a saved manifest in a
dedicated project-relative script source directory. No configured script is silently ignored.
The entire bounded source subtree is frozen, excluding `.git`, `.zig-cache` and `zig-out`; file
additions/removals and byte changes reject final publication. Compilation consumes that snapshot
through the canonical `nexora_add_game_module` CMake/Zig adapter and runs its entry/authored tests.
SDK glue is embedded as data in EditorCore from canonical configure dependencies, materialized
only in the private build operation. No engine checkout is required at runtime or in a game output.
Configured CMake/Ninja/Zig executables and authored script tests are authorized code, not sandboxed;
arbitrary external/dynamic inputs are not a hermetic-build guarantee. Root-level legacy script
manifests retain their original adapter workflow; this new snapshot producer currently requires
the dedicated source directory and reports an error otherwise.

The compiled library must negotiate the same required V3 table as `GameplayModuleHost::Load`.
Inspection opens the library/executes its loader, but calls no create/start/update/stop/destroy.
It proves neither actual gameplay initialization nor device/input/audio acceptance. Actual native
startup/transitions remain integrated runtime gates. Engine target dependencies generate the host
`GameRuntime.profile`; Windows additionally supplies the configured MSVC redistributable DLLs.
The profile and compiler paths are host configuration, never selected by authored project files.
Generated development profiles record actual target DSO paths because POSIX libraries can live
in a sibling lib directory; installed profiles can instead list the flat adjacent filenames.

BuildProcess launches each tool directly with owning UTF-8 argv and an independent one-worker
process pool; no shell, GUI borrow or outer-pool deadlock. Cancellation is checked between CPU
phases and while waiting for tools; managed process groups drain before operation cleanup. Cooking
itself is synchronous and cannot be interrupted internally. Logs are bounded; failed tool diagnostics
include up to 4096 valid UTF-8 output bytes. Ordinary allocation/filesystem exceptions propagate to
the embedding host. Private-directory cleanup is best effort and refuses aliases.

An optional owning `GameBuildProgressSink` receives actual cooking/runtime/source/configure/
compile-and-test/verification/readiness phases on that worker. It must not borrow UI, workspace
or document state; exceptions unwind the managed child and operation before reaching the host.
The pure progress observer bounds displayed output to the latest 16 KiB, removes terminal controls,
rejects invalid UTF-8 display bytes and extracts at most 32 diagnostics with 1024-byte fields.
Script locations under the frozen source directory are displayed project-relative. Raw log bytes
remain separate. The Editor coordinator transfers owning snapshots through a mutex mailbox,
retaining bounded diagnostics; only the authoring thread updates widgets and publishes output.
The CLI supplies no sink and preserves its existing JSON stdout and explicit mode flags.

SDK data is embedded in bounded byte chunks, avoiding MSVC literal-size limits and preserving
canonical file bytes, including non-ASCII and line endings. No SDK strings allocate at module
startup; workers assemble owning bytes on demand. The Editor's Ninja discovery is independent of
the generator used to compile the Editor, including Visual Studio/MSBuild. Installed tools and
the native runtime profile still resolve from the trusted Editor installation, not project files.

`PreparedGameBuild` owns the staged artifact and private operation; it is move-only. The owner
thread calls Publish with its current workspace/lease/recovery/document/content guard. Publication
also rereads every exact source baseline and script file inventory. A CLI uses a read-only workspace,
writing the explicitly requested output and separate engine-user-data build records, never authored files. The graphical host must additionally resolve dirty
authoring decisions and cancellation before invoking this publication step.

The native Windows profile supplies a GUI-subsystem Unicode-entry Player. Its exact bytes become
`Game.exe`, rather than a generated launcher or shell wrapper. Verification modes continue using
redirected streams and native UTF-16 paths. The Windows delivery admission checks both copies and
uses ordinary process flags/no arguments to run the source-free result and close its real window.

Output is flat and runtime-only: renamed Game executable, `Game.nxgame`, `Game.launch`, optional
Game module, configured runtime libraries and LICENSE. No `Build.commands`, `Build.sources`,
`Build.tools`, `Game.inventory`, compiler/SDK/cache, authored scene/Zig sources or Editor DLL enters
the game directory. Moving the game to another directory/computer does not require a build receipt.

`GameBuildRequest::records_parent` is a trusted embedding-host override, not an authored setting.
The default is `build-records` beside the Editor's recent-project store in engine user data
(Windows LocalAppData/Nexora/Editor, macOS Application Support/Nexora/Editor, Linux XDG config
nexora/editor). The no-home fallback uses a namespaced temporary Editor directory. Storage must be
normalized, physical and outside authored projects, private operations and runtime output in both
directions. Missing parent components are created one at a time while refusing aliases/reparse
points. CLI hosts may explicitly choose `--build-records=PATH`; graphical settings add no extra root.

After successful Publish, `PreparedGameBuild::Receipt()` holds an owning `GameBuildReceipt` with
the external current-record directory and `Publication.receipt` path. The locator values survive the
prepared object; a later successful publication replaces current records and retains the previous
directory as history, so callers archive bytes before another publication when retaining a snapshot.
Preparation and failed/cancelled publication
have no successful receipt. The CLI preserves its existing JSON status/source-required fields and
adds UTF-8 JSON-escaped `receipt` and `records` locators. Private staging metadata is frozen alongside
the runtime package, with independent exact inventories. Record storage retains `Build.commands`,
`Build.sources`, `Build.tools`, runtime `Game.inventory`, a complete `Records.inventory` and a
versioned receipt bound to project UUID, full output path and runtime inventory. The path-derived
record key is checked against the complete binding, so a hash collision cannot select another output.
Commands retain exact direct argv/CWD; tools retain compiler/SDK sizes/fingerprints. No child
environment or raw tool output is published. Size/FNV-1a-64 records detect accidental changes;
they are not signatures or adversarial authenticity guarantees. Delivery evidence uses SHA-256.
Runtime admission remains 512 MiB/256 files; each inventory/receipt read is bounded to 64 KiB.

An existing clean output requires its unmodified matching external records and complete runtime
inventory. Foreign/changed runtime or record files reject; a missing clean-output receipt does not
silently claim an arbitrary directory. A legacy output is admitted only when no current external
record exists and its complete colocated `Game.inventory` verifies the same project and all old
files. Successful migration copies old development metadata into `Legacy.*` external records,
then replaces the game with the clean runtime package. A damaged/foreign legacy output remains
untouched. Prior successful external receipts are retained as history, without old runtime binaries.

Publish stages runtime and records independently on their destination filesystems, rechecks exact
prepared bytes, existing record/output snapshots, source/cancellation/owner guard, then replaces
both directories. Any normal record-commit failure rolls the game back; record replacement failure
also restores prior metadata. The transaction guard performs recovery on unexpected exceptions.
Failed rollback retains old backup directories and normal error results identify their paths.
Cancellation linearizes at the final owner/source guard. This is recoverable for serialized
cooperative writers, **not** power-loss atomicity across directories/filesystems or isolation from
hostile concurrent filesystem replacement. Private cleanup refuses aliases and is best effort.

The Editor executable's `--build-game=...` command and graphical Build worker use this same
producer/publisher. ✅ Configured Play, native Linux pixels/GUI script Build, full Linux/Windows/
macOS gates and the relocated Windows Editor/game delivery are accepted in PR #721. See the
[acceptance record](../../Tools/Build/evidence/ProjectSceneDelivery-Acceptance-2026-10-26.md).
This does not accept every platform's independent installation or complete Editor milestones.
