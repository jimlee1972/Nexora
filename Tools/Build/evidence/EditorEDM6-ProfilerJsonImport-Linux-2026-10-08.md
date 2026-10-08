# ED Profiler JSON import: Linux evidence

Date: 2026-10-08. Source parent: `a66a772c`.

The Profiler reads its schema-1 `.nexora/frame-processing.json` into an owning static wall-time
capture through ProjectWorkspace. It validates current project UUID, source, metric, scope, unit,
sample count, ordered nonzero lossless uint64 frame IDs, lossless dropped count, finite nonnegative
wall times, false GPU/memory availability and null unmeasured samples. The nonrecursive reader has
no external dependency, consumes at most 128 KiB plus one over-limit probe, accepts 1-600 samples,
field reordering, JSON whitespace and equivalent ASCII escapes, and rejects duplicate/missing/
unknown fields, invalid numbers, trailing data and unsupported schemas. Closed or recovery-pending
projects and unsafe metadata/leaf paths reject without file or workspace mutation. Read-only import
is permitted. Filesystem checks follow the serialized authoring-thread contract.

UI emits an independent one-shot Import JSON request. The application owns synchronous IO and
publishes a validated owning snapshot to the shared static imported plot. Failed load/publication
preserves the previous import; live capture is unchanged. Project root/UUID change or detachment
clears imported data, status and pending requests. Modal/recovery/close/no-project states block
requests. CSV/JSON export formats, existing signatures, public class layouts, C SDK and module
boundaries are unchanged; two additive C++ methods expose import/request consumption.

Tests in `editor.profiler_export` cover round trip, field order/ASCII escapes/no final LF, every
truncated prefix, unsupported/duplicate/unknown/missing fields, mismatched project/count, malformed/
nonfinite/overflowing numeric data, 600/601 samples, exact 128-KiB and overflow, uint64 IDs beyond
2^53 and UINT64_MAX, full/subnormal/maximum double precision under comma locale, unavailable
measurement preservation, read-only/closed/recovery access, missing/directory/valid/dangling aliases,
metadata-parent aliases and failure/retry. Actual Window pointer/button events at 1x/2x DPI cover
independent one-shot JSON requests, snapshot publication, failed-load preservation, read-only access,
modal/recovery/no-project gates and detach cleanup. Workspace-budget tests also reject JSON import
for occupied/uninspectable recovery entries. Existing independent normal/optimized Python consumers
still verify actual C++ JSON exports. This is schema-1 import, not arbitrary capture ingestion.

Validation commands use `/workspace/.nexora/env.sh`, with graphical shell enabled and lavapipe;
Khronos core/synchronization validation is enabled for the full gate:

```bash
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development -R 'editor.profiler_export|editor.profiler_json|editor.workspace_budget'
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
python3 Tools/Build/TestDocumentationCI.py
```

Development configure/build passed; focused tests **4/4 passed**, **1.77 s**. Minimal Monolithic
Shipping configure/build passed. Documentation routing **16/16 passed**. Full Linux Development gate **152/152 passed**, none skipped, **341.06 s**.
Changed Markdown/bilingual pairing and `git diff --check` passed. Generated tools, build trees and logs are uncommitted.

Physical-display Linux and Windows DPI/IME, complete graphical milestone acceptance, arbitrary
capture import, GPU/memory instrumentation and remaining ED work stay open.
