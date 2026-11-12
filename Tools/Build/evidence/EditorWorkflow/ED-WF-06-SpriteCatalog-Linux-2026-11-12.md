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

## Acceptance limits

This slice verifies file/registry admission and packaging. It does not expose the independent
source table through the gameplay Host, add source-bearing Sprite/pool descriptors, demonstrate
source-bound native pixels, or finish main Editor source indexing/Inspector/Undo/Play/Build.
Source-free native and SDK behavior remains covered by the unfiltered regression suite. No
Windows, macOS, Android, iOS, physical-device or clean-host run is claimed from this Linux session.
ED-WF-06 remains open, ED-WF acceptance stays **4/9**, and graphical Editor stays **0/8**.
