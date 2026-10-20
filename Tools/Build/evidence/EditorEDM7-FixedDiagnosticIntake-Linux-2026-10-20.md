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
keyboard acceptance remains unchanged and must run in applicable hosted CI. Its supplemental
local software-display execution is recorded below.

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

## Supplemental native software-display workflow

After the full gate, isolated workspace tooling supplied Debian Xvfb 21.1.16, xdotool 3.20160805.1
and Mesa lavapipe 25.0.7. Downloaded packages matched the SHA-256 values in the Debian package
index; no system directories or repository dependencies were changed. The first native invocation
could not start Xvfb because the default command sandbox disallowed its local socket binding.
That environment-only attempt failed before the Editor launched and is excluded from acceptance.
With the required command network permission, the Xvfb startup probe succeeds and the unchanged
`LinuxDiagnosticPrivacy.py` passes against this exact product build, with actual native
keyboard/pointer consent/inspection/clear/opt-out/close/restart and read-only source conservation.

```bash
python3 Tests/EditorImGui/LinuxDiagnosticPrivacy.py \
  --editor build/linux-development/Apps/Editor/NexoraEditor \
  --xvfb /path/to/isolated/Xvfb --xdotool /path/to/isolated/xdotool
```

Runtime observations explicitly report `software=1`; the read-only restart presents all 80 frames
and retains no consent/events. Six implementation/test hashes still match the full Linux gate.
This is cloud native-window execution using software Vulkan, not physical display, real GPU,
DPI/IME or screen-reader acceptance. No explicit validation-layer enablement result is claimed
for this supplemental privacy invocation.
