# Editor ED-M3 cooked 2D Play asset ownership — Linux, 2026-10-15

Beads: `nexora-zih.2`, supporting `nexora-zih`. Base: `3cb44bcd28a34a620ce67daaa54c5e7fd908b9ab`.

`TwoDPlayAssets` freezes one explicitly selected cooked generation's Atlas/Clip dependency closure.
The immutable snapshot owns decoded payloads, admits at most 4096 unique assets and 64 MiB of
combined cooked payload, validates envelope type/schema, payload identity, exact Atlas dependencies
and every Clip frame region. The existing Runtime store verifies cooked hashes during admission.
No Runtime, C/Zig, source or scene schema changes; the optional Editor/TwoD dependency is unchanged.

The test uses actual registered 2D importers, AssetCooker, BundleBuilder and AssetGenerationStore.
It verifies explicit old/new generations under the same UUID, collection, rollback, store destruction,
shared Atlas retention and canonical GameplayBridge animation lookup after store destruction.
Exact and exceeded payload limits, the 4096/4097-asset dependency boundary, duplicate/zero roots,
missing assets, unsupported types, malformed codecs, mismatched UUID/dependency lists and missing
frame regions reject without stealing external pins/resident references. Successful and failed
captures balance their own temporary store references. No native rendering change is introduced.

Validation:

- Development configure and full build passed (573 actions).
- Full `ctest --preset linux-development`: initially 310/311 passed, one failure, zero skips,
  1104.28 seconds. New `twod.play_assets` passed in 0.15 seconds.
- Same-source targeted rerun of `editor.linux_build_process_console`: 1/1 passed, 7.41 seconds.
  The first run observed operation-1 cancellation but no expected operation-2 failure diagnostic.
  No source or settings changed. All 311 distinct cases have passed; this does not claim a clean
  first full-suite run or a fix for the intermittent native UI failure.
- Earlier focused `twod.play_assets`: 1/1 passed, 0.18 seconds.
- Shipping configure/build passed (18 incremental actions), Minimal Monolithic scope only.
- 2D-OFF graphical configure passed: new source/test absent and Editor retained. Configure only.
- Documentation tests 16/16; seven Markdown files validated; three touched C++ files formatted.
  Root README unchanged; five source/build-file hashes unchanged through validation.

Existing native Play soak passed 32 cycles/128 lifecycle records in 82.44 seconds, preserving source
bytes/pixels with zero validation errors. Warmed RSS peak growth was 4,927,488 bytes under the
33,554,432-byte guard. This does not establish all-process leak freedom. Exact-head hosted results,
merge and branch cleanup are recorded in the associated PR.

Initial focused compilation found an incorrect test callback name and inconsistent multi-declaration
array deduction. Both were corrected before the successful focused test and final gate. The final
test and implementation also pass Clang syntax checking with `-Werror=shorten-64-to-32`.

This does not authenticate package/project/source provenance, check project writer leases, attach
the main Editor Play World/Game UI, or implement integrated hot reload. `TwoDPlayContext` still
captures the isolated document independently. Production ED milestones remain 0/8; 2D milestones
remain 9/11, with physical Windows M6/M7 acceptance not run here. Shipping Minimal Monolithic
excludes Editor/testing; a successful Shipping build does not exercise this new Editor class.
