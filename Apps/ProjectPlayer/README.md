# Nexora Project Player verification

Enable `NEXORA_BUILD_PROJECT_PLAYER=ON` together with the asset pipeline to build the Runtime-only
`NexoraProjectPlayer`. The current executable verifies a **StaticView cooked package**:

```sh
NexoraProjectPlayer --verify-package '/path/to/project package.nxproject'
```

It loads the actual World snapshot, resolves all cooked mesh/scalar PBR bindings and validates
bounded dependency/integrity/identity contracts. Success prints JSON with
`status: VERIFIED_STATIC_VIEW`, project/scene UUIDs, entity/asset/resolved-renderer/inactive-component
counts and at most 64 render item summaries. IDs remain decimal strings to preserve every uint64
bit. Up to 64 inactive-component summaries identify entity/type/name and payload size; names are
lossless hexadecimal so preserved non-UTF8 names cannot invalidate JSON. Unreported counts are
explicit. These are diagnostic caps, not loading/validation limits: all bounded scene entities,
bindings and opaque records are validated. Invalid arguments exit 2; invalid data/input exits 1.
Failures do not print a success report.
The Windows console entry uses CRT `wmain` and native UTF-16 filesystem paths; POSIX `main`
interprets path arguments as UTF-8. The test fixture writer uses the same platform path rules.
MSVC/clang-cl support the wide console entry directly; MinGW uses its `-municode` link option,
without adding Shell32 or a command-line reparser. Linux path/CLI tests have run; Windows and
macOS execution have not been performed in this cloud environment.

Unknown opaque records remain present but inactive. Scalar material bindings override StaticView
appearance explicitly while preserving full legacy shader IDs; an unimplemented nonzero legacy
shader without such a binding rejects. No project source directory is required to verify an
already-produced package. The player does not compile/export an Editor project, load gameplay,
render a native window or deploy an application. Those dependent workflows remain open.

See [the Runtime package contract](../../Engine/Runtime/ProjectPackage.md) for the wire, limits,
resource collision policy and ownership. Linux cloud CLI acceptance covers real cooked artifacts,
corruption/truncation/missing-file and alias failures; physical display and other platform runtime
acceptance are separate.
