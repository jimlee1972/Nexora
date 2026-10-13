# ED-M6 scoped graphical mesh memory file actions — 2026-10-13

Beads: `nexora-62u.2.11`. Base: merged PR586/main5f6cec3f. Supporting cloud graphical slice;
full ED-M6 and physical milestone acceptance remain open (0/8).

## Behavior and ownership

Explicit independent Export mesh memory, Import mesh memory and Clear mesh import controls bind
core `.nexora/mesh-memory.json` interchange. Requests own project UUID/root copies and are consumed
once. Application owner checks both before synchronous core IO and scoped owning static publication.
Widgets do no IO or native reads, and retain no workspace/history/native borrow. Shared validation
and captured/current scope checks reject bad/stale publication while retaining previous static data.
Open writable/nonempty/unblocked live history is required for export; read-only and empty-live import
are supported. Focus immediately revokes pending authority; Draw/Take revoke modal/recovery/close
requests, readonly changes revoke queued writes, and project UUID/root changes/closure/detachment
clear requests/static views. Focus/modals leave safe static data intact. Live Clear and static Clear
remain independent of live CPU/GPU/RSS/mesh and other imported files.

Live/static curves share the bounded byte/elapsed-time plot; static exact last values/unavailability,
plot scale, dropped samples and historical native-surface geometry/CPU-key scope are explicit. No
RSS/whole-device VRAM or project allocation ownership is inferred. Existing schemas/dependencies,
public host layout and stable C/Zig/scene formats are unchanged; additive C++ APIs require rebuild.

## Validation

- Final restored targeted actual1x/2x pointer widgets (new mesh controls, existing GPU/RSS/live-mesh)
  and native24-frame process no-implicit-capture acceptance:5/5 passed1.05s.
- New tests cover owning UUID/root one-shot requests, synchronous owner export/import, zero/null,
  source mutation after publication, stale roots/UUIDs, invalid samples/corrupt files preserving
  prior capture, live/static/other-import Clear, readonly queued-write revocation, focus loss,
  close/recovery gates, project replacement/closed/detached scope and unavailable-only static views.
- Preliminary compile failed because the shared interaction gate was referenced before declaration;
  gate placement was corrected. Preliminary layout pushed old GPU controls out of the original
  viewport; new controls were moved after the unchanged GPU controls, with bounded scrolling before
  new pointer tests. Original regression assertions remained intact; failed attempts excluded.
- A subsequent review added held-click project-scope cancellation and regression tests. The first
  full gate was deliberately interrupted for that fix and is not delivery evidence. Disabling only
  scope-change gesture cancellation caused the new test to fail with old-held-press/new-project
  activation; production cancellation was restored. The final full gate is required afterward.
- `cmake --preset linux-development` and `cmake --build --preset linux-development`: passed
  (311 actions after MSVC declaration correction; graphical Editor/SDK/Slang enabled, headless off).
- `ctest --preset linux-development --output-on-failure`:254/254, no failures/skips,825.62s; new
  mesh controls0.06s, oldGPU0.03s/RSS0.04s, native24-frame no implicit mesh file0.49s, native
  allocation/material pixel/drain0.68s.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed5 actions;
  Minimal Monolithic/native Vulkan, Editor disabled.
- Five changed C++ files format23 pass; DocumentationCI16 passed, synchronized EN-ZH/changed relative
  Markdown links, unchanged concise root README and diff checks passed. Final-head hosted gates
  are required before merge; exact hosted results will be recorded in the PR/Beads.

Widget tests use real Dear ImGui with synthetic native-style WindowEvents at1x/2x and synchronous
production core IO in the owner harness. The unchanged production measurement path/native Vulkan
allocation/pixel/drain tests cover actual diagnostics; these widget fixtures do not claim physical
GPU-memory calibration. Native24-frame Editor acceptance additionally rejects implicit mesh files.


Hosted first-head Windows compile rejected duplicate DLL export annotations on three new methods
inside the already exported test-access class (MSVC C2487). Member annotations were removed to match
the existing class contract; no assertions/API signatures or runtime implementation were weakened.
Fresh full local configure/build311/test254/254 passed825.62s after this correction. Fresh hosted
source checks remain required; failed original Windows runs are not acceptance proof.
