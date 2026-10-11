# ED-M5 owning material tool document/history — Linux, 2026-10-11

Beads: `nexora-032.1.7`, depending on scalar tool `nexora-032.1.4` / PR 526.
The frozen document-only feature source is `c581086f79470d6357cbbc5028ffec6ab3d957b4`.
Review-only foundation `b5ea7e8e` starts from accepted Main
`1023b19aac8ad84ffd1b7b05a429c3e6f295cfad` plus SDK/scalar prerequisites and
separately committed production numeric/fixture corrections. It deliberately includes
no unrelated signed-bridge copy. Never merge the foundation; Main delivery requires
landed prerequisites, own-feature replay and independent current-head hosted acceptance.
This owner performs no file IO, signed admission, graphical control or GPU Preview.
Full Editor milestones remain **0/8**.

## Ownership and state transitions

`MaterialToolDocument` is noncopyable/nonmovable and belongs to its construction thread.
Its scope owns project UUID, asset UUID and nonzero generation; each successful change
advances a nonwrapping serial. Snapshots deeply own canonical source and MaterialAsset,
exact saved raw source and history/dirty observations. They borrow no document, source,
workspace, native table or callback.

Open uses production import/validation/export, retaining up to 64 KiB original raw source
and at most 1024 canonical bytes. Failed invalid/future/oversized replacement preserves
the current owner; replacing or closing a dirty owner requires explicit discard. Apply
validates owning bytes before staging one history entry. Equal canonical input preserves
serial, dirty state and pending Redo. Scope/serial/thread/read-only rejection conserves
current data, saved baseline and history. Undo/Redo retain at most 64 combined transitions.

AcknowledgeSave checks exact previous raw bytes and caller-confirmed published material
against current canonical state, retaining Undo/Redo and advancing only the saved baseline.
It does not publish a file, verify a filesystem write, confer writer/Play authority or
perform native calls. Hosts independently authorize invocation and recheck current
project/document/source, writer, recovery and Play before deferred authoring or publication.
Existing persisted/C/Gameplay schemas, module dependencies and class layouts are unchanged;
public C++ consumers rebuild.

## Actual acceptance coverage

The test loads the real production scalar-material module through the trusted low-level
PluginHost and applies actual native Edit output. This is not represented as signed Manager
or graphical acceptance. Original negative-zero bits/raw source and unrelated material
lanes remain exact; forty Undo/Redo/no-op cycles preserve state. Seventy actual edits prove
the 64-transition history cap and complete retained replay. Wrong-thread entry points,
stale observations, read-only, failed save acknowledgement, empty/unavailable callbacks,
invalid future/oversized sources and dirty close/replacement preserve current evidence.
Owning results survive unload/close; retired serials cannot replace the reopened owner.
A feature-OFF test path uses the public codec rather than pretending to load a missing plugin.

## Commands and focused results

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_CRYPTOGRAPHY=ON -DNEXORA_ENABLE_SLANG=ON \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON \
  -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON \
  -DNEXORA_FEATURE_SCALAR_MATERIAL_TOOL=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
```

Configured Linux toolchain and pinned ImGui/Vulkan sources were used. Current graphical
Development built **385 steps**, and real material/import/document focus passed
**3/3 in 0.57 s**, including document history **0.02 s**. Complete acceptance is recorded below; previous source focus is historical.
Xvfb/software Vulkan do not supply physical-display acceptance, and other-platform
execution is not attributed to Linux. Graphical reference-tool editing/preview/save,
actual scoped file publication and missing-backend controls remain separate work.

## Current standalone full acceptance

The frozen document-only feature passed the complete graphical Development suite:
**251/251 in 641.36 s**, zero skips, including actual native scalar/document history,
Inspector, prefab isolation, scene tabs/preview, Game View, Showcase and ProjectPlayer.
Minimal Monolithic Shipping passed **5 steps**, excluding Editor/scalar by profile.
The source owner performs no file publication, signed invocation, GUI control or GPU
Preview; those behaviors are not inferred from unrelated native regression tests.

## Explicit scalar-OFF / unavailable-module acceptance

A separate portable Development build uses provider NONE, Editor/SDK ON, graphical/native
backends, Slang/Zig, Showcase and ProjectPlayer OFF, and scalar feature explicitly OFF.
The current document/import targets built **40 steps** and passed **2/2 in 0.57 s**;
the document remains registered while the scalar native test is absent. Its document
path uses the production public codec and does not claim native plugin execution.
Shipping cache also confirms Editor/scalar are forcibly OFF.

Final documentation regressions passed **16/16 in 0.783 s**; both roadmap languages,
new evidence links, three touched C++ files and diff whitespace checks pass.
Root README remains unchanged.
