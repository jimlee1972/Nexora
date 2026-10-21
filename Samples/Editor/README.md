# Editor SDK executable samples

`NexoraEditorReflectedArraySample` is a public-header C++ consumer of the counted reflection
contract. It creates its own temporary project, performs real document transactions, saves a scene,
reopens it in a fresh read-only World/file session and removes the temporary project after owners
and file locks have been destroyed. It needs no window, Dear ImGui, physical GPU or native plugin.
It accepts no project/output paths, loads no project extension and starts no transport service.

```bash
cmake --preset linux-development
cmake --build --preset linux-development --target NexoraEditorReflectedArraySample
ctest --preset linux-development -R '^samples.editor_reflected_arrays$'
build/linux-development/Samples/Editor/NexoraEditorReflectedArraySample --help
```

The expected success line reports authority/stale checks, append/edit/reorder, Undo/Redo, scoped
Save and read-only reopen. Failures return1 with a diagnostic; unsupported arguments return2 before
creating a project. `--help` returns0 without changing files. The sample's CTest bound is30 seconds.

## Capability and version

`NEXORA_FEATURE_EDITOR_API_SAMPLES` defaults to the Editor capability and can be disabled explicitly.
The module declares its Editor dependency in `Config/Modules/modules.json`. Disabling the graphical
Shell does not disable this consumer. When Editor is unavailable, including Shipping and the global
dedicated/headless profile, the sample feature is forced OFF and no fake target/test is registered;
an explicitly retained ON value produces a configuration status message. The global headless profile
strips Editor entirely and differs from running this window-free sample in Development.

The schema uses version-two portable reflection semantics:16 fixed bytes, an unsigned little-endian
count at offset8 and signed64-bit little-endian tail elements starting at16, with maximum count4.
Reserved prefix bytes and another unavailable provider's complete payload stay exact. This is not
the layout of a native `std::vector`, and it neither loads nor restores a native component provider.
The [Editor contract](../../Engine/Editor/README.md#reflected-opaque-property-authoring) owns versions,
limits and incompatibility behavior. Build C++ consumers against the matching SDK; stable C/Gameplay
ABI and persisted scene formats are unchanged by this example. It introduces no restart requirement.

## Ownership and embedding

All SDK calls here run synchronously on one authoring thread. Workspace and document owners outlive
their borrowed SceneFileSession; owning reflection observations may be retained as data but never
reuse an old observation after a catalog, selection, generation or source-byte change. Every edit
prepares a fresh observation and records one complete transaction. Source values and unknown data
survive Move/Undo/Redo and fresh file reload.

`authorized` is an embedding-owner decision, not a permission grant from the Inspector. A real host
must recheck current writer/owned-document, stopped Play, modal/recovery and scope policy at the
transaction boundary. This sample owns its stopped Editor World and derives authority from its
actual temporary workspace; a real read-only workspace cannot clear the reopened data. Candidate,
schema, capacity, stale or authority failures must retain prior bytes/history. No native pointer,
service callback or plugin lease crosses this example's interface.
