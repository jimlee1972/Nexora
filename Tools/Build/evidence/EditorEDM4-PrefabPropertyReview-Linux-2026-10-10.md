# ED-M4 scoped graphical prefab property review/revert — Linux, 2026-10-10

Beads: `nexora-pmb.1.11`. Validated on main `64b20ea7` with the pending
save, isolation owner/controls, retained history, stable comparison, atomic snapshot,
property candidate and Inspector pointer synchronization slices. Foundation commit
`680264f4` records the exact dependency composition; those source implementations
match their separately validated originals. This evidence does not claim their main
acceptance or completion of the entire Prefab/Editor milestone.

Graphical Development configure/build passed (338 full incremental steps).
**246/246** tests passed, zero skips, **631.57 s**. Minimal Monolithic Shipping
configure/build passed (5 steps). Final focused Core/UI/native flows passed **3/3**,
**31.99 s** before the table/presentation polish; later authority/UI focus passed
**2/2**, **0.18 s**, and the final full gate verifies every current change.
The final native prefab flow passed **31.56 s**; 1x/2x controls passed **0.15 s**.

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_CRYPTOGRAPHY=ON -DNEXORA_ENABLE_SLANG=ON \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON \
  -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
```

The actual owner reviews mixed name/TRS/opaque edits without mutation; a confirmed
full property revert restores complete source values as one guarded Undo/Redo.
Document/session generation, keys, selection, clipboard and saved baseline survive.
Published variant overrides remain independently dirty after reverting to their exact
retained base, even after that base's latest source advances. Regular prefabs review
against their last publication. Stale content, changed publication, invalid archive,
replaced scope, false authorization, read-only, recovery and external workspace
changes reject without source publication or partial live changes.

Real 1x/2x controls emit scoped Review and separate confirmed Revert requests.
Cancel preserves edits; scope replacement revokes confirmation; read-only controls
retain Review and suppress Revert. The native Vulkan/Xvfb host edits both variant
name and position, reviews them, explicitly confirms revert, and verifies all project
files stayed byte-identical. One Undo restores both edits and an explicit Save records
them; one Redo restores the exact source properties and another explicit Save records
that state. Stable identities, unknown bytes, original scene and base survive. A
read-only actual restart inspects the revision and preserves every project file.
All prefix/remaining native diagnostics reject VUID/SYNC validation errors.

Review/candidate data are owning bounded observations, never publication authority.
At execution, the host requires stopped Play and resolved project/modal/export/close
state; the session independently rechecks UUID/root/generation, asset publication,
PreparedSave and exact stored immutable source. Unsaved structural/new field identities
require explicit Save before review; compatible revert requires identical node sets.
Reference metadata rows remain inspection only. Results/history are not a total
allocator-memory quota. Source apply, targeted selection, structural reconciliation,
persistent live instance bindings and reference rebase remain separate work. Public
C++ consumers rebuild; stable C/Gameplay ABI is unchanged. Full Editor milestones
remain **0/8**. Rebase to accepted main and require all current hosted checks before merge.
