# External Chrome trace inspection — Linux, 2026-10-10

Beads `nexora-62u.2.5`. Source foundation `3c1ccab5` is accepted main `6dbd45a3`
plus the pending separately reviewed native reflected-pointer fixture fix #493.
The foundation itself must never be merged. Rebase only this feature onto current
main after the fixture prerequisite is accepted, and require fresh exact-head CI.

Managed four-CPU Linux: graphical shell, OpenSSL cryptography, Slang, Zig gameplay,
showcase and native Project Player enabled. Pinned ImGui/Vulkan overrides preserve versions.

- Full Linux Development: **241/241 passed, zero skipped, 599.07s**.
- Linux Shipping configured and built successfully, five steps.
- Focused importer, actual1×/2× controls and existing profiler/GPU capture: **4/4 passed,0.24s**.
- Initial importer compile/fixture run:1/1 passed,0.11s.
- Work-area logs: `reflected-inspector/chrome-{configure,first-build,first-tests,ui-build,ui-tests}.log`,
  `chrome-full-{build,tests}.log`, `chrome-shipping-{configure,build}.log`.

Importer fixtures cover wrapped/bare real complete-event traces, explicit scope,
Chrome microsecond timestamps despite displayTimeUnit, duration conversion, ignored
nonmatching/metadata events, raw UTF-8 and equivalent Unicode/surrogate pairs,
unknown nested JSON, numeric/string process/thread IDs, exact byte budget, latest600
retention/sequence/drop counts and simultaneous ordered timestamps. Every truncation,
duplicate/equivalent escaped keys, malformed numbers/Unicode, missing fields, time
regression, oversized event/string/object/depth/input and invalid selectors reject.
Prior owning captures and input bytes remain unchanged. Shared capture validation
protects public static-view publication.

Project fixtures read the explicit project-owned trace without writes. Writer and
read-only observers import; missing/corrupt/oversized files, recovery, external
workspace changes, nonregular paths and supported symlink/hard-link fixtures reject.
Closed project scopes reject. Linux alias creation is exercised; hosts unable to
create an alias omit that fixture without claiming physical-host acceptance.

Actual1×/2× pointer controls emit one owning UUID/root/event/process/thread request,
copy static capture contents, reject invalid publication without erasing last-good
capture, keep live and independent wall/RSS imports separate, allow read-only requests,
block close-modal requests and revoke window/request/capture on project change/detach.
Existing profiler export and GPU static publication tests remain green. Full existing
native Xvfb/Vulkan acceptance passes; a dedicated native Chrome interaction tour is
not claimed by the synthetic control tests.

Complete-event intervals retain their external clock; ph=X alone never proves CPU
or GPU timing. CPU/GPU classification, memory and host/project clock calibration remain
unavailable. This parser supports a documented bounded Chrome subset, not arbitrary
binary capture formats. No IO in widgets, no native loads, no implicit telemetry or
transmission. Bounds are logical, not a total allocator-memory quota. Public C++
consumers rebuild; stable C/Gameplay ABI unchanged. Full milestones remain **0/8**.

## Latest accepted Main integration

Only the owned Chrome trace feature was replayed onto accepted Main
`4e4c2984d1339cb80af83ff40a6735b6497cf6dd`; no parent/foundation branch was merged.
Graphical Development rebuilt **316 steps**, and full CTest passed **242/242 in
594.19 s**, zero skips. The Chrome CPU trace fixture repeated in **0.10 s**.
Minimal Monolithic Shipping passed **5 steps**, with Editor excluded. This remains
bounded external CPU trace import/inspection; native GPU clock calibration, memory
provider acceptance and physical platform checks remain separate. Contracts and
bilingual supporting roadmap scope remain unchanged, with complete milestones **0/8**.
Fresh current-head hosted checks and clean current Main remain mandatory.
