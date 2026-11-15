# Configured Editor main Game source services

Supporting task: `nexora-whr.8.3.6`, based on accepted main `d5800c86b037e9cbb2d46a9d8b70744013e769db`.

The main configured Editor previously reconstructed immutable source bindings during rendering
and did not attach main Game source services to its gameplay host. It now owns
`GameSpriteSession` on the existing Play World, preserving the authoritative Runtime scene session.
Current-frame composition retains gameplay source/style/animation changes and global pool values.
Both saved ScriptProject compilation and manually selected gameplay libraries receive the optional
generation-guarded parent/source API. Custom single-scene and source-free paths remain separate.
The C/Zig wire layouts, codecs and module graph are unchanged; C++ consumers rebuild.

## Reload ownership

The session's explicit fork copies scene metadata, bridge state and pool identities into an
already cloned Play World, retaining immutable asset/template owners. Its Clip resolver owns the
shared catalog, so destroying a previous owner cannot invalidate a branch. Rebind admits all
scene/entity memberships before any pointer changes. Neither operation opens native devices.
Existing single-scene bridge fork/rebind methods still reject the main Game router.

`PackagedPlayContext` stages World, Runtime scene session and Sprite owner together.
`PlayWorldReload::sprites` exposes only the candidate owner. `PlayTwoDApiProxy` factors the
existing generation gate across both custom and main Game bridges. Capture writes are rejected;
candidate effects remain private. Old cached tables are bound to retired World/metadata before
Stop/Destroy and library finalizers. Commit adopts the new owner before releasing staged World
storage. Canonical Scene services and legacy input snapshots remain available; this does not
attach the custom-scene rich UI/audio/input service registry to main Game.

## Focused acceptance

- `twod.game_sprite_session` covers 32 metadata/pool forks and rebinds, animation half-frame
  conservation, camera/style/World preservation, pool-iteration rejection and previous-owner
  destruction before the next generation resolves a Clip.
- `editor.packaged_sprite_play` covers 32 successful CPU module replacements and 160 failures
  injected during Create, Restore, Start, Capture writes and cached old source writes. It checks
  published World bytes, owner identity, source frame/style, pool size/handle/value, animation
  progress, retired callbacks and the composed frame after every accepted replacement.
- `editor.linux_packaged_source_services` opens the actual read-only Editor with saved
  ScriptProject settings. The normal observer/compiler builds the canonical Zig SDK script;
  initial admission is explicitly observed. Actual Game-canvas acquisition and D/W/S/A input
  exercise initial source, animation, Additive, unload and Replace. Five complete independent
  masks cover **5,184** source/pool pixels. Each phase compares **36** underlying 3D pixels
  against a separate source-free Editor and checks sampled authoring Scene pixels and all saved files.
  Entity remapping, one stable pool handle, SDK Stop/Destroy, texture release and native shutdown
  are checked. The focused native run passed in **71.25 s** with zero rejected UI textures.

The first CPU attempt requested the already loaded startup scene; its fixture was corrected to
use the other scene. The first native attempt compiled/loaded and rendered its initial mask but
did not click the Game canvas before sending gameplay keys. The driver now acquires and observes
the ordinary input capture, including its discarded acquisition batch. These failures are retained
in the external execution directory; neither required weakening a product or pixel oracle.

## Complete delivery validation

- Full Linux configure, all-target build and unfiltered CTest: **550/550**, zero failures/skips,
  **2273.63 s**. This includes the new actual configured Editor SDK case and existing custom,
  source-free, Inspector, persisted Build and native source/animation regressions.
- **20 actual GNU15 compiler commands across 18 translation units**, retaining the original
  producer/consumer definitions and warning options; all pass.
- Full Shipping build and canonical SDK/deleted-source Unicode Player proof: five independent
  masks totaling **65,536** source/pool pixels, unchanged central 3D, stable handles and native drain.
  The full Development suite passes that same Player proof independently.
- Monolithic graphical Editor/Player build, **11 CPU tests** and the actual configured Editor SDK
  five-phase native test pass. Both Editor link modes verify **5,184** source/pool pixels each.
- Actual TwoD-OFF/testing-OFF graphical Editor/Player build and capability-4 package refusal pass.
- A separate graphical/Zig/TwoD configuration without native ProjectPlayer retains the precompiled
  Sprite test and excludes the new saved-SDK compilation test. Its required native Player/Ninja
  prerequisites are explicit. The earlier full550 pass (**2288.06 s**) is retained separately;
  configure/build/unfiltered CTest were rerun after this final registration guard with the same550
  enabled test names and unchanged C++/Python implementation hashes.
- All **14 sequential local gates** pass on the frozen 19 implementation/build/test files.
  Formatting, API metadata and changed-document/bilingual validation run separately after the final
  documentation update. Root README and milestone totals remain unchanged.

The [machine receipt](ED-WF-08-EditorGameServices-Cloud.json) retains exact commands, source/log
hashes, SDK records and native receipts. All applicable hosted checks must pass for the unchanged
own PR head/base/tree before squash merge; final hosted evidence belongs to that PR. Execution logs
are retained under `/workspace/work/engine-goal-20261112/authoring/editor-main-services`.

The native case proves the initial SDK generation and scene transitions. It does not establish
32 native Zig replacements or asset watching/reimport/publication. Those remain parent work,
along with joint ED-WF-07/08 and physical/clean-host acceptance. Workflow **5/9**, graphical Editor
**0/8** and TwoD **9/11** remain unchanged. The independently owned standalone Player reload
task and historical investigations are not closed by this supporting slice.
