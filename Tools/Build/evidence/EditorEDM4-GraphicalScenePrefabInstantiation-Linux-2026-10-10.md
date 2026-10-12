# Graphical prefab instantiation into an active Scene — Linux, 2026-10-10

Beads `nexora-pmb.1.20`. Review-only foundation `be43fd2d` combines graphical
rebase PR503, nested materialization PR502, accepted atomic forest PR482,
persistent bindings PR505 and guarded project owner PR506. Never merge this
foundation; replay only the graphical feature onto main after prerequisites.

`Instantiate in scene` requires a clean published isolated prefab and writable
active scene. The host rechecks project/owner/document/source-file/destination role,
published revision, complete source closure and prepared target; one explicit
action imports bindings and properties into the active scene's independent Undo.
Isolation document/history, prefab sources and archived versions remain separate.
Only ordinary Scene Save writes placement records and existing startup selection.

- Development configure/build passed (338 actual steps).
- Actual 1x/2x control fixtures passed blocked/reference-role/dirty/stale requests,
  owning request, one scene Undo/Redo, clipboard and real Save/reopen bindings.
- Initial focused run: 13/14 passed; native executed placement/Scene Save/Undo/Redo,
  then its overstrict file set rejected the existing Scene Save startup setting.
- Diagnostic native run identified exactly `.nexora/scenes/Main.scene` and
  `.nexora/scene-session.ini`. `SceneFileSession::RememberCurrent` writes the latter
  after ordinary Save. Final assertions retain all other files and verify exact
  schema/project UUID/current relative path, not an unrestricted file allowance.
- Final focused gates **14/14 passed,99.28s**, including actual X11/Vulkan native
  placement/Save/history/reopen/read-only acceptance **88.71s**.
- Final full graphical Development **251/251 passed,698.12s**, zero failures or
  skips. Minimal Shipping configure/build passed (five actual build steps).

Native acceptance checks exact variant13 source bindings, original scene content
on one Undo, exact bound bytes on one Redo, source/archives preservation and
read-only disabled placement after reopening the bound scene. Logical source/
metadata budgets apply; structural instance override/rebase and full Editor
milestones remain open (**0/8**).
