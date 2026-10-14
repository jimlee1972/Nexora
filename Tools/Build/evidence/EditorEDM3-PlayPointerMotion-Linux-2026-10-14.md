# ED-M3: Optional copied Play pointer motion and wheel

Date: 2026-10-14. Beads: `nexora-rd7.2.10`. Base: `7f49087b23fb6fcbc1c4ebc49aaf25eaf3f68dc8`.
Machine-readable scope, source hashes and measurements: [JSON](EditorEDM3-PlayPointerMotion-Linux-2026-10-14.json).

The graphical Editor now serves a negotiated optional V3 pointer callback after the existing 2D
extension. Original V3 prefix, legacy input wire and ABI major remain compatible. The 56-byte
caller-sized V1 snapshot owns bounded native client-pixel deltas, horizontal/vertical wheel units,
five raw held bits and the legacy batch sequence. Unknown output suffixes and all error outputs
are preserved. C/Zig helpers check each optional callback's field end; the older 2D host remains valid.
See [ADR-0010](../../../Roadmap/en/ADR-0010-Play-Pointer-Input.md) and the
[owning contract](../../../Apps/Editor/README.md#optional-copied-play-pointer-input).

Real Zig0.14.0 and C++ compare every new wire offset, alignment, field-end and host size. Exact-size
original/2D host prefixes reject absent pointer services without invoking the callback. A positive
old 2D host probe failed with the old total-host-size guard, then passed with the extension-end guard.
C11 static assertions and the additive manifest/baseline validator preserve the original baseline.

Thirty-two state/provider lifecycles cover batch sums, native int32 extremes, saturation, owning
non-consuming reads, raw versus legacy button bits, empty batches, acquisition, blur/Escape in both
orders, coordinate-domain changes in both orders, profile/user resets, four logical users, output
preservation, future suffixes, Fixed/Update, Pause/Step, Stop/Destroy and unbound-owner rejection.
Five focused state/layout/API checks passed in0.04s.

The actual dynamically loaded C-ABI module drives native X11/lavapipe Game mesh pixels from
`(40,-20)` motion and both signs of horizontal/vertical 120-unit wheel input. Uncaptured input,
empty batches and Escape remain stable; Pause/manual Step is neutral and source scene bytes remain
unchanged. Fixed and Update deduplicate displacement by sequence without a seconds multiplier.
The strengthened native test passed in6.18s, after three consecutive earlier passes in5.95/5.82/5.98s.

A Fixed-only fixture initially missed a batch without a fixed tick (restored-run failure1.59s).
Reading in Update too, with sequence deduplication, resolves this consumer error. Withholding the
host capability deliberately prevented module start/native Game viewport (expected failure12.53s);
provider bytes were restored exactly. These faults do not change production acceptance assertions.

The first full run passed279/280 in859.65s. The scene-services fixture expected exactly the old
allocator/scene capability mask and rejected the newly implemented pointer bit. Its expectation now
includes that bit and requires the corresponding callback; other service assertions remain intact.
The final complete gate passed after this fixture contract update. A strict-warning audit also
added an explicit null pointer suffix to the non-provider Showcase host aggregate. The final gate
enables Showcase as well as real Zig (304 tests); an early rerun was stopped before this C++ edit.

The initial Shipping configure/build passed (12 actions, Monolithic; Zig OFF). Final Shipping
with real Zig and Showcase also passed (10 actions). Final Linux Development configure/build and **304/304 tests passed in980.97s**, with zero skips.
The native pointer case passed in6.14s; scene-services in0.03s and Prefab isolation in89.20s. Local Development enables Showcase and the pinned real Zig compiler. Development commands:
`cmake --preset linux-development -DNEXORA_BUILD_SHOWCASE=ON`,
`cmake --build --preset linux-development` and `ctest --preset linux-development --output-on-failure`.
Final `cmake --preset linux-shipping` / `cmake --build --preset linux-shipping` use
`NEXORA_ENABLE_ZIG_GAMEPLAY=ON`, `NEXORA_BUILD_SHOWCASE=ON` and the same Zig0.14.0 executable. Hosted validation must
pass on the final PR head before merge. Physical platforms are not claimed by the local Linux run.

Cursor lock, raw/unbounded relative motion, application sensitivity/inversion, gamepad, simultaneous
physical devices, physical calibration and full pointer-look/ED-M3/Editor acceptance remain open.
