# Owning semantic scene comparison — Linux cloud evidence

`CompareSceneRevisions` synchronously compares caller-held base/local/remote byte revisions using
isolated temporary Worlds and the production SceneDocument parser/migration. Its output owns changed
stable entity/field paths and optional base/local/remote values. Missing whole sources and existing
empty fields are distinct. Runtime presence, parent/sibling order, local TRS, camera/light/mesh fields,
tracked authoring names, Euler turns and exact opaque type/payload values remain inspectable.
Quaternion signs are canonicalized to one hemisphere, including zero-w rotations, so q/-q do not
fabricate scene changes. Authored Euler hints remain separate and preserve intentional full turns. Paths
sort deterministically. Shared/local/remote choices are read-only per-field hints; unresolved conflicts
remain explicit. They do not certify a valid merged scene or publish data.

`SceneDocument::ReloadBytes` shares the existing versioned parser with file Reload. File Reload moves
its already owning bounded buffer, avoiding another full source copy. Successful reload has the same
replacement/generation/history boundary; rejected bytes preserve the existing document and history.
Comparison never reads/writes files, loads a plugin, changes live World/document/selection/Undo/Redo,
acknowledges a saved baseline or keeps an input borrow. Temporary document generations may advance the
process's monotonic generation allocator; existing live generations remain unchanged.

Each source is bounded at 8 MiB/4,096 entities, each semantic value at 64 KiB (32 KiB opaque binary),
each snapshot at 4 MiB and the result at 131,072 rows/16 MiB. These are logical byte/field budgets,
not total RSS or an allocation sandbox. Authoring-node capacity rejects during production-parser
ingestion before legacy hierarchy validation/migration; legacy parent validation visits each node/edge
once rather than repeatedly searching ancestors. Corrupt, unsupported, dangling authoring metadata, unsafe text
or over-budget revisions return no partial result and preserve caller data. Project/revision provenance
belongs to the serialized caller. Unknown payloads are complete hexadecimal bytes and type names,
without interpreting their private provider schema or rewriting references.

## Actual acceptance

Tests author real camera/child scene revisions with Unicode names, independent transform changes,
720-degree authored Euler turns and unknown empty/binary payloads. They verify explicit conflicts,
independent local/remote hints, identical concurrent changes, whole-source and object deletion,
deletion versus editing, missing versus empty values, owning output after caller mutation, sorted
repeatable results and unchanged live selection/generation/Undo/Redo. Legacy source migration compares
equal to its real canonical revision. Known Runtime component updates and actual sibling reordering
produce the corresponding stable fields.

Empty/corrupt/unsupported/truncated/dangling and oversized sources reject in all revision positions.
Exact 8 MiB valid source and 64 KiB opaque display values pass. Actual 32 KiB + 1-byte opaque payload,
aggregate field overflow with 64 real opaque components, 4,097 real entities and a combined result
from three distinct 4,096-entity revisions reject. Under-budget 63-component and actual 4,096-entity
snapshots pass. No racing writer, source publication, weakened assertion or skipped test is used.

Initial 74-step focused build passed 1/1 in 1.97s. Expanded two-step build passed 1/1 in 6.39s.
Final full graphical/native configure/build completed 403 remaining steps and **218/218 in 505.68s**,
zero skips; semantic acceptance passed 6.47s inside that full run. Minimal Shipping completed 74 steps.
The initial functional commit was a1f4a6a1 on accepted owner main 2e1b03a1. Automated review
then identified two corrections. Real identity, zero-w half-turn and general rotations
serialize with opposite signs and compare without changed fields/conflicts. A 20,000-node legacy
parent chain below 8 MiB rejects on its 4,097th authoring node in each revision position, preserving
existing document bytes/generation/history. The bounded ReloadBytes overload shares the production
parser and is used before comparison; ordinary Reload APIs retain their existing node policy.
Final corrected focused gate passed **1/1 in 6.38s**. Renewed full build completed 252 steps and
Linux passed **218/218 in 503.10s**, zero skips; minimal Shipping completed five incremental steps.
Corrected functional commit is **f8ebea16**. Exact final-head hosted checks
remain required before merge. The source/scene/gameplay wire formats are unchanged; C++ consumers rebuild
for the new parser/comparison APIs. Graphical conflict/provider presentation and reviewed merge
publication remain separate work; full ED-M4 and complete Editor milestones remain open (0/8).

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

GCC 14.2, CMake 3.31.6, Ninja, Slang 2026.18, Zig, Xvfb/xdotool and software Mesa Vulkan were used.
Focused commands select editor.semantic_scene_comparison; Linux CI requires its registration.
Touched C++ formatting and diff checks pass. The root README remains concise/unchanged; English and
Traditional Chinese supporting roadmap entries are synchronized. No local Windows/macOS or physical
host result is inferred from Linux, and no complete semantic merge/provider workflow is claimed.
