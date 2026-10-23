# SP-06 lifecycle cloud evidence — 2026-10-23 UTC

This is a manually maintained evidence summary. Generated logs/JUnit/build output remain outside
Git or in CI artifacts. Base: Nexora main `796592c527528c4d348a5cbc101180ede466e48b`.
The implementing PR identifies the tested head and hosted CI; this local report does not preclaim
hosted success or physical Windows acceptance. Beads `nexora-4j7` tracks the delivery and
`nexora-4j7.1` retains the physical/product acceptance gate.

## Delivered scope

- Optional owner-thread Lifecycle V1 with copied, nonconsuming focus/minimize/power/disconnect
  sequences and connected/rumble masks. Services V1 grows from 88 to 96 bytes on 64-bit hosts,
  preserving all old prefixes. No original Input/Audio ABI or coordinate contract changes.
- Explicit close opt-in and exact pending-token cancellation/acceptance. Legacy immediate close
  remains default. Native external destruction cannot be cancelled. Acceptance never saves for Zig.
- Win32 power messages and dedicated minimize/restore events, surviving resize coalescing.
- Dynamically loaded XInput motor/capability calls; bounded finite gains/duration, explicit unsupported
  devices, expiry and zero on blur/minimize/suspend/close/Stop. Failed zeros retry on owner ticks;
  disconnected commands never replay. Native adapter teardown attempts zero before DLL release.
- Player updates gameplay even while it cannot present, exposes events before fixed ticks, clears
  unsafe controls at loss and gates built-in UI until a focused neutral frame, preserving fresh
  post-focus movement.
- Zig `PauseGate` and an actual V3 diagnostic own pause, consistent snapshot serialization/error,
  pending marker retention, explicit three-second preparation and close/save UI. Supplied diagnostic
  font is explicitly registered/removed; no synthetic missing-glyph fallback is claimed.

## Review regressions

The final patch also covers three observed integration hazards. Native continuous read failures
now create one disconnect edge per outage rather than one per poll, preventing repeated snapshots.
The Player restores input focus **before** taking a complete gamepad sample; cleared/ignored actions
cannot masquerade as physical release. The actual GameplayInput plus the shared Player helper test
holds confirmation across the interruption and several recovery frames, then releases and presses.
Reconnects rearm through the same neutral gate. Finally, actual SP-01 and SP-04 modules tolerate
zero dimensions, and a dynamic-module regression repeats minimize/restore and checks restored UI.

The Windows MSVC `/W4 /WX` build additionally caught two test-only size_t-to-uint32_t
arguments. Explicit casts now match the fixed 16-byte payload API; the corrected fixture also
passes Windows GNU cross-compilation with `-Wshorten-64-to-32 -Werror`. No warning suppression
or production behavior change was introduced. The full local Development gate was rerun after the correction; Shipping production sources
were unchanged from their preceding successful gate.

The Windows resize gate now checks the complete resized/minimized/zero-extent/restored/final-extent
sequence instead of its pre-lifecycle assumption of a single coalesced event. A separate ordinary
resize-only batch must still coalesce to exactly one final extent; no native assertion is disabled.

The existing real Xvfb/Player movement gate exposed an overbroad end-of-batch raw-input clear.
The final Player separates UI rearming from gameplay actions: actual losses clear controls at their
event boundary, restored foreground routing accepts fresh keys, and UI-only suppression preserves
new D movement while still requiring a physically released/new gamepad confirmation. The shared
Player helper regression checks both outcomes together. Native keyboard acceptance remains strict.

## Local validation

Full `linux-development` configure/build/CTest passed: **292 registered, 276 passed,
16 native-prerequisite skips, 0 failures; 151.56 seconds**. The skips are the existing Vulkan/
Slang/XTEST native prerequisites; none of the new SP-06 tests skipped.
Full Monolithic Shipping build and independent baseline-CPU ReleaseSafe Zig/Player export passed.
The independent Player observed 240 completed UI frames with balanced module allocations and
zero project assets, using the optional lifecycle table and explicit close guard.

`runtime.gameplay_lifecycle` covers all previous services prefixes plus intermediate-size canaries,
wrong-thread rejection, failed-query preservation/future suffixes, unsupported/invalid rumble,
steady-clock expiry, failed native zero retry, Stop/destruction zero, retained same-batch
interruptions, idempotent pending close, cancellation/stale-token rejection and explicit acceptance.

`gameplay.zig_lifecycle_slice` links the actual Zig module object with real Runtime Storage.
It tests a loss/recovery before one fixed tick, a stopped combat counter, retained pending marker,
no automatic resume, three-second confirmation, minimize, controlled-slot disconnect/reconnect,
suspend with huge wall-clock deltas, countdown cancellation, close cancellation and acceptance.
A nonempty `sp06.tmp` directory forces atomic-save rejection even for a privileged test process:
the previous payload remains byte-identical, failure blocks resume/exit, explicit retry succeeds,
and a fresh module reads the saved counter/marker and starts paused. It also exercises the exact
Player UI gate with a restored held button, a neutral release frame and a fresh confirmation.

`api.gameplay_lifecycle_zig_abi` and `gameplay.zig_assets_wire_layout` compare C/Zig sizes, alignment
and all offsets, preserve missing-service outputs, and probe the SP-05-sized old host.
`api.lifecycle_baseline_guard` mutates C/Zig order, the independent baseline, services suffix and
every callback's required metadata. API-M6 reports ABI 3 / 123 documented exports.

Targeted GCC 14 ASan/UBSan/LeakSanitizer runs passed for the lifecycle provider and actual Zig/Storage
regression, with default leak detection and no quarantine override. These run outside the ptrace
sandbox so LeakSanitizer can inspect threads; they are not a full sanitized engine build, and the
Zig object uses the repository's normal Zig runtime safety rather than C++ sanitizer instrumentation.

Zig 0.14 Windows GNU cross-compilation of the current Win32 window and native gamepad translation
units passed. This proves compilation only. The initial `-fsyntax-only` driver invocation reported
FileNotFound; normal object compilation supplied the usable result. A full MSVC link/runtime gate
belongs to hosted Windows CI. Win32 message injection coverage exists in the native window test,
but was not executed on this Linux host. macOS native power and non-XInput haptics are not delivered.

The first independent Player check correctly rejected an empty diagnostic UI. Registering the
supplied real font and restricting labels to its supported glyphs repaired this; UI callback errors
now fail rather than silently claim output. Final acceptance uses the repaired module.

## Reproduction and remaining acceptance

[Commands](SP06-Lifecycle-Cloud-2026-10-23/commands.txt) record the executed gates/toolchain setup.
No generated logs are committed. Paired API/2D/Zig/Window roadmaps and the complete-plan haptics
scope reference are synchronized. The repository-root README is unchanged.

Physical Windows Alt-Tab/minimize, actual controller removal/reconnect and motor 0/50/100%, true
sleep/resume, repeated close/retry/cancel, disk failures and final game draft/transaction behavior
remain **NOT VERIFIED**. Message injection does not suspend Windows; a backend callback does not
prove audible/physical motor output. The diagnostic pending marker is not the game's real card,
transaction envelope, checkpoint migration, resource recovery or eighteen-minute run integration.

OS notifications cannot guarantee a last-moment save or motor zero before sleep. Forced termination,
crash, frozen owner loops and lost hardware connections can bypass ordinary cleanup. Retain normal
checkpoints and the last reliable file. Neither these cloud tests nor hosted CI complete SP-06
product acceptance, M6/M7 or unrelated SP gaps.
