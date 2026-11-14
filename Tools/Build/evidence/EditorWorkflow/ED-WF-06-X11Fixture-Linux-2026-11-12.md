# ED-WF-06 X11 legacy fixture boundary follow-up

PR #772, supporting task `nexora-whr.6.4.1`. Published head
`d5593a7ed6ef002dcef085b65d9b92b4cbc22904` passes its original complete Linux gate,
but push Build [43263512857](https://github.com/jimlee1972/Nexora/actions/runs/43263512857),
Linux desktop job [131341163595](https://github.com/jimlee1972/Nexora/actions/runs/43263512857/job/131341163595)
fails the existing `window.x11_held_key_native` legacy subcase.
The fixture repair is `f1bdb18b`; the [machine receipt](ED-WF-06-X11Fixture-Cloud.json)
freezes the compiled snapshot, all 486 cases, both 30-run inventories and native artifacts.

## Observed failure and contract

The hosted full suite records **509 passed, 1 existing optional-font skip, 1 failed / 511**, exit 8,
2188.63 seconds. All three real Zig Sprite Player cases pass: embedded 36.98 s, catalog 36.44 s,
native flip/repack 48.52 s. Normal detectable-repeat input passes, including 20 genuine rapid B taps.
Legacy mode receives 25 native N presses and 25 releases, and preserves all 25 text commits,
but emits two physical down/up pairs. Focus-reset, capability and lifecycle checks still pass.

The existing [Window contract](../../../../Engine/Window/README.md) normalizes only immediate,
already-buffered genuine legacy release/press pairs with matching window, keycode and timestamp.
Split arrivals are forwarded; no release waits for a future event. The old live legacy fixture
assumed that buffered-pair precondition without establishing or observing it. The exact missed
native pair is unavailable in the hosted artifact; the report does not prove its transport timing.
The failure is consistent with the documented boundary, not evidence of a new Sprite regression.
An unchanged local 30-iteration run passes all iterations (58.33 s); it does not reproduce or
erase the hosted failure.

## Fixture change

Production Window sources and behavior remain unchanged. The legacy fixture now confirms the
consumer is stopped, performs a genuine held N gesture using XTEST on its owned server, and
synchronizes the final release before resuming the consumer in a `finally` block. The existing
0.75-second hold, native-repeat/text/physical/focus/global-repeat/lifecycle assertions and all
deadlines remain. Normal detectable-repeat mode still processes the held gesture live and
preserves all 20 rapid B tap edges and text commits.

The test-only native observer peeks exclusively at an already-buffered immediate successor,
without reading, waiting or consuming. The helper additionally requires every repeated legacy
release except the final physical release to carry a matching buffered-press witness. At least
one repeat must occur; a missing stop acknowledgement or server synchronization fails. No skip,
assertion tolerance, retry-on-failure, production environment hook or event suppression is added.

Focused matcher/native tests pass 2/2. Thirty successive controlled native runs pass (57.70 s).
This is a scoped fixture check; the complete repaired-source gate and exact-head CI remain required.
The original failed CI and all subsequent results must remain separate in the final receipt.

## Complete gate

Fresh Development and Shipping configure/build exit 0. The complete repaired-source CTest gate
passes **486/486, zero failures/skips, 1392.69 seconds, exit 0**, with two workers and no failed-case
retries. All five frozen test-source hashes match. The native held-key case passes in 1.92392 s;
native source flip/repack in 48.7938 s; original embedded/catalog cases in 37.40/40.20 s; saved
layout restart in 30.2236 s. All five full-traversal source captures match the curated PNGs exactly.
The earlier 486/486 gate (1392.80 s) remains separately recorded in the original source receipt.
The rebuilt strict Monolithic Minimal Shipping Player repeats CPU admission and all five native
phases successfully: 30 frames, two visible instances, balanced allocations and
`development_test_copy=False`. All five Shipping PNGs match both the new Development traversal
and curated images exactly. This remains software Vulkan/Xvfb correctness, not physical input
or clean-host acceptance.

```sh
cmake --preset linux-development
cmake --build --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development --output-junit /workspace/work/editor-completion/repack-x11-full-junit.xml
ctest --preset linux-development -R '^window.x11_(held_key_native|key_repeat_matcher)$'
ctest --preset linux-development -R '^window.x11_held_key_native$' --repeat until-fail:30
python3 Tests/TwoD/SpriteSourcePlayerAcceptance.py --app /workspace/Nexora/build/linux-shipping/Apps/TwoDPlayer/NexoraTwoDPlayer --compiler /workspace/Nexora/build/linux-development/Tools/TwoD/NexoraTwoDAssetCompiler --zig /workspace/work/editor-completion/tools/lib/python3.12/site-packages/ziglang/zig --cmake /workspace/work/editor-completion/tools/bin/cmake --catalog --repack --shipping --native --evidence-dir /workspace/work/editor-completion/source-repack-x11-shipping
```

Workflow remains 4/9, graphical Editor 0/8 and 2D 9/11 until complete repaired-head CI and merge.
This fixture repair does not accept ED-WF-06 or physical input.
