# Concurrent Editor integration — Linux cloud, 2026-10-12

Base: main `4908e35654debdd45f967e06e7b501216bd703d8`.
Beads: `nexora-nqn.12`; retained global Chrome async feature: `nexora-62u.2.8`.

## Accepted contract restoration

Concurrent older-branch integrations duplicated native C ABI/callback and scalar material targets,
removed accepted prefab source/test registrations, erased isolated and live-placement declarations
and graphical owners, and left combined property transactions inconsistent. Preserve the unique
union of delivered source/targets and restore existing targeted review, source apply, instantiation,
reference rebase, placement overrides/rebase, and owning test geometry. Keep the new scoped managed
native invocation API and register its existing production-material/recursive-fixture test.

Property application uses one staged helper for ordinary properties, isolated prefab base reference
changes, and live placement revision changes. Undo/Redo restores both kinds of metadata; unchanged
properties do not erase pending placement Redo. The additional combined-history regression retains
all prior placement identity, sibling order, generation, opaque-byte and atomic failure assertions.
Restore the existing owning source publication implementation and native shortcut/input handshakes;
all writer/project/Play/consent guards and test deadlines remain intact. Public C++ clients rebuild;
stable C/Gameplay ABI, source format, module dependencies and physical-host acceptance are unchanged.

## Validation

The complete Linux Development gate passed **243/243**, zero failures/skips, in **836.69 s**:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

The restored placement snapshot/history, scoped managed native invocation and Chrome importer tests
passed. Native isolation (88.30 s), reflected Inspector (14.92 s), plugin manager (27.14 s), source
rebase (13.89 s), source inspection (5.82 s), overrides (12.50 s), selected revert (23.38 s), and the
actual 1x/2x Chrome display fixture passed. Hierarchy lifecycle benchmark passed in 176.28 s.
Linux Shipping Monolithic configure/build also passed with testing OFF and Editor stripped:

```bash
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

External task-local X11/Vulkan SDK overrides remain outside version control. GCC 14.2, Dear ImGui,
Xvfb, Mesa lavapipe and the existing managed proxy were used. The 18 changed C++ files passed
clang-format dry-run; documentation routing passed 16/16. CMake registers each native tool fixture
and target exactly once; platform-exclusive surface tests retain their established branches.
Root README remains unchanged. Windows, macOS and physical display/GPU acceptance did not run
locally. Earlier PR537 gates prove its original Chrome slice only, not this repaired-main change.
Fresh hosted CI results and the merge identity are tracked in the repair PR and Beads.
