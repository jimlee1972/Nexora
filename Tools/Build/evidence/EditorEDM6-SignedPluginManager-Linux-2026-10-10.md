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

## Fresh signed Main integration — 2026-10-11

Only the manager feature, its accepted input correction and documentation were replayed
onto accepted Main `0eb7e9a6738d452013ff7bad0ca571ee3345e1ac`, after signed native
admission PR 484 and dormant-history PR 515 landed. No prerequisite/foundation was
merged. Bilingual conflicts preserve Main CI-cleanup evidence and locate manager support
under ED-M6; overview/contracts distinguish session management from persistent trust/recovery.
Product code and native assertions remain unchanged by this replay.

Full graphical/Cryptography/Slang/Zig/Showcase/native ProjectPlayer Development configure/
build passed **370 steps**, full CTest **245/245 in 615.67 s**, zero skips. Actual Core
manager **0.02 s**, 1x/2x widgets **0.10 s**, native manager **25.32 s** pass alongside
the accepted native Inspector, project upgrade, scene tabs/preview and normal close.
Minimal Monolithic Shipping passed **14 steps**, with Editor excluded by profile.
Full milestones remain **0/8**; fresh independent published-head checks and a clean/current
Main/head guard are required before ordinary squash merge. Older hosted heads do not
qualify this replay. No physical-host or other-platform local execution is claimed.

Final documentation regressions pass **16/16 in 0.595 s**; changed-document links,
12 touched C++ formatting and diff whitespace checks pass. Root README remains unchanged.

## Rendered source selection correction — 2026-10-11

The old hosted `b0c61350` native test reached real install/enable/revoke operations but
failed final `scene_selected=1`. The current parent-only replay starts from Main
`1023b19aac8ad84ffd1b7b05a429c3e6f295cfad`. It selects the actual source control in
the native host's initial 1280x720 dock layout before resizing to the existing
1600x1200 Extensions fixture. X11 readback uses the existing Inspector pixel helper;
the initial control is at x=523, rather than the old resized-layout x=595.

An initial probe incorrectly retained the old coordinate and failed; it is excluded.
The corrected focus passed **3/3 in 25.82 s**, including the real native workflow
**25.71 s**. The current graphical Development product rebuild passed **378 steps**.
Selection/source conservation, native lifecycle, read-only restart, Vulkan validation,
the 12-second operation deadline and the 120-second CTest timeout are unchanged.
The frozen current runtime/test source is `ba662ee8`; full current-source integration
acceptance is recorded separately below. Older failed hosted heads cannot qualify it.

The corrected frozen source passed the complete graphical Development suite:
**250/250 in 662.11 s**, zero skips, including native source selection and extension
lifecycle (**25.76 s**), reflected Inspector, scene tabs/preview and normal close.
Minimal Monolithic Shipping passed **14 steps**, excluding Editor by profile.
Documentation regressions passed **16/16 in 0.551 s**; changed-document links,
**11 touched C/C++ files** and diff whitespace checks pass. The original source
checkout remains unchanged. Fresh current-head hosted checks remain required.
