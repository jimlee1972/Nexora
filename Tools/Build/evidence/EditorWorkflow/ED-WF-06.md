# ED-WF-06: compatible Sprite sources and Player catalogs

Work order: [#747](https://github.com/jimlee1972/Nexora/issues/747), under
[Epic #741](https://github.com/jimlee1972/Nexora/issues/741). Date: 2026-11-12.
Acceptance follows merged registry/API/catalog/native delivery PRs #767, #769, #770 and #772.
This record accepts the C/Zig/Player work order; Editor authoring and animation/reload remain
ED-WF-07/08, and final Windows Editor delivery remains ED-WF-09.

## Evidence and behavior

| Required behavior | Executed evidence |
| --- | --- |
| Preserve existing C/Zig consumers; negotiate independent size/version-checked source access | [Registry evidence](ED-WF-06-SourceRegistry-Linux-2026-11-12.md), [API evidence](ED-WF-06-SourceAPI-Linux-2026-11-12.md). Historical Sprite/pool strides remain 64/144 bytes; independent source strides are 80/160. Prefix boundaries, tail canaries, absent/short/unsupported tables, actual old C++ headers/consumer and strict C11 consumers pass. Zig layout and real SDK-built gameplay consumers pass. |
| Resolve the canonical immutable Sprite/Atlas closure | [Catalog evidence](ED-WF-06-SpriteCatalog-Linux-2026-11-12.md). Catalog kind 4 is Sprite source; existing opaque kind 3 and copy/release semantics stay intact. Content kind 3 is independently Sprite source. Actual preloaded/deferred C-only and Zig consumers enforce cooked type/UUID/hash and Atlas-first closure, reject missing/corrupt/wrong-type dependencies and active-UUID replacement, and perform no implicit dependency I/O. Export uses the existing compiler and cooked schema. |
| Preserve owner thread, lifetime, old frames and failed-output behavior | Registry/API evidence and `twod.gameplay_content`, `twod.gameplay_assets`, `twod.sprite_source_gameplay`. Source publication/removal, Atlas replacement, active/live budgets, bounded retirement, wrong-thread rejection, output atomicity and retained immutable frames/owners pass. Existing reload proxies retain their contracts. |
| Actual C/Zig consumers resolve trim, Pivot, flips, negative scales and Atlas repacks | [Native acceptance](ED-WF-06-NativeSource-Linux-2026-11-12.md) and [receipt](ED-WF-06-NativeSource-Cloud.json). Real SDK Zig uses public negotiated tables and the file catalog. Five actual native captures check asymmetric quadrant colors, empty positions, Pivot-relative X/Y/XY mirrors and a negatively scaled live pool instance. Atlas-only replacement moves its packed rectangle and doubles its texture extent; source identity, transforms, current geometry and original handles remain intact. `twod.sprite_source` also executes real `PackAtlas` repacking and retained-frame assertions. |
| No authored-source or working-directory dependency | All three real Zig Player fixtures compile through the common SDK, export, relocate into a Unicode path and execute from an unrelated CWD after deletion of the authored project. File-catalog fixtures delete initial raw Atlas/source before module compilation. Corrupt cooked sources reject before Start; runtime package hashes stay unchanged. Strict actual Shipping catalog/native fixtures use `development_test_copy=False`. |
| Preserve schemas and explicit legacy behavior | `twod.sprite_source`, `twod.sprite_source_gameplay`, wire tests and API baseline guard. The 96-byte `.nx2sprite` schema 1 and source-bearing NX2S schema 3 are unchanged. Source-free scenes remain canonical schema 2; backward reads/mixed scenes pass. Old reads/animation reject a source binding without corrupting output; explicit legacy writes clear it. No fallback reinterprets source IDs as historical Atlas/Clip payloads. |
| Feature-OFF and test-free consumers still build | Explicit `NEXORA_ENABLE_2D=OFF`, `BUILD_TESTING=OFF` Modular Development builds of the actual general Host and Editor core pass. [Native receipt](ED-WF-06-NativeSource-Cloud.json) records exact flags and log hashes. The separate hosted build-contract jobs also pass. |

The Editor source-index/frozen-Play foundation from [PR #771](https://github.com/jimlee1972/Nexora/pull/771)
uses the same canonical resolver and immutable closure. Its [evidence](ED-WF-07-SourceSnapshots-Linux-2026-11-12.md)
does not accept source assignment in Inspector, Undo, authored save/reopen/Build or animation reload.

## Reproducible local gate

Implementation commit `96b8ab5ff400c8255564516f60df5af96717cd18` adds only the new Zig fixture,
its Python acceptance mode and CTest registration. The first published evidence head is
`d5593a7ed6ef002dcef085b65d9b92b4cbc22904`. Its push Linux legacy held-key fixture fails while
all source Player cases and PR/SDK checks pass. Fixture-only repair `f1bdb18b` establishes the
already-buffered legacy precondition without changing production Window behavior. Final published
head: `09cb057b69c2568deff6db5a72c09a1ac8fc87b1`, tree
`84e1ca9e207097bfde2c015de4324f2a0428d051`.
The [follow-up receipt](ED-WF-06-X11Fixture-Cloud.json) freezes all five test files, the complete
486-case inventory, original/new native observations and artifact/JUnit/log hashes. All three
source fixture hashes remain unchanged.
The acceptance documentation adds no implementation change.

```sh
cmake --preset linux-development
cmake --build --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development --output-junit /workspace/work/editor-completion/repack-x11-full-junit.xml
cmake -S /workspace/Nexora -B /workspace/work/editor-completion/repack-feature-off -G Ninja -DCMAKE_BUILD_TYPE=Development -DNEXORA_LINK_MODE=Modular -DBUILD_TESTING=OFF -DNEXORA_ENABLE_2D=OFF -DNEXORA_ENABLE_EDITOR_SDK=ON -DNEXORA_ENABLE_EDITOR=ON -DNEXORA_ENABLE_CRYPTOGRAPHY=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF -DNEXORA_ENABLE_NATIVE_BACKENDS=OFF -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_ENABLE_SLANG=OFF
cmake --build /workspace/work/editor-completion/repack-feature-off --target NexoraHost NexoraEditorCore --parallel 4
```

All commands exit 0. Full CTest: **486 registered, 486 passed, 0 failed, 0 skipped**,
with no failed-case retry. The repaired full gate is **1392.69 s**; the first **1392.80 s** gate
remains separate in the original receipt. Native repack: 48.7938 s; original embedded/catalog modes retain their
assertions/deadlines and pass. Independent strict Monolithic Minimal Shipping runs the same five
native phases, 30 real frames, two visible instances and balanced allocations. All five curated
Development/Shipping PNGs are byte-identical. Exact native CLI commands, inventories and artifact
hashes are in the linked native evidence/receipt; the catalog receipt separately retains strict
Shipping C-only and real Zig catalog admission runs.

The environment uses GNU 14.2, task-local CMake/Ninja/Zig and an extracted X11/Vulkan sysroot.
Native windows run under Xvfb with software Vulkan. This establishes Linux rendering correctness;
it does not establish physical-device performance, clean-host installation or final Windows ZIP
acceptance. Hosted Windows/macOS evidence is identified separately by Actions job.

## Delivery gate

All **41 latest exact-head Actions jobs succeed**; the [complete inventory](ED-WF-06-CI.md)
records all runs/jobs, earlier attempts and legacy status checks. PR #772 is merged as
`e9ab13accecc878f970c2e1ebb8d473303fab8c6`. Fetched main tree `84e1ca9e207097bfde2c015de4324f2a0428d051` exactly matches the tested head.
All supporting code PRs (#767, #769, #770, #772) are merged; this acceptance document
adds no runtime change and is validated through the established documentation route.

## Failure history and remaining work

The supporting receipts preserve their original failed gates separately from successful
ones: environment prerequisites, test fixture/optional wire-enumerator repairs, native Unicode
arguments and digest path normalization, Windows snapshot-reader sharing, and initial native
flip placement, plus the first-head X11 legacy fixture failure. The existing legacy fallback
normalizes only already-buffered pairs; the controlled fixture now proves that precondition and
retains every original physical/text/focus/lifecycle assertion. Its 30 native iterations pass,
but the unchanged original fixture also passes 30 local iterations, so exact hosted transport
timing remains unestablished. Original runtime assertions and capture deadlines remain. Interrupted runs and
an earlier unallocated hosted macOS runner cancellation are not counted as passes.

✅ ED-WF-06 accepts compatible source APIs and source-free Player delivery, bringing workflow
acceptance to **5/9 (55.6%)**. ED-WF-05, ED-WF-07, ED-WF-08 and ED-WF-09 remain open. Complete
graphical Editor milestones stay **0/8**; 2D stays **9/11**, and physical/clean-host gates remain open.
