# ED-WF-06 Sprite catalog admission — Linux, 2026-11-12

Scope: `nexora-whr.6.3`, based on main `ee1cc3f1e08560118438a6fcc262dbebb3f9bb79`
(merged PR #768). Registry foundation PR #767 is included. Canonical source descriptor/Host
negotiation is independently owned by `nexora-whr.6.2` and is not implemented by this slice.

## Delivered contract

- Add catalog kind 4 for cooked `sprite_source` envelopes. Existing catalog kinds 1/2/3 and
  all C/Zig table/descriptor layouts remain unchanged; content registry kind 3 is a separate
  namespace. Opaque kind 3 still copies arbitrary bytes, including a cooked source envelope.
- Owner-thread publication verifies exact file length, cooked hash/type, container/payload UUID
  and exactly one Atlas dependency. Registry admission validates live Atlas/region/trim closure.
  Missing dependencies fail without implicit reads or partial publication. Active UUIDs cannot
  be replaced by a catalog request. Explicit source-before-Atlas removal is required.
- Player accepts `sprite` and `deferred_sprite`. Explicit preloaded Atlases are admitted before
  Clips/sources, independent of manifest order. Deferred entries preserve descriptor order;
  callers load the Atlas first. Canonical export cooks `.nx2sprite`, or copies `.nxcook`, with
  the existing contained-path, per-file/startup budgets and atomic staging rules. Copied cooked
  envelopes are validated at runtime rather than being trusted by the exporter.

## Executed validation

Cloud Linux uses GNU C++ 14.2, CMake 3.31.6, Ninja 1.13.2 and Zig 0.14.0. Development includes
the graphical Editor, native Vulkan, Slang, 2D and Zig gameplay. Shipping is Monolithic with
testing disabled and 2D enabled. Dependencies/cache/state are external to the repository.

Focused commands (after sourcing the external build environment):

```sh
cmake --preset linux-development
cmake --build --preset linux-development --target NexoraGameplayAssetsTests NexoraTwoDProjectManifestTests NexoraTwoDPlayer NexoraSpriteCatalogFixture NexoraTwoDAssetCompiler
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development -R '^(twod\.(gameplay_assets|project_manifest|sprite_catalog_export|project_export)|gameplay.zig_assets_wire_layout)$' --output-on-failure
```

Initial focused run: 4/5 passed; the new independent module omitted `abi_version = 1` on its
content/assets getter inputs and failed at Start. Correcting the fixture retained production
behavior and all assertions. The new package test then passed (0.99 seconds); its strengthened
diagnostic/copy assertions passed again (1.01 seconds). The original source-free project export,
asset IO, manifest and C/Zig wire checks passed in the initial run.

`twod.gameplay_assets` exercises actual background reads and copied result states, metadata/hash
tampering, wrong envelope type, missing/duplicate/foreign dependency lists, payload UUID mismatch,
missing Atlas, invalid region/trim, active UUID protection, kind-3 copy compatibility and retained
source/Atlas snapshots. Rejection preserves the last-good registry and releases staging bytes.

`twod.sprite_catalog_export` builds an independent module using only public C headers (no engine
DSO dependency). It exports canonical eager/deferred sources, verifies exact pre-cooked copies
and inventory hashes, deletes authored inputs, relocates to a Unicode path and recreates the
Player process. The module observes preloaded source metadata through the original content table,
requests a missing-dependency source, loads its Atlas, retries successfully, rejects a duplicate,
copies opaque kind 3 and removes dependencies in order. Corrupt hash/type/identity/closure and
bad region/trim reject with expected diagnostics. Failed cooking publishes no output and leaves
an existing package unchanged. Restored package bytes run successfully and remain immutable.

Final gates (all exit 0):

```sh
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development --output-junit /workspace/work/editor-completion/sprite-full-junit.xml
cmake --preset linux-shipping
cmake --build --preset linux-shipping
ctest --test-dir build/linux-shipping -N
```

The complete unfiltered Development traversal passed **477/477, zero failures/skips**, in
**1296.89 seconds**. No failed-case retry was needed. This includes the new package test (1.01 s),
source-free project export, old C consumers, C/Zig layout/baseline guards, 100k hierarchy lifecycle
(158.09 s), native Play soak (77.48 s), native 2D Game (62.62 s), packaged Play (46.46 s), both
1,000-cycle authoring replays and layout restart. The Xvfb lifetime case passed (0.48 s);
this single pass does not close its independent intermittent-readiness investigation.

Shipping configure/build passed with 2D enabled; Minimal Shipping registers zero tests by
contract. An additional Python driver imported `SpriteCatalogAcceptance.py`, selected
`build/linux-shipping/Apps/TwoDPlayer/NexoraTwoDPlayer`, the Development-built independent C ABI
fixture/compiler, and forced every exporter call to `allow_development=False`. It asserted
`inventory.json` reports `development_test_copy=False` for every published package, then ran all
the same relocation, process-recreation, deferred IO and corruption assertions successfully.
The normal Shipping dependency policy rejects engine DSOs. This is CPU file/registry admission,
not a clean-host/native game-delivery verdict. The driver log is `sprite-shipping-package.log`.

Documentation routing unit tests passed **16/16**; changed Markdown/bilingual roadmap validation,
touched C++ `clang-format --dry-run --Werror`, Python byte compilation and `git diff --check`
passed. Root `README.md` remains unchanged. [The machine receipt](ED-WF-06-SpriteCatalog-Cloud.json)
records exact implementation/binary hashes and selected test results. All log/JUnit files use
the external `/workspace/work/editor-completion/` task directory, including the initial focused
failure and subsequent package rechecks. The CMake diagnostic revision names the parent commit
because the candidate was validated before committing; implementation hashes identify the exact
candidate, and the delivery PR records its immutable head and hosted CI separately.

## Windows catalog initializer repair

The first `d960b133` hosted runs are failed evidence, not accepted CI: PR Build
`43189516120`, push Build `43189431588` and SDK producer `43189516100` report MSVC C2397 at
`TwoDPlayer.cpp`'s catalog initializer. The runtime conditional expression has C enum type;
MSVC's signed underlying enum cannot implicitly list-initialize the ABI's `uint32_t` field.
The matching test catalog had the same pattern. Both now explicitly cast the selected enum to
the field's fixed width; kind values, C/Zig layouts, loader semantics and assertions are unchanged.

A Clang 19.1.7 freestanding `x86_64-pc-windows-msvc` syntax probe includes the real
`nexora/gameplay_assets.h` and list-initializes a `uint32_t` catalog field from a runtime enum
conditional. The original initializer fails with `-Wc++11-narrowing`; the explicit cast passes
with `-std=c++20 -Werror -fsyntax-only`. This checks the Windows ABI/compiler rule in Linux;
it does not claim a local MSVC/Windows SDK run. Full repaired-source Linux gates and real Windows
CI must pass before acceptance. Probe inputs/results are retained externally as
`catalog-enum-width.cpp` and `catalog-enum-width-result.json`.

PR Build's independent Windows Vulkan package job (`131086197468`) also reported
`Seven-step guided tour did not complete.` Push Build's same-head Vulkan job passed. Its root
cause is unconfirmed from the retained job log; no tour assertion, duration or input is weakened.
The complete native job will be checked again on the repaired head.

Before integrating main `69de02df` (accepted source API PR #769), repaired-source Development
and Shipping builds, strict Shipping package admission and focused asset/catalog/C-Zig checks
all passed (3/3 focused cases, 2.60 s). Its full 477-case CTest was intentionally interrupted
with exit 130 after 30 passing cases when that new main became available. The partial traversal
is retained as `sprite-msvc-full-ctest.log`, not claimed as a full pass; owned test descendants
were drained before merging. The integrated API/catalog source requires a fresh complete gate.

## Integrated file-catalog source API validation

Main `69de02df` contains accepted PR #769's compatible source API, source-content service suffix
and source-aware Player extraction. Six documentation conflicts are reconciled by preserving
both slices' contracts and evidence; no production source API implementation is duplicated.

The additional `twod.sprite_source_catalog_player` mode reuses the existing real SDK/Zig fixture
and all its scene/pool, negative-scale, handle, replacement and native pixel checks. It cooks the
initial red Atlas and Sprite source, deletes those two raw inputs **before compiling the module**,
then exports `sprite` before `atlas` in the manifest. The Player must admit that explicit Atlas
first and resolve the file-backed source. Replacement blue Atlas/shifted metadata remain embedded
to test explicit live registry replacement. Both original embedded mode and new file-catalog mode
must pass; the catalog case also rejects a corrupt cooked file before module Start, then restores
the package and verifies immutable bytes after relocation, CPU execution and Linux native capture.
This adds no new descriptor layout or rendering algorithm.

Integrated implementation head: `45b397eba9efe06326d2c15acd7aff6be7bd8ebb`, including main
`69de02df` and the MSVC repair. Development and Shipping configure/build passed. The fresh
unfiltered command below passed **482/482, zero failures/skips**, in **1341.74 seconds**, exit 0:

```sh
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development --output-junit /workspace/work/editor-completion/sprite-merged-full-junit.xml
```

The file-catalog Zig/native case passed in **40.878 s**, and original embedded mode in **35.6288 s**.
The native report records 30 Vulkan frames, two visible instances and balanced module allocations;
vendor 65541/device 0 identifies the software adapter. Exact pixel assertions retain the red/blue
regions and the old empty positions, checking pivot shifts on positive and negative scale.
Captures from this full traversal: [red](CatalogSource-red.png), [blue](CatalogSource-blue.png).
Layout restart (27.7596 s) and Xvfb lifetime (0.509132 s) also passed; no failed-case retry was needed.

The final Shipping Player additionally passed the same real Zig file-catalog/replacement case
using `--catalog --shipping`, with two visible CPU-extracted instances. Export uses the normal
Shipping dependency policy and asserts `development_test_copy=False`; no native Shipping pixel
or clean-host verdict is claimed. Exact command:

```sh
python3 Tests/TwoD/SpriteSourcePlayerAcceptance.py --app /workspace/Nexora/build/linux-shipping/Apps/TwoDPlayer/NexoraTwoDPlayer --compiler /workspace/Nexora/build/linux-development/Tools/TwoD/NexoraTwoDAssetCompiler --zig /workspace/work/editor-completion/tools/lib/python3.12/site-packages/ziglang/zig --cmake /workspace/work/editor-completion/tools/bin/cmake --catalog --shipping --evidence-dir /workspace/work/editor-completion/source-catalog-shipping-final
```

The independent C-only catalog/opaque/deferred/corruption suite also passed again under strict
Shipping policy. Final implementation hashes remain unchanged through the complete traversal.
The machine receipt distinguishes the original candidate, interrupted repair attempt and integrated
candidate, including selected tests, binaries, Windows-ABI probe and full-traversal pixel hashes.
Documentation routing unit tests remain 16/16 passing; final Markdown/bilingual, C++ format,
Python byte-compilation and diff checks are repeated after recording these results.

## Test index width follow-up

Head `79e5f67c`'s Windows SDK run `43213265709` passed all three jobs, confirming the production
MSVC C2397 repair. Its Windows Desktop (`131166249638`) and mimalloc (`131166249789`) jobs then
failed a separate test-only warning: `GameplayAssetsTests.cpp(302)`, C4267 (`size_t` to `unsigned`
argument), promoted to C2220 by the unchanged `/W4 /WX` policy. The final opaque-request index
now explicitly uses the ABI's `uint32_t`; the constructor already bounds this catalog to 4096.
All fixture data, request ordering and assertions remain unchanged. Production sources and ABI
are unchanged from the integrated candidate above.

A Clang 19 actual-translation-unit audit using each CMake compile command and
`-fsyntax-only -Werror=shorten-64-to-32` reproduced exactly that test line before repair. All five
touched C++ units pass after repair: Player, asset loader, asset tests, manifest tests and the
independent C ABI module. Compiler commands and diagnostics are retained in
`catalog-width-before.json` / `catalog-width-after.json`. CI warning policy is not changed.
The test-width repair receives a fresh full local gate and exact-head hosted CI before merging.

The same `79e5f67c` PR Vulkan tour passed while its push Vulkan tour again failed the final
seven-step state check (`131166502762`). Artifact `13943915817` was materialized through the GitHub
connector, but the workspace's enforced network allowlist does not permit the returned File Service
storage host. No proxy/policy bypass is attempted. Failure-only CI logging now prints the selected
room, ticks and tour fields from the replay, observed-completion and final snapshots. It preserves
the original failure code, native execution, 300-second deadline and all assertions; future failed
states can be inspected through the existing GitHub job-log tool without a network-policy change.

Implementation `df4a32bf` then passed Development configure/build and a fresh complete traversal:
**482/482, zero failures/skips, 1321.40 seconds, exit 0**, without retries. The command uses
`sprite-index-width-full-junit.xml` in the external task directory. Production implementation
hashes are unchanged from the integrated candidate; the prior Shipping build and both strict
Shipping package verdicts therefore remain applicable. All five touched C++ translation units
pass the width audit. Failure-only workflow logging was checked with YAML parsing and the 16/16
documentation-routing unit tests; its original native exit code remains intact. Final Markdown,
bilingual, formatting, byte-compilation and diff checks pass before publication. Repaired-head
hosted Windows/Desktop/mimalloc/native jobs still have to succeed before merge.

## Native test argument paths

Head `13baa763` compiled the Windows SDK, Desktop and mimalloc targets successfully, but push
mimalloc CTest failed `twod.sprite_catalog_export`: its fixture generator aborted with status
3221226505 while receiving the Unicode `source Unicode 精靈` output directory. The generator
still used narrow `main` arguments, unlike the Player/AssetCompiler's native Windows entrypoints.
It now uses `wmain` on Windows and carries native `filesystem::path` values directly into both
fixture modes; Linux retains `main`. MinGW receives its normal `-municode` test link option.
Exceptions now report their diagnostic and exit 1 instead of terminating without useful text.
Unicode test paths, cooked data and all assertions remain unchanged.

Development configure/build and focused asset, relocated Unicode package, residency stress and
real Zig transition checks pass **4/4** (7.88 s); the changed C++ unit retains a clean strict
width audit. Fresh full-gate and repaired-head Windows results remain required. Both `13baa763`
Windows Vulkan guided-tour jobs succeeded; earlier intermittent failures remain retained.

Implementation `504cc58508d96bbb8e564a2e5a0bb504e2edabc6` then passed Development configure/build
and the complete unfiltered Linux gate: **482/482, zero failures/skips, 1330.12 seconds, exit 0**,
without retries. The native file-catalog Zig case passed in 38.2141 s, the original embedded case
in 43.1693 s, Unicode catalog export in 1.02699 s and layout restart in 31.0394 s. Frozen hashes
match every implementation file and the failure-only workflow diagnostic. Production files remain
unchanged, so prior strict Shipping results still apply. The machine receipt preserves all earlier
candidates and adds this native-argument repair, including complete-log/JUnit hashes. Markdown,
bilingual, formatting and documentation-routing checks pass; repaired-head hosted CI is the
remaining merge gate.

## Windows snapshot publication race

Published head `ec4b51b2` passed the hosted Windows SDK producer/consumer, but PR Vulkan job
`131218056002` failed on a concrete sharing violation while reading `showcase-lab.json` during
an export. Its replay snapshot is valid and intentionally paused; the completion snapshot is null.
The test reader previously treated file existence as completed publication and reopened the live
files for copying, allowing both file-lock failures and partial JSON reads. This evidence does not
establish the cause of every earlier intermittent tour failure.

The self-contained packaged acceptance script now reads a complete JSON object and nonempty
Markdown with bounded retries inside the original five-second export deadline, and retains the
owned strings for evidence writes. No app simulation, input, seven-step/210-second assertion or
300-second completion deadline changes. A deterministic PowerShell regression loads the actual
reader via its AST without launching the Windows app. It reproduces the old exclusive-writer
sharing violation, verifies complete publication and owned evidence, and rejects eight invalid or
incomplete states. Its initial run exposed JSON-array admission; the explicit root-object check
repairs that case and all cases then pass under official PowerShell 7.5.4 on Linux. Windows
PowerShell 5 runs the same regression before native Vulkan acceptance in hosted CI. Fresh full
Linux validation and repaired-head hosted results remain required before merge.

The same published head's Windows mimalloc job `131218056211` confirms the Unicode generator and
both Sprite cooks now succeed. It then reports `KeyError: assets/0000.nxcook` in the new test's
inventory cross-check: native Windows digest keys contain backslashes, while the public inventory
uses POSIX paths. The test now uses `relative_to(...).as_posix()` without changing any file/hash
assertion or export behavior. Focused relocated catalog acceptance passes in 1.46 s on Linux.
That hosted run has **251 passes, seven existing unsupported skips and one failure / 259**;
earlier shorthand describing 258 passes counted skipped cases and must not be read as executed
passes. The early snapshot-reader full traversal was deliberately interrupted after one passing
test (exit 130) to freeze this additional repair; its log is retained separately. A fresh complete
gate covers the final combined repairs.

## Accepted Editor snapshot integration

PR #771 merged as main `f0e497e6` while the combined repair gate was running. That traversal is
retained as intentionally interrupted after 158 passing cases (exit 130), without claiming a full
pass. Local merge `5e205010` integrates the accepted immutable Editor indexing/reimport/Play capture
foundation and resolves five documentation conflicts by retaining both delivered slices. No runtime
source conflict occurred. Workflow artifact paths and both new Editor source tests are preserved.
The Windows fixture/snapshot-reader repairs are restored unchanged after integration. A fresh
Development/Shipping build and full unfiltered traversal validate the combined tree; workflow 4/9,
Editor 0/8 and full ED-WF-06/07 acceptance remain unchanged.

The integrated runtime plus frozen Windows verification repairs (committed as
`689a81ec49c584b989dc27ca674ab83ac1c379e6`) passed a fresh complete gate: Development and
Monolithic Minimal Shipping configure/build, followed by **485/485 Linux tests, zero failures/skips,
1378.70 seconds, exit 0**, without retries. All 18 recorded source/workflow hashes match. The three
accepted Editor source cases pass together with the catalog work: `twod.editor_sprite_sources`
0.0167996 s, `editor.sprite_source_play` 0.0133694 s and actual `editor.sprite_source_native`
1.49664 s. File-catalog Zig/native passes in 42.6902 s, embedded mode in 36.7861 s, catalog export
in 0.988307 s and layout restart in 30.3291 s.

After the quiet full gate, both strict Shipping package suites pass again using the rebuilt actual
Shipping Player: independent C-only preload/deferred/corruption/opaque checks and real Zig
`--catalog --shipping`, with `development_test_copy=False`, two extracted instances and balanced
allocations. These Shipping runs are CPU admission/extraction, not native or clean-host acceptance.
PowerShell 7.5.4 passes the synchronized writer-lock/owning-snapshot regression; Windows PowerShell
5 still requires actual hosted execution. Documentation routing 16/16, final Markdown/bilingual,
touched C++ formatting, Python compilation, workflow YAML and diff checks pass. The machine receipt
retains exact full/interrupted inventories, hashes, initial failures and rebuilt binary identities.
Exact published-head hosted CI remains required before squash merge.

## Acceptance limits

This slice adds file/registry admission and packaging. Host source-table exposure and source-bearing
Sprite/pool descriptors come from accepted PR #769. The joint file-catalog fixture exercises those
existing APIs through source-bound Player pixels. Immutable Editor source indexing/reimport and Play
capture come from accepted PR #771; Inspector assignment, Undo, authoring save/reopen, Build and
source-animation reload remain open. Source-free native and SDK behavior remains covered by the
unfiltered regression suite. No
Windows, macOS, Android, iOS, physical-device or clean-host run is claimed from this Linux session.
ED-WF-06 remains open, ED-WF acceptance stays **4/9**, and graphical Editor stays **0/8**.
