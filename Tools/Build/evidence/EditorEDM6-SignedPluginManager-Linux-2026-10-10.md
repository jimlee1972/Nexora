# Graphical signed package manager — Linux acceptance, 2026-10-10

The native Editor owns session publisher keys, capability policy, verified package review and
installed metadata through PluginManager. Widgets issue owning project/scope/configuration requests;
the owner rechecks access, Play/export, modal, recovery and external-change gates. Installation never
enables code. Opening a project or restarting restores neither publisher keys nor enabled providers.

## Acceptance

- Full Linux Development graphical/native configure/build/CTest, with actual OpenSSL cryptography,
  Slang, Zig, Showcase and native Project Player: **241/241 passed**, zero skips, **620.96 seconds**, after 320 build steps.
  Minimal Monolithic `linux-shipping` configure/build also passed, **5 build steps**.
- Final focused Core/widget/native acceptance: **3/3**, **20.38 seconds**, including the actual native
  window **20.27 seconds**. Real widgets run at 1x and 2x scaling. Ctrl+Alt+E opens Extensions after
  Scene focus; modified shortcuts preserve the Scene tool. Close/Play/modal guards reject actions.
- Actual RFC8032 public test keys/signatures and compiled cooperative, legacy and dishonest-ABI
  modules verify review/install/enable separation. Source mutation after review cannot replace owned
  bytes; installation copies exact bytes without constructor execution. Enable registers real services;
  disable and key revocation stop/unload the real provider and revoke service visibility.
- Codec truncation, schema/length overflow, unsafe paths, corrupt discovery, immutable version conflict,
  occupied staging, tampering, stale project/configuration, dependency policy and unavailable providers
  preserve source or installed evidence. Legacy mappings report RestartRequired and block unsafe removal.
  Actual binary ABI failure retains Rejected state and the real reported ABI/loader diagnostics.
- Recovery evidence and external workspace bytes revoke mapped services and cancel review; neither is
  removed by the manager. Resolution permits explicit re-enable. Read-only review remains available;
  install, enable and removal cannot write or execute native code.
- Xvfb/xdotool exercises real UI key entry, permission configuration, package review, install, enable,
  disable, re-enable and key revocation, then normal close and read-only restart. Installed bytes and
  project/scene/input bytes are checked. Actual constructor/registration/stop/unload events prove the
  lifecycle; restart does not auto-enable. Vulkan validation output is rejected by the acceptance script.

Commands: the repository `linux-development` preset with graphical shell, Slang, Zig, Showcase,
Project Player/native and Cryptography enabled (`NEXORA_CRYPTOGRAPHY_BACKEND=OPENSSL`),
`cmake --build --preset linux-development -j4`, `ctest --preset linux-development`, followed by
`cmake --preset linux-shipping` and `cmake --build --preset linux-shipping -j4`.
Pinned ImGui/Vulkan source overrides reuse their exact repository dependency revisions.

Native extensions remain trusted in-process code. Permission declarations do not sandbox filesystem,
memory or dependencies. Session keys are explicit user configuration, not package-enrolled credentials.
Only the desktop Linux sealed-image native backend is implemented; Android and other native backends
fail closed. Hostile filesystem races, native crash isolation, power-loss recovery, persistent trust,
network distribution, automatic updates and graphical third-party tool contributions remain open.
This is a supporting ED-M6/7 slice; all **0/8** complete milestone counters remain unchanged. Windows,
macOS, Android and iOS were not run in this Linux cloud environment.

## Current accepted-main integration

The latest full run includes accepted main
`638e982e64a937d97d94c804a1becb4b2ed55301`, Crypto PR #477 head `6e6a419f`,
and the current signed-admission implementation. PluginManager, panel and
Core/widget/native fixtures are unchanged from the original validated feature.
Both build-console and extension-manager root interaction guards remain active.
The current main Inspector and real native build-console tests run in this
**241/241** gate; Shipping remains successful (5 steps). Documentation-only
admission integration evidence does not change the validated runtime paths.
Published-head hosted checks are independently required before merge.

## Hosted native input synchronization correction

Hosted jobs `114178847271` and `114178847260` on `82f07194` failed actual
installation/selection assertions. A captured native window showed that Ctrl+Alt+E
had not opened Extensions. Preserve all assertions; the test now spaces modifier/key
transitions by 100 ms, separates movement/down/up, and settles the selection layout.
The rebuilt native host/fixture on main `64b20ea7` and signed admission `317b2abd`
passed eight consecutive complete Xvfb/lavapipe runs, **202.48 s** total. Each run
checks immutable reviewed installation without autoexecution, explicit enable,
cooperative disable/reload, publisher revocation and read-only restart without
implicit trust or execution. Failure cleanup uses bounded process waits. This
changes only native test synchronization; it does not weaken permissions, trust,
source preservation, lifecycle or Vulkan validation assertions. New hosted checks
on the corrected head are required; the older failed head cannot merge.
