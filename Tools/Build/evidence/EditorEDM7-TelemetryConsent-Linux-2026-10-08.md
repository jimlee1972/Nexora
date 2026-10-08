# ED telemetry consent and queue: Linux evidence

Date: 2026-10-08. Source parent: `35a870c2` (Profiler JSON import plus main integration).

TelemetryConsent previously kept its collected event strings when Set(false) revoked consent and
could append events indefinitely after opt-in. Revocation now clears the retained vector; a later
opt-in cannot restore previous events. Reaffirming consent preserves accepted records. Record
retains at most 1,024 events, with at most 1,024 UTF-8 bytes per event. Empty, embedded-NUL, invalid
UTF-8, oversized and capacity-overflow records return false without changing accepted events.
The queue stays inspectable through its existing borrowed Events() span; mutations invalidate that
borrow, and the owner serializes access. Existing signatures and class layouts are unchanged.

This is an in-memory consent/retention primitive. It neither persists nor transmits events, and it
provides no content redaction or secure memory wiping. Graphical privacy controls, transport,
persisted-queue deletion and release privacy acceptance remain separate open work.

The existing editor.preview_contract test now exercises disabled rejection, exact event-byte and
queue-count limits, malformed UTF-8, embedded NUL, multilingual/supplementary text, saturation
without eviction, unchanged accepted records after rejection, idempotent consent/revocation, full
queue release and a fresh re-enabled session. Profiler JSON and independent JSON consumers are also
rerun after integration with the updated native renderer/main.

Commands use `/workspace/.nexora/env.sh`, feature-on Development, lavapipe and Khronos
core/synchronization validation for the full gate:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development -R 'editor.preview_contract|editor.profiler_export|editor.profiler_json'
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Development configure/build passed. Focused tests **4/4 passed**, **0.63 s**. Full Linux
Development gate **152/152 passed**, none skipped, **340.89 s**. Minimal Monolithic Shipping
configure/build passed. Changed Markdown/bilingual pairing and `git diff --check` passed.
Generated tools, build output and logs are not committed. Both roadmap languages and the owning
Editor contract README are synchronized. Overall ED-M7 graphical acceptance remains open.
