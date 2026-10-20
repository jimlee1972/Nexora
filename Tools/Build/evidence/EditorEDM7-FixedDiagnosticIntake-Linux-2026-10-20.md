# ED-M7 fixed local diagnostic intake: Linux evidence

Date: 2026-10-20. Base: `4744c746ef588a7c764e85803d0867a37d2ece58`.
Beads: `nexora-gu7.41.1`, under the open M7-06 privacy/trust goal.

Previously, the privacy panel displayed only known labels but its backing `TelemetryConsent`
queue could retain arbitrary caller text. Admission now retains only the engine-owned
`frame.presented` label. The native producer uses `LocalDiagnosticEvent::FramePresented`; invalid
enum values reject. The existing string API accepts that exact label only, rejecting private
paths, commands, source snippets, Unicode text and near-matching labels before retention. This
intentionally restricts legacy free-form callers. Typed intake carries no caller text; constructing
the legacy by-value argument is still the caller's responsibility.

Consent remains default off, the queue admits at most 1,024 records without eviction, and opt-out
clears logical records. Reaffirming consent preserves accepted data; clear/re-enable/project/close/
restart retain their established lifecycle. The owner serializes all calls and `Events()` borrows
until mutation/destruction. Typed admission catches allocation failure and relies on vector's
strong exception guarantee to retain previous records; no allocation-fault injection result is
claimed. No persistence, network adapter or plugin callback is added. This restricts diagnostic
data collection; it does not claim generic secret detection, secure memory erasure, complete
extension trust, or redaction of arbitrary Runtime/Console producers.

The Core contract test checks disabled typed/text rejection, explicit opt-in, exact compatibility
labels, invalid enum values, private POSIX/Windows paths, a fictitious credential-bearing command,
source text, malformed bytes/NUL, case/whitespace variants, capacity without eviction and consent
transitions. Actual ImGui controls at 1x/2x verify rejected text is absent from the retained queue,
complete safe row counts, clear, opt-out, project/nil scope and fresh-model restart. Existing native
keyboard acceptance remains unchanged and must run in applicable hosted CI; it has not run locally.

Validation:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development -R 'editor.(preview_contract|diagnostic_privacy)$'
CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
ctest --preset linux-shipping
```

Linux configure and all 493 product build steps pass. Focused tests pass 2/2 in 0.10 seconds.
Full Linux gate: 298 registered, 286 passed, 12 existing environment skips, zero failures in
560.48 seconds. Deep Hierarchy passes in 67.40 seconds and original 100k lifecycle in 193.30
seconds, retaining scale 1 and the original 180/300-second deadlines.
Minimal Monolithic Shipping configure/build passes its nine integration steps; Editor is stripped
and CTest registers no tests, so this is build evidence only. Slang/Zig are disabled locally; the
pinned Vulkan header source is reused for Shipping. Documentation tests pass 16/16; changed
Markdown links/bilingual pairing, changed-line formatting and diff checks pass. Six implementation/
test hashes were frozen before configure and remain unchanged through completion. Existing C++
symbols/class layout remain; the new overload adds an export and matching consumers rebuild.
Stable C/Zig ABI, data formats and dependencies are unchanged. Applicable exact-head hosted CI
remains required before squash merge; no local Windows/macOS/native-display pass is claimed.

The overall Editor roadmap remains 0/8 accepted milestones. Physical display/input/accessibility,
complete privacy/trust and every unrelated open goal remain open.
