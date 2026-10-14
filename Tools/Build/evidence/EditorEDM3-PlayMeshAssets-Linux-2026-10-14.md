# ED-M3 frozen Play mesh assets — 2026-10-14

Beads: `nexora-rd7.2.7`. Initial base: merged PR592/main `d6ea19a8`; upstream PR591/main
`20736fca` is integrated. Prepared separately from immutable PR593 diagnostics; validated dependency commit `06bbf8f7` is
now integrated. Publication waits for its merge and proven base/source tree equivalence; any source
difference requires integration and revalidation. [Raw scope and staged source hashes](EditorEDM3-PlayMeshAssets-Linux-2026-10-14.json).

## Change and ownership

The V3 ABI already declares `resolve_asset`, but the native Editor left it null. Game View also
published its Play mesh catalog only after module Create/Start, so startup could not resolve an
imported project mesh. `FreezeGameMeshes` now validates project generation and creates one immutable
owning CPU catalog before cloning/starting the module. Both native renderer and module binding
share it; failed copy admission changes no World. Existing Play catalog allocation is shared,
without adding a per-call cache. Authoring reimport/delete/replacement cannot change the frozen
revision. The loader receives an already immutable snapshot, retaining it through Stop/Destroy
and releasing it after module teardown, including failed loads. Calls are owner-thread serialized;
no World/entity/name/native borrow or file IO crosses the resolver.

The callback returns the exact existing 64-bit MeshResourceId for imported mesh UUIDs. Full UUID
identity is rechecked after resource lookup; distinct UUIDs that fold to one resource cannot alias.
Zero/unknown/non-mesh UUID or null output returns InvalidArgument; unbound host returns Lifecycle;
provider-free embeddings return Unsupported. Failures preserve output. Nonzero snapshot generation
is required, and the application freeze helper also checks current project generation. Other asset
types and dynamic materials remain open. Stable C wires/capability bits/resource derivation,
scene/project formats, public host layout and module dependencies are unchanged; matching private
C++ consumers rebuild. Existing native residency ownership and full physical0/8 remain separate.

## Independent evidence before diagnostics integration

The initial availability regression builds against the old host and fails with `Play mesh asset
resolver is unavailable`. The complete candidate regression passes32 Play cycles, checking startup
resolution, full-width handles, zero/unknown/colliding UUID rejection, absent provider, invalid
snapshot generation, Editor World rejection, clone mesh spawn, frozen versus reimported renderer
geometry, same-count catalog replacement, module failure/unload, weak catalog release and unchanged
source/Undo cursor. Frame-owned geometry stays valid after Stop. An additional weak ownership check
verifies failed Start releases its catalog after Destroy.

A controlled fault replaced full UUID lookup with only folded resource lookup. The collision
regression fails with `rejected asset request changed output/result`; correct source is restored
byte-for-byte, rebuilt, and four Play regressions pass (0.13s). Assertions/deadlines are unchanged.

A C-ABI-only native fixture resolves in Create/Start, independently compares its handle against the
actual clone MeshRenderer wire, spawns a replacement mesh, deletes the clone original, and moves the
new entity on fixed ticks. The real Xvfb/lavapipe Game View test requires pixel motion, pause/step/stop,
Console delivery, unchanged source and successful resolution through Stop/Destroy. It passes1/1,
4.85s. This is real dynamically loaded mesh rendering, not only a callback count.

## Combined-source validation

After integrating upstream20736, targeted Play and actual native mesh tests pass5/5,5.17s
(83 rebuild actions). With the reviewed PR593 dependency, all six focused Play/native tests pass
(4.95s), including exact asset service-error increments and mandatory native diagnostic markers.

- `cmake --preset linux-development`: pass.
- `cmake --build --preset linux-development`: pass,412 rebuild actions.
- `ctest --preset linux-development --output-on-failure`:265/265,826.04s,no skips.
  This includes the seven upstream2D cases, 100k lifecycle, diagnostics and all native asset/input cases.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: pass,29 actions.
  Minimal/Monolithic with Editor/ImGui/SDK off; no Shipping graphical Editor claim.
- `ctest --preset linux-development -R '^editor.play_(mesh_assets|diagnostics|gameplay_module|scene_services|physics_services)$|^editor.linux_native_play_mesh_assets$' --output-on-failure`:
  6/6,4.95s. Native mesh passes again in the full gate,4.77s.
- `python3 Tools/Build/TestDocumentationCI.py`:16/16. Seven touched C++ files pass
  `clang-format --dry-run --Werror`; nine source/config hashes, translated evidence links,
  unchanged root README and `git diff --check` pass.

Publication waits for PR593 merge and proof that its base tree and this validated source remain
identical after replay onto main. Any source difference requires integration and revalidation.
Final-head hosted CI and own PR delivery remain pending; full ED-M3/physical0/8 stay open.
