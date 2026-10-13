# Play acquisition release — Linux cloud, 2026-10-13

Beads: `nexora-rd7.2.4`. Base: `e94ef19e1dc2dd521cde530d04c4ca88239ba107`.

`PlayInputState` previously discarded the acquisition batch before processing native blur or a
pressed Escape. Later batches could therefore deliver held movement/buttons without another focus
acquisition. Release now revokes focus before acquisition suppression, regardless of event order.
Subsequent batches remain neutral until explicit owner reacquisition; its first batch is still
discarded. Sequence advancement, copied snapshot lifetime, bindings and the Gameplay C ABI remain
unchanged. A key-up Escape does not revoke focus. This does not implement new devices or pointer look.

The existing input-state fixture checks blur and Escape both before and after a key event, later
neutral batches, increasing sequence, immutable released copies and recovery acquisition suppression.
The actual Gameplay V3 host fixture verifies neutral selected/unselected lanes for user0 and user3.
Existing native Game input and project-profile checks remain in the required complete gate.

## Validation

The required Linux Development commands are:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

Configure and the 411-action build passed. The complete suite passed **244/244**, zero failures or
skips, in **830.94s**, including the two changed fixtures and native Game/input workflows.

An independent temporary harness compiled against the immutable base header reproduces four
acquisition-release failures (blur/Escape before/after a key). The same harness against the fixed
header reports zero failures. No production source was swapped during the gate. The three changed
C++ files passed clang-format dry-run, and documentation routing passed 16/16 tests; changed Markdown
links and bilingual pairs passed.

Windows/macOS and physical-device acceptance are separate host workflows; full graphical milestone
acceptance remains 0/8. Hosted PR results are recorded in the PR and Beads after publication.
The root README is unchanged; no module dependency or linkage boundary changes require Shipping.
