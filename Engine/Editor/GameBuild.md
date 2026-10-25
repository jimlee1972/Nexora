# Shared installed-game build contract

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

`PreparedGameBuild` owns the staged artifact and private operation; it is move-only. The owner
thread calls Publish with its current workspace/lease/recovery/document/content guard. Publication
also rereads every exact source baseline and script file inventory. A CLI uses a read-only workspace,
writing only the explicitly requested output. The graphical host must additionally resolve dirty
authoring decisions and cancellation before invoking this publication step.

Output is flat and source-free: renamed Game executable, `Game.nxgame`, `Game.launch`, optional
Game module, configured runtime libraries, LICENSE, `Build.sources`, `Build.commands`, `Build.tools`
and `Game.inventory`. Commands retain the exact directly launched argv/CWD; tool records retain
compiler/SDK file sizes and fingerprints. No child environment or raw tool output is published. No
scene text, Zig source, SDK/compiler/cache or Editor DLL enters the game output. Source/startup/
scene identity and inventory size/FNV-1a-64 fingerprints are retained as change/corruption evidence;
these are explicitly not signatures or adversarial authenticity checks. Independent delivery
evidence records SHA-256. Aggregate output admission is 512 MiB and 256 files.

Publish requires a physical destination parent. An existing output must match the complete engine
inventory for the same project, with no foreign/changed files. It copies to a unique sibling and
revalidates stage/source/owner/previous output before committing. Existing output moves to a reserved
backup, then the candidate takes its name. Rename failure restores the previous output; a failed
rollback reports and retains the backup path. The two renames are recoverable under cooperative
serialized writers, **not** a power-loss-atomic replacement of a nonempty Windows directory or
isolation from hostile concurrent filesystem actors. Cancellation linearizes at the last guard.

The Editor executable's `--build-game=...` command uses this same producer/publisher. Graphical
build coordination, Editor Play, Linux/macOS relocation and final Editor ZIP remain dependent
acceptance work; the candidate API and Windows CPU tests do not close phase 4/5.
