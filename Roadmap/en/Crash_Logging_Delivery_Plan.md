# Persistent crash and game diagnostics

✅ Linux local capture/persistence acceptance: 18 subprocess scenarios plus invalid-output rejection; Development full gate and Monolithic Shipping build passed. Other native platforms remain unverified here.


This is the delivery contract for the V1 Crash Reporting and Logging sections. A memory crash
ring or `CrashReporter::Capture` alone is not native crash capture. The old unconditional
“Crash Infrastructure” completion marker is narrowed accordingly.

## Local execution path

- Application entry owns `core::DiagnosticSession` through error reporting and module teardown.
  Editor, ProjectPlayer, TwoDPlayer, standalone 2D games, Showcase, Host and DedicatedServer
  use `RunDiagnosed`. Engine bootstrap supplies a shared fallback for other embeddings.
- Every admitted Core log is journaled on the producer before `Write` returns; asynchronous
  observation/ring semantics remain unchanged. This deliberate synchronous OS-write cost prevents
  losing an accepted breadcrumb while the consumer is behind. It is not a power-loss guarantee.
- Player V3 log callbacks also persist gameplay messages. Gameplay load/create/start/update/
  fixed-update/state migration/stop/destroy and library finalizers have faulting-thread context.
  Error results, C++ exceptions, PlaySession failures and job exceptions leave durable records.
- POSIX fatal signals and `std::terminate` write through a preopened descriptor without logger
  locks, allocation, file discovery, C++ stack unwinding or symbol lookup. Fatal signals terminate
  abnormally after capture. The OS retains its core/native report policy. The owner thread uses
  an alternate signal stack; arbitrary foreign-thread stack overflow is not guaranteed.
- Windows SEH and CRT fatal signals use a preopened crash file and a preloaded system DbgHelp
  entry point for best-effort MiniDump capture. Missing dump support is explicit in `crash.log`.
  In-process dump generation can fail when the OS or process is damaged.
- Per-run `game.log` rotates at 4 MiB with three retained predecessors; critical Error/Fatal
  copies in `crash.log` are capped at 4 MiB, reserving the native append path. Native capture
  appends signal/exception code, address, instruction pointer and callback context. Build ID,
  application, platform, process and startup time are written before installing handlers.
  `modules.txt` is refreshed before gameplay initialization for address/module correlation.
  Atomically replaced `context.txt` retains up to 32 bounded metadata entries, including the latest
  created GPU/backend, committed primary game scene and process RSS at Engine bootstrap. Render
  graph callbacks also identify their pass on the faulting thread.

## Finding the files

`NEXORA_LOG_DIR` overrides the root (UTF-8 on POSIX, native Unicode on Windows). Otherwise:

| Platform | Root |
| --- | --- |
| Windows | `%LOCALAPPDATA%\Nexora\Logs` |
| Linux | `$XDG_STATE_HOME/Nexora/Logs`, or `~/.local/state/Nexora/Logs` |
| Apple | `~/Library/Logs/Nexora` |
| No user-data root | OS temporary directory, `Nexora/Logs` |

Each launch creates a unique `run-<time>-<pid>-<counter>` directory. The location is printed to
stderr without redirecting the application's streams; standalone Windows startup errors also show
it in the dialog. Preserve the whole run directory. `crash.log` always has startup metadata:
`fatal kind=` distinguishes native capture from an ordinary session or a recoverable game error.
Windows reserves `crash.dmp` at startup; only a populated dump is crash evidence.

Run directories are preserved across launches and are not automatically deleted. Products can
apply their own retention policy after collecting evidence; do not delete a currently active run.
There is no automatic upload and no claim to recover logs from a crash that preceded installation.
Explicit unwritable output roots fail startup visibly. Runtime I/O failures are reported to stderr
and the independent crash file where possible; no disk-full/power-loss durability is promised.

## Acceptance and remaining platform scope

`runtime.crash_diagnostics` runs independent child processes, including a real V3 dynamic library,
for normal/concurrent writes, escaping, rotation, C++ and non-standard exceptions, terminate,
abort, worker crashes, module create/start/update/fixed-update/stop/destroy crashes, callback error results, captured job
exceptions and job crashes. POSIX additionally exercises owner-thread stack exhaustion.
It verifies the last accepted log without calling Flush, faulting callback, module map, preserved
abnormal exit and explicit bad-output failure. The test is enabled in existing cross-platform CI.
See [acceptance evidence](../../Tools/Build/evidence/Crash-Logging-Linux-2026-11-03.md).

Windows/macOS/Android/iOS native execution and physical-device acceptance remain platform gates;
Linux cloud evidence does not complete them. POSIX capture provides raw instruction addresses,
not an in-handler symbolic backtrace. Full stacks require the OS core/native report and matching
unstripped ELF/PDB/dSYM artifacts. A managed out-of-process crash backend, centralized symbol
archive/lookup, upload consent/queue/backend integration, and unrestricted foreign-thread stack
exhaustion capture remain separate production extensions. They must not be marked complete by
this local crash-log delivery.
