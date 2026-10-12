# Graphical persistent prefab source inspection — Linux, 2026-10-10

Beads `nexora-pmb.1.22`; this review depends on owning source inspection PR509.
The unpublished branch was reparented onto actual PR509 `bcb4829e` after verifying
its complete base tree equals the copied foundation `2f1160d1` (tree `600a905e`),
with an unchanged feature diff. No additional foundation PR is required.

The real scene Inspector displays source identity and retained revision for a
single selected bound node. Inspect source emits an owning project/file/node/
placement/scoped-source request. The native host rechecks scope and stopped Play,
modal, export, recovery and external-change policy before invoking owning source
inspection. Drawing performs no source IO. Reports explicitly describe captured
available/unresolved, current publication and scoped-source facts; refresh is an
explicit request. Reports carry no document borrow or write authority.

Actual 1x/2x mouse controls cover blocked interaction, valid once-only requests,
read-only inspection, invalid report rejection, selection replacement, hidden
Inspector frames and real close-modal cancellation. Inspection preserves prepared
scene bytes and clipboard; actual paste/Undo still restores the scene. Project,
file, document generation, node and placement identity scope reports and requests.

The native Xvfb/lavapipe fixture opens an actually saved format-4 bound scene at
retained revision1 while publication is revision2. Each physical inspection
verifies exact scoped diagnostics and unchanged scene/source/project files.
Deleting the retained archive produces unresolved status; restoring it and
explicitly inspecting again restores available status. Read-only reopen repeats
the complete sequence with the entire project file map unchanged after close.
The native host retains its initial 1280x720 geometry. Presented button position
is observed per gesture because the captured report can add a scrollbar and wrap
the UUID. No inspection gesture is retried and no UI model setter substitutes
for the actual native control. Vulkan validation diagnostics remain fatal.

Final focused UI/native **2/2 passed,5.89s**. Three consecutive cold native
workflows passed **17.48s**. A first fixed-coordinate diagnostic failed after
report-induced wrapping moved the button; that failed attempt is excluded.

Final graphical Linux Development configure/build passed (209 steps after exact
base reparenting). Full `ctest --preset linux-development` **245/245 passed,
618.54s**, zero failures or skips. Graphical shell, Cryptography/OpenSSL, Slang,
Zig, Showcase and native ProjectPlayer were enabled. Minimal Monolithic
`linux-shipping` configure/build passed (five actual steps).

Live instance override/revert/apply/rebase and complete Editor milestones remain
open (**0/8**). Linux acceptance does not claim physical display or other OS runs.
