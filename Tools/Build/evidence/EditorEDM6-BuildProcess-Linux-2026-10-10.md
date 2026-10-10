# Bounded owning build processes — Linux cloud evidence

BuildProcess launches an explicitly authorized absolute executable with owning UTF-8 argv and
working directory on the existing Core JobSystem, without shell interpolation or document borrows.
One operation rejects replacement until its terminal worker is consumed. Owner calls serialize;
shutdown cancels and joins before lifetimes end. POSIX uses posix_spawn with an owned process group,
normal SIGCHLD policy and reserved unreaped direct-child identity. Windows creates suspended with
restricted inherited handles and assigns a kill-on-close Job Object before execution. Mobile without
a backend reports Unsupported. This is cooperating native process policy, not a sandbox.

Admission caps arguments at 256, executable/argv bytes at 32 KiB, and retained raw merged output at
256 KiB (default 64 KiB). Exact dropped bytes remain inspectable. Raw output may contain binary,
controls, paths or secrets: callers sanitize/redact before display/persistence. Environment is
inherited; the API persists no commands/output/environment or credentials. No endpoint is contacted.

Nonzero exits, signals, failed launch, incomplete drain, cancellation and stale scope never publish
success. The unsigned optional exit code retains native Windows status; failed launch/signals do not
fabricate a normal exit. Exited means actual zero only: verified artifacts, checksums, target manifest
and reproducible build publication remain separate host work. Cancellation stops managed descendants
and drains the direct child/worker; deliberately detached POSIX groups are outside that policy.

## Actual acceptance

The real standalone child fixture is copied into a Unicode working directory. Tests reproduce empty,
Unicode, quote/trailing-backslash and shell-metacharacter arguments exactly, verify actual cwd/merged
stderr and prove no shell marker is created. Actual exit seven and POSIX signal termination fail.
A 2 MiB flood retains only the configured 4 KiB tail with an exact dropped count and final marker.
A different owner scope rejects an actual zero exit. Missing executables report no invented status.

Queued cancellation uses the actual occupied single Core worker and produces no child output.
Running cancellation stops a twenty-second sleeper within three seconds. A real descendant fixture
proves managed tree termination and POSIX reaping, with analogous native handle verification on
Windows. Shutdown drains a running child and blocks new intake. Invalid NUL/count/byte/output budgets
preserve the retained observation. Windows-only fixtures additionally check full native exit bits
and malformed UTF-16 paths. These Windows branches are not claimed as locally executed on Linux.

Initial focused build completed 77 steps and 1/1 in 0.47s. Expanded native-failure/path contracts
passed 1/1 in 0.49s. Full Linux completed 405 incremental build steps and **219/219 in 512.37s**,
zero skips; minimal Shipping completed 74 steps. Canonical cwd fixture verification then passed
1/1 in 0.48s after two test-only build steps, accommodating native temporary-directory aliases.
The Windows fixture explicitly uses binary stdout/stderr so exact byte assertions do not depend
on CRT CRLF translation; the final fixture gate passed 1/1 in 0.48s after two more test-only build
steps. No production behavior, deadline or assertion was weakened.

~~~sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_BUILD_SHOWCASE=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON \
  -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
~~~

GCC 14.2, CMake 3.31.6, Ninja, Slang 2026.18, Zig, Xvfb/xdotool and software Mesa Vulkan were used.
editor.bounded_build_process is required by Linux CI. Hosted exact-head gates remain required before
acceptance; Linux does not establish local Windows/macOS or physical-device results. C++ consumers
rebuild; stable C/Gameplay ABI and module dependencies remain unchanged. Full milestones stay **0/8**.
