# Graphical three-way prefab rebase — Linux, 2026-10-10

Beads `nexora-pmb.1.17`. Review foundation combines source apply PR498, reference
history PR499 and core rebase PR501; the foundation has no PR and must not merge.
After prerequisites reach main, retarget only this feature and require fresh hosted CI.

Managed four-CPU graphical Linux with cryptography, Slang, Zig gameplay,
showcase and native Project Player; pinned ImGui/Vulkan dependency overrides.

- Actual 1x/2x controls plus core/reference focused gates: **3/3 passed, 0.57s**.
- Expanded read-only and blocked-authoring consent gates: **3/3 passed, 0.51s**.
- Native X11/Vulkan complete workflow: **1/1 passed, 79.08s**.
- Full Development: **248/248 passed, zero skips, 685.71s**.
- Linux Shipping configure/build: **passed, five build steps**.
- Commands: `cmake --preset linux-development`, `cmake --build --preset linux-development -j4`,
  `ctest --preset linux-development`, `cmake --preset linux-shipping`,
  `cmake --build --preset linux-shipping -j4`; graphical/native feature options above enabled.
- Logs: `build-console/rebase-ui-{first,readonly,native-first,full,shipping}-*.log`.
  Separate native screenshot probe verifies actual layout and is excluded from acceptance.

Public 1x/2x actual pointer controls show owning old-base/local/current-source comparison,
require an explicit name conflict choice, disable unresolved/unprepared application, forward
owning Prepare choices, and require a separate confirmation. Cancel preserves every byte.
Scope changes and blocked authoring clear consent; unblocking does not resurrect it. One actual
Undo/Redo restores properties/reference together; explicit wrapped Save retains variant1 and
clean reopen retains reference2. Real read-only workspace review and choice preparation can
produce a candidate, while UI and authoring owner both reject application without mutation.

Native acceptance preserves prior create/edit/save/variant, full/selected revert and explicit
source-apply workflows. Source advances through actual isolated controls to revision6 while
variant10 retains source4; Review rebase displays two whole-group conflicts. Actual dropdowns
take the new source name and keep local position11. Prepare and confirmation preserve all files;
confirmed Rebase changes only live properties/reference. Save11 publishes the mixed result with
source6 and retains exact variant10. One Undo/Save12 restores the old name/position/reference4;
one Redo/Save13 restores the mixed result/reference6. Stable identities, unknown opaque bytes,
original scene and source6 stay unchanged. Source advances to7 through real controls, and
read-only reopen of variant13 can review/prepare latest source but never rebase or save. Every
project file remains unchanged by the read-only flow; Vulkan VUID/SYNC diagnostics reject.

No reusable Scene/runtime schema, stable C/Gameplay ABI or module dependency changes.
Structural identity reconciliation and persistent nested/live-instance editing remain unfinished.
Full Editor milestones remain **0/8**.
