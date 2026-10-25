# Owning multi-scene capture and cooker

`GameProjectExport.h` supplies the common Editor/CLI build prerequisite. `CaptureGameProject`
consumes an explicit project workspace, build settings and current CPU Content/mesh/material
catalogs on their authoring owner thread. It validates settings/file scope and reconciles Content
UUID renames, loads every enabled **saved** Scene into an isolated World/SceneDocument, and returns
owning runtime snapshots plus copied referenced mesh/scalar-material inputs. Disabled scenes are
omitted, including missing disabled scene files. It does not replace the current Editor document,
write settings, publish a package or authorize a stale caller. Recovery and unsafe/missing enabled
paths reject. Returned inputs retain no workspace/document/catalog borrow.

`CookGameProject` is pure synchronous CPU work. It borrows one immutable input for the call and
uses `CookStaticProjectData` for each captured scene, sharing the existing geometry/material codecs,
cooker, tangent authority and complete StaticView scene validation. It merges exact closures by
full UUID; same UUID with different cooked bytes/type/dependencies rejects. Unused supplied assets
are omitted subject to the existing supplied-input admission limits. No source lookup, native GPU
work, module execution or publication occurs in this cooker. Independent inputs may cook in parallel;
ordinary allocation exceptions propagate.

The merged package uses the [Runtime game catalog contract](../Runtime/GameProjectPackage.md),
including startup membership, unknown required behavior rejection, global resource collisions,
canonical encoding and aggregate limits. One frozen decoded mesh/material owner is shared across
resolved scene templates. Authoring entity IDs remain local until Runtime instance remapping.

A publication host must retain/recheck exact settings/source baselines, workspace access/recovery,
project/content generations, catalog snapshot identities and cancellation before atomic publication.
This capture/cooker alone provides none of those publication permissions. The graphical game-build
coordinator, reproducible shared CMake entrypoint, Editor Play use, native Player transitions and
final distribution remain dependent work. Existing StaticView Export UI and its publication guards
continue unchanged; complete Editor milestone acceptance remains open.
