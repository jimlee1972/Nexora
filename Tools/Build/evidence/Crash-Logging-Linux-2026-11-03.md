# Persistent crash/game logging — Linux acceptance

Date: 2026-11-03 (Asia/Taipei). Scope: local process capture and gameplay persistence.
Source base: `7c9164b6306e`; requested implementation is on `fix/persistent-game-crash-logs`.
No Windows/macOS/Android/iOS native run or physical GPU acceptance is claimed by this evidence.

## Environment and commands

Debian 13, GCC 14.2, CMake 4.4.4, Ninja 1.13.2. The fresh cloud image lacked CMake/Ninja,
Clang and X11 development files. Verification used task-local tools/sysroot; no runtime dependency
was added to the repository. `NEXORA_LOG_DIR` selected a writable task directory because the cloud
user-data directory is read-only. Missing native Vulkan/display capabilities produced 14 skips.

Development was additionally configured with `NEXORA_BUILD_PROJECT_PLAYER=ON` and
`NEXORA_2D_GAME_TARGET=NexoraCrashGameSmoke` to compile both Player dispatch and standalone entry.
Shipping used the same optional entry targets.

| Command | Result |
| --- | --- |
| `cmake --preset linux-development` | PASS |
| `cmake --build --preset linux-development -j 8` | PASS |
| `ctest --preset linux-development --output-on-failure` | PASS: 251 passed, 14 skipped, 0 failed; 265 registered; 178.26 s |
| `ctest --preset linux-development -R '^runtime.crash_diagnostics$' -V` | PASS: expanded native crash suite, 0.23 s |
| `cmake --preset linux-shipping -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_2D_GAME_TARGET=NexoraCrashGameSmoke` | PASS |
| `cmake --build --preset linux-shipping -j 6` | PASS: monolithic Host, DedicatedServer, ProjectPlayer, TwoDPlayer and standalone game |
| `clang-format --dry-run --Werror <all touched C++ headers/sources>` | PASS |
| `git diff --check` | PASS |

The final full suite includes the expanded lifecycle fixtures and a native worker crash carrying
`Gameplay.worker-遊戲`, proving that non-ASCII fault context survives capture. Development and
Shipping were rebuilt after the Unicode-context correction. The focused crash suite also passed
before the final full regression run.

## Native subprocess acceptance

`Tests/Runtime/CrashDiagnostics.py` passed 18 independent scenarios, plus explicit failure for a
non-directory output root:

- Normal shutdown with nested Engine/session ownership, 64 concurrent records, control/NUL escaping,
  invalid gameplay wire length, and a recoverable game error in the critical journal.
- Rotation with bounded 4 MiB game files; previous-run/session isolation.
- Standard and non-standard application exceptions, terminate and abort.
- An ordinary worker crash with preserved Unicode context, a captured game job exception, a crashing job and owner stack exhaustion.
- Real dynamically loaded gameplay module crashes during create, start, update, fixed-update,
  stop and destroy, with the exact callback in the crash record.
- Update/fixed-update error return codes with callback/result/generation records.

Each scenario verifies metadata and module-map files, latest committed context, abnormal exit
when appropriate, and the last accepted Core/game log without explicitly flushing the async
consumer. POSIX fixture core dumps are disabled so the test proves the application's own log
capture instead of relying on a generated OS core.

A separately retained real `update-crash` child exited with `-11` (SIGSEGV). Its crash file contained:

```text
Nexora diagnostics version=1 application=CrashDiagnosticsFixture build=7c9164b6306e platform=Linux pid=22880 started_ns=1793640659589447758
fatal kind=signal code=0xb address=0x0 instruction=0x7f95283c3119 context=Gameplay.update
```

The surviving game journal ended with:

```text
1793640659591082 Info thread=16746384752898708300 [Game] accepted immediately before crash
1793640659591106 Info thread=16746384752898708300 [Gameplay.module] /workspace/Nexora/build/linux-development/Tests/Runtime/libNexoraCrashGameplayModule.so
1793640659591234 Info thread=16746384752898708300 [Gameplay] gameplay breadcrumb before failure
```

ASLR addresses/PIDs are observations of that one run, not stable golden values. Windows CI runs
the same applicable cases and checks the MiniDump signature when the system backend is ready;
that platform's result must be read from its actual CI/host run. POSIX symbolic stacks still need
matching OS core/native reports and symbols; this log path never unwinds in the fatal handler.
