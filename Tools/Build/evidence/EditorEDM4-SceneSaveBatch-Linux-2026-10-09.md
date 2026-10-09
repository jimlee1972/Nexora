# Coordinated immutable scene publication — Linux cloud evidence

The serialized authoring owner can prepare and publish up to sixteen named SceneFileSessions in
one current writer project. Prepared scene bytes, exact originals and next session baselines own
their storage. The aggregate original/output payload limit is 128 MiB, not a total RSS guarantee.
Invalid, stale, externally changed, duplicate, ASCII case-colliding, aliased, read-only, recovery
or unsafe-path input rejects before source publication. Unknown component bytes, Euler turns,
selection, identities and per-document Undo/Redo remain intact.

All originals/outputs and sibling stages verify before sequential native replacements. The bounded
binary project-UUID manifest distinguishes preparing, prepared, committed and rolled-back phases.
Preparation authorizes no source writes. Recovery preflights all originals, outputs and current
destinations before restoration; foreign changes, corrupt payloads/manifests and unknown entries
retain inspection data. Original scene copies remain ordinary .scene files. Completed phases allow
cleanup retry after earlier cleanup removed some copies. Uncommitted Discard restores originals;
committed Discard only finishes cleanup and keeps acknowledged outputs. Workspace authoring,
export, save and shutdown recovery gates recognize the batch journal.

A successful batch acknowledges every baseline only after all destinations verify and the commit
record publishes. Interrupted publication rolls back or honestly returns RecoveryRequired with
dirty documents and originals retained. PublishedRecoveryRequired reports a complete acknowledged
batch whose cleanup remains gated. Independent readers do not observe a filesystem-wide atomic
snapshot; the host stops/drains readers. This does not promise fsync/power-loss durability, hostile
concurrent filesystem isolation, Unicode-wide case folding or graphical additive acceptance.

## Actual acceptance

Real file-backed tests exercise successful dirty multi-document publication and independent reopen;
opaque metadata/selection/Euler turns; preserved Undo/Redo and exact external-change baselines;
stale/project-switch rejection; null/duplicate/excessive inputs; occupied staging, aliases and
portable ASCII case collisions; genuine sixteen-document publication; actual oversized aggregate
payload rejection with a valid bounded subset; second-file interruption after first publication;
new-destination rollback; missing stage and late authoring edits; foreign/corrupt/unknown recovery
inputs; restarted writer/read-only observers; and committed cleanup retry with missing earlier copies.
The private owner-thread interruption seam invokes no production plugin callbacks.

Review feedback identified two permanent-gate risks at initial metadata failure and final directory
retirement. Recovery now retires a strictly empty ordinary journal without inferring a phase or
writing any canonical source. Failed final directory removal best-effort restores the bounded phase
manifest; occupied or invalid journals still preserve every entry. Real failed ostream metadata
writing leaves both original files and dirty baselines intact and permits a subsequent full save.
A late entry injected after manifest removal blocks the real directory removal, retains the
committed phase and unknown entry, and then resumes cleanup without rolling back acknowledged
outputs. A manifest-less occupied directory rejects; an empty terminal directory only retires itself.

Initial focused target built 72 steps and passed **1/1 in 10.97s**. Related batch/prepared/workspace/
scene-file/external-save regression passed **5/5 in 12.03s**. The first full graphical/native build
passed 390 steps and **213/213 in 477.03s**, zero skips; minimal Shipping passed 74 steps.

After integration onto accepted tool-capability main
**5f46215ed5e062139e5a3c1a6646002e2401a85f**, the 270-step incremental graphical/native build
passed. The complete gate passed **214/214 in 467.50s**, zero skips; minimal Shipping passed its
five-step incremental build. This integration retains the already accepted native Save observation
helper and metadata acceptance. No helper/assertion/deadline changes belong to this batch patch.

After those review fixes and integration onto accepted overview main
**8e9b153966c1b3c94aefae4ae197b59fb08b6387**, the focused target built 30 steps and passed
**1/1 in 10.20s**. The full graphical/native build passed 279 incremental steps and
**216/216 in 505.12s**, zero skips; minimal Shipping passed 14 incremental steps.

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

GCC 14.2, CMake 3.31.6, Ninja, Slang 2026.18, Zig, Xvfb/xdotool and Mesa software Vulkan were used.
Touched C++ formatting/diff checks pass; Linux CI requires the batch acceptance registration.
Fresh final-head hosted checks are required before merge. No Windows/macOS or physical-host result
is claimed from this Linux execution. The rebuild-required C++ API changes no scene/gameplay schema.
Graphical document ownership/tabs, persisted composition and full ED-M4 remain open; milestones 0/8.
