# ED-M0 recovery scope and frame-input validation — Linux, 2026-10-22

This supporting change does not accept ED-M0. Physical Linux display, Windows per-monitor DPI,
installed IME and visual/glyph evidence remain open. The baseline is main
`0c24ec75330ab5c94721847abca7656c5cb5e272`, containing PR #695 after its final 19-job CI passed.

## Reproduced problems and resulting contracts

An independent portable probe against the baseline host opened A's recovery modal, pressed its
actual Discard button, then released while drawing B. B's journal was removed and A's survived.
Releasing after detaching to a null workspace caused SIGSEGV. Detach without a click also retained
the stale modal. These probes exercised the real host in a portable ImGui context with synthetic input.

The host now owns copied project UUID/root values for recovery UI, rather than retaining a model
pointer. Scope replacement cancels the old popup, active pointer/navigation activation, error and
unconsumed choice. Held or trickled activation events drain normally; recovery buttons require a
neutral frame before accepting a fresh action. Newly discovered journals receive the same guard.
Missing journals close stale prompts, while a successful same-scope result remains consumable
exactly once. Direct `ApplyRecoveryChoice` remains usable without a preceding Draw and retains
same-scope duplicate rejection and failure/retry behavior.

Separate portable probes showed `SetDisplay(NaN, ...)` aborting on the first frame and a NaN delta
poisoning frame state before an assertion on the second frame. Positive infinity also persisted.
Nonfinite dimensions now use one logical unit; nonfinite deltas use 0.0001 seconds. Existing
finite lower bounds and valid values are preserved.

## Portable regressions

`editor.recovery_scope` exercises actual WindowEvent pointer/key translation and ImGui controls:

- 1x/2x pointer down on A followed by release on B or null; both original journal bytes survive.
- Same UUID/different root, and same root/different UUID with the same C++ workspace object reused.
- Old Enter/Space held across replacement for 45 frames, then one fresh keyboard recovery.
- Deferred press/release events with ImGui input trickling enabled, and newly discovered journals.
- Journal disappearance, same-scope error retention, cross-scope error/result revocation, direct
  calls, duplicate rejection and exactly-once successful results.

An initial focused test lacked visible keyboard navigation: it had a default NavId but had not
sent Tab, so ImGui correctly ignored Enter. The fixture now uses actual Tab down/up navigation,
checks visible Recover focus, then retains the original single action and all data assertions.
Production code was not changed to accommodate that fixture failure.

`editor.imgui_contract` adds multi-frame NaN, positive/negative infinity, zero, negative, below-bound
and normal display/delta cases, checking exact normalized values and finite ImGui time.

## Environment and validation

Debian 13 / GCC 14.2.0 / CMake 4.4.4 / Ninja 1.13.2 / Python 3.12.14 / Slang 2026.18.
Development used graphical shell and Slang ON, Zig gameplay OFF, Xvfb/X11 and Mesa lavapipe.
The external sysroot, driver/layer paths and writable task-owned Mesa shader cache were selected
through environment variables. The executor allowed Xvfb's local sockets. No physical display
was attached and no native input fixture or Window backend was changed.

```bash
cmake --preset linux-development
cmake --build --preset linux-development -j 4
ctest --preset linux-development -R '^editor\.(imgui_contract|recovery_scope)$' --output-on-failure
ctest --preset linux-development
ctest --preset linux-development --rerun-failed --output-on-failure
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j 2
```

| Check | Actual result |
| --- | --- |
| Development configure/build | Passed; 407 initial incremental steps, subsequent test-only rebuilds passed |
| Focused contracts after fixture correction | 2/2 passed, 1.87 s |
| Complete Development CTest | 364 executed: 363 passed, 1 failed, 0 skipped; 1148.38 s |
| Unchanged-source failed-test rerun | 1/1 passed; 92.45 s |
| Shipping Minimal engine configure/build | Passed; 9 incremental steps, Editor disabled |
| Changed Markdown/link/bilingual and diff checks | Passed |

The complete suite failed the unchanged `editor.linux_prefab_isolation` fixture at its single-Undo
properties/identity restoration assertion (25.42 s). The same-source standalone rerun passed its
complete workflow in 92.43 s. No authoring action retries, deadlines or data assertions were changed.
This is an intermittent result, not a diagnosed cause or fix, and not a clean first-pass suite.
The original CTest log/LastTest/failed-test records were retained externally. The original fixture
cleans its temporary project, so those records do not retain the failed project's exact bytes.
Existing actor-owned `nexora-nqn.15` remains open for native workflow investigation.

All 5,859 recorded source/test/tool/configuration hashes were unchanged through the complete suite.
The new recovery scope test passed there in 0.09 s. Native display/recovery relaunch, Game 2D's
32 cycles (66.46 s), both 1x/2x 1,000-operation authoring replays, context/surface lifetime and
Hierarchy lifecycle (164.41 s) also passed. Windows/macOS CI and physical-host acceptance are
separate gates; these Linux results do not certify them.

Tracking: `nexora-nqn.20` (audit), `.21` (finite inputs), `.22` (recovery scope).
