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

## Review correction before merge

The original `16afa135` candidate passed full550 and all41 hosted checks. The final premerge guard
then found [a valid P2 review](https://github.com/jimlee1972/Nexora/pull/792#discussion_r4576735153):
the configured host's despawn hook called the session's metadata-only helper. This removed the
metadata before Reconcile could discover the deletion, while retaining bridge animations and a
ready overlap/sweep snapshot. PR792 was converted to draft; that candidate was not merged.

A new regression first reproduced a stale snapshot on the old implementation. The host now calls
`GameSpriteSession::Bridge().ForgetEntity`, matching the existing custom-scene path. After the32
successful CPU replacements, the test adds a collider and primes both query types, despawns through
the actual host callback before EndFrame, and checks Lifecycle with unchanged outputs. Rebuilt
queries then contain no hits; the same pool handle/source/style remains valid and composition
still draws. All three focused cases pass in0.37s. Only the host header and regression changed
among the19 implementation/build/test files; the complete gate was repeated for that correction.

## Complete delivery validation

- Final Linux configure, all-target build and unfiltered CTest: **550/550**, zero failures/skips,
  **2289.26 s**. The previous550/2273.63s and earlier550/2288.06s remain separate evidence.
- All **14 required local gates** and **20 actual GNU15 commands across18 translation units** pass
  on the frozen19 files after the reviewed cleanup correction.
- Full Shipping build and canonical SDK/deleted-source Unicode Player proof pass. Development
  independently passes the same five masks: **65,536** source/pool pixels per profile, unchanged
  central3D pixels, stable handles and native drain.
- Monolithic graphical Editor/Player build, **11 CPU cases** and the actual SDK Editor five-phase
  native test pass. Both Editor link modes retain **5,184** independent source/pool pixels each,
  authoring bytes/samples and stable handles through Stop/Destroy/native shutdown.
- Actual TwoD-OFF/testing-OFF Editor/Player build and capability4 package refusal pass. The prior
  no-native-Player registration proof is retained under an unchanged CMake checksum.
- Formatting, API/document utilities, changed Markdown/three bilingual pairs and unchanged root
  README are checked after this final evidence update, before committing the correction.

The [machine receipt](ED-WF-08-EditorGameServices-Cloud.json) retains commands, source/log hashes,
SDK records, native receipts and the red/green review regression. The corrected source requires
fresh exact-head hosted checks; original16afa135's41 successes do not validate a different head.
Logs remain under `/workspace/work/engine-goal-20261112/authoring/editor-main-services-review-fix`.

The native case proves the initial SDK generation and scene transitions, not32 native Zig
replacements or asset watching/reimport/publication. Follow-ups `nexora-whr.8.3.7` and `.8.3.8`
remain open. ED-WF-07/08, Workflow **5/9**, graphical Editor **0/8**, TwoD **9/11** and
physical/clean-host acceptance remain unchanged. Independently owned standalone reload work and
historical investigations are not closed by this supporting slice.
