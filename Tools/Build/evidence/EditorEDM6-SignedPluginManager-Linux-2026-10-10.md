# Graphical signed package manager — Linux acceptance, 2026-10-10

The native Editor owns session publisher keys, capability policy, verified package review and
installed metadata through PluginManager. Widgets issue owning project/scope/configuration requests;
the owner rechecks access, Play/export, modal, recovery and external-change gates. Installation never
enables code. Opening a project or restarting restores neither publisher keys nor enabled providers.

## Acceptance

- Full Linux Development graphical/native configure/build/CTest, with actual OpenSSL cryptography,
  Slang, Zig, Showcase and native Project Player: **236/236 passed**, zero skips, **589.01 seconds**.
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
