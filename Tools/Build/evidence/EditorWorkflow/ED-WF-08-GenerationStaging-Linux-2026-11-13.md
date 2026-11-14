# ED-WF-08 isolated TwoD generation staging

Date: 2026-11-13 (UTC). Supporting task: `nexora-whr.8.2.1`; parent `nexora-whr.8.2` remains open.

`GameplayGeneration` owns a single-scene or catalog Play World, Sprite/collider/Tilemap state,
scene requests/revisions, exact pool owner/index/generation/free lists and bridge camera/query/
animation values. Quiescent `Stage()` allocates an unpublished independent candidate. Wrong-thread,
entered callbacks, pool iteration, scene publication, recursive resolver-copy staging and failed
allocation reject without publication. Immutable Clip owners survive staging. Candidate and retired
callbacks continue targeting their own World/pool; all old code retires before publication.

The [contract](../../../../Engine/TwoD/GameplayGeneration.md) defines resolver lifetime, borrow/work
quiescence and strict Runtime transaction duties. External content/audio/UI/storage/window services
need separate isolation or rejection. Production Player/Editor reload, file watching and native
reload pixels remain separate integration work. No Runtime-to-TwoD dependency or C/Zig ABI changes.

## Actual acceptance

Full Linux Development passes **525/525**, zero failures/skips,
**1992.24s**. All11 configure/build/test/native gates pass, including Shipping Full/native
source-animation linkage regression, Monolithic five-test consumers and TwoD-OFF/testing-OFF
graphical Editor/native Player builds. The [machine receipt](ED-WF-08-GenerationStaging-Cloud.json)
retains complete test results,25 source/build/fixture hashes (24 own plus the inherited native Clip
fixture), exact commands and native artifacts. All six own affected C++ translation units additionally
compile with GCC15.2; the full engine matrix uses GCC14.2. Hosted exact-head platform CI is a separate
delivery gate. No local MSVC, macOS or mobile execution claim.

`twod.gameplay_generation` covers independent World/metadata/query values, half-frame paused clocks,
style preservation, live/stale/reused pool handles, Tilemap chunk state, pending/completed catalog
requests and rebinding, failed resolver allocation, wrong-thread calls and callback/copy reentry.
The focused CPU/real-library pair passes2/2 in4.74s before the full gate.

`twod.source_reload_transaction` compiles five canonical SDK modules, then performs **32** unique
library replacements and **128** prepare/restore/reloaded/schema rejections. Every rejection leaves
the old module callable; complete World/Sprite/Tile snapshots, camera, count-only animation probe,
cached collision query, source/style, two pool handles and allocation balances stay valid. The
independent counter/tick model advances. Source11:12 is reimported to Atlas18:19/region73/Pivot3
without changing World anchors or handles. CPU extraction verifies available source geometry.

A real old-library job retains a work lease, modifies module-only data and returns before capture.
The pointer is cleared and lease released before retirement;100 job ticks migrate through the80-byte
SDK state envelope. New leases for the closed/stale generation reject. Old normal and rejected
candidate libraries are observed unloaded with no probe handle pinning them. After Unload, both
module entities and its pool instance are gone, the embedding's sentinel instance survives and
all host allocations balance. No worker calls owner-thread World/TwoD APIs.

The first test oracle incorrectly expected RestartRequired for the SDK payload schema mismatch;
existing SDK/Runtime correctly return RestoreState after staging. That fixture expectation and
rollback count were corrected without changing Runtime/SDK behavior. Initial CPU test compile
mistakes and three failed oracle runs are retained separately. The earlier full525 run was
intentionally interrupted with actual exit130 and zero completed test results when parent Source
CI exposed the inherited native Clip fixture compiler warning; it is not full acceptance.

A later concurrent full attempt also retains the existing Prefab graphical180s timeout and actual
interruption130, with fixture/input hashes. The same Prefab test passes separately and final full
suites run sequentially without changing its deadline or assertions. Failed/partial attempts remain
separate from final acceptance.

## Reproduction

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development --output-on-failure
ctest --test-dir build/linux-development -R '^twod\.(gameplay_generation|source_reload_transaction)$' --output-on-failure
cmake --preset linux-shipping -DNEXORA_SHIPPING_PROFILE=Full -DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_EDITOR_SDK=ON -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_ZIG_EXECUTABLE=/workspace/work/engine-goal-20261112/tools/lib/python3.12/site-packages/ziglang/zig -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-shipping
python3 Tools/Build/ValidateApiM6.py .
python3 Tools/Build/TestDocumentationCI.py
```

The machine receipt's hashed runner gives complete native/Monolithic/OFF commands. Existing
Development/Shipping animation regressions retain30 native frames,60 GPU instances,two visible
Sprites and all five asymmetric phases. They verify linkage/regression, not new reload pixels.
Unconfirmed Vulkan residency issue `nexora-ko9` remains open. Complete ED-WF-07/08, production main
packaged source services and all physical/clean-host acceptance remain open. Workflow **5/9**,
Editor **0/8**, TwoD **9/11** remain unchanged.
