# Editor typed 2D project Content — Linux, 2026-10-16

Beads: `nexora-zih.3`, supporting `nexora-zih`. Base: `4743cc2bd6da24547a4b3ded0dabc3914917aa8f` (main after PR609).

Optional 2D source import now carries immutable decoded Atlas/Clip owners through the real
workspace index, project Content model and synchronous/background reimport transactions.
New validated source UUIDs become initial sidecar identities; existing metadata remains
authoritative across rename. No source bytes, metadata identities or C/Zig codecs are migrated.
Clip dependencies initialize the graph and update atomically with the admitted payload.
Publication retains existing access, project/source revision, cancellation and cycle checks.

`twod.content_import` exercises actual project files/sidecars and Core jobs. It covers uppercase
source discovery, duplicate and malformed new identities preserving the old index, existing
sidecar mismatch, old shared payload conservation, path Undo retaining the new payload,
synchronous/queued Atlas and Clip replacement, dependency-cycle rollback and stale source,
dependency and project generations. Read-only and cancelled requests cannot publish. Counts
4096/4097, retained Undo limits, four maximum-sized Atlases exceeding 64 MiB capacity accounting,
and sparse source size above 64 MiB all exercise admission without partial artifact publication.

Focused acceptance passed in 1.02 seconds. The first focused run failed a test assertion that
expected the specific importer error to be the final diagnostic; the application appends a
generic worker failure afterward. The assertion now inspects the complete diagnostic sequence.
No importer/production behavior was changed to hide that test error.

The first 2D-OFF test build exposed a missing standalone forward declaration in the private
source reader (its 2D-ON include had supplied the name). The OFF branch now declares the opaque
type explicitly. The concurrently running initial Development CTest was deliberately interrupted
and retained; configure/full build/CTest were restarted against the final source. That interrupted
run is not reported as a completed gate. The corrected OFF Editor Core and existing Workspace
budget/Material import tests pass 2/2 in 3.96 seconds; new typed source/test targets are absent and
the built Editor library has no TwoD dynamic dependency. A separately compiled temporary probe
also confirms arbitrary `.nx2atlas` bytes retain generic indexing with 2D disabled. The probe uses a task-local loader path; no repository runtime loader setting was changed.

Final validation:

- `cmake --preset linux-development` and `cmake --build --preset linux-development`: passed.
- `ctest --preset linux-development`: initially 325/326 passed, zero skips, 1051.43 seconds.
  `build.v2_production_toolchain` failed because this reset executor lacked `clang++`.
  After restoring real Clang 19.1.7, the same-source targeted rerun passed 1/1 in 0.37 seconds.
  All 326 distinct cases have passed; this is not a clean-first-full-run claim. No source or
  test settings changed for that rerun. New `twod.content_import` passed in the full run (1.04 s).
- `cmake --preset linux-shipping` / `cmake --build --preset linux-shipping`: passed,
  Minimal Monolithic scope only (Editor/testing excluded).
- 2D-OFF graphical configure and actual Editor Core/test build passed; Workspace budget and
  Material import tests passed 2/2 in 3.96 seconds. New source/test targets are absent; readelf
  confirms no TwoD native dependency. An isolated linked probe passes generic nx2atlas indexing.
- Documentation tests 16/16 and seven changed Markdown files pass. Ten C++ files are formatted;
  both new translation units pass Clang syntax with `-Werror=shorten-64-to-32`. Root README unchanged.
- Twelve source/build SHA-256 hashes are unchanged after the final gate. They are retained in
  the paired JSON. Exact-head hosted results and merge/branch cleanup are recorded in the PR.

The existing native Play soak passed 32 cycles/128 lifecycle records, source bytes/pixels unchanged,
with zero validation errors. Warmed RSS peak growth was 8876032 bytes under its 33554432-byte
limit. These existing fixture measurements do not establish all-process leak freedom.

Capacity accounting is per indexed typed set and per live/Undo snapshot. It is neither allocator
overhead nor an all-external-snapshot RSS bound. Clip import records references without loading
dependent Atlas files; cooked Play closure validation remains separate. New identity failures
preserve the prior index but valid earlier sequential sidecars may already exist. No multi-file
filesystem rollback or automatic old-sidecar migration is claimed.

Main Editor Play/Game routing, project-scoped cook/freeze admission and integrated preview/reload
remain open. This does not accept any enclosing ED milestone or change 2D 9/11 (P0 6/8).
Local Windows screening and partial Windows VM evidence on main belong to other runs; no physical
Windows acceptance was executed in this Linux cloud task. Shipping Minimal Monolithic excludes
Editor/testing and does not exercise the new Editor payload class.
