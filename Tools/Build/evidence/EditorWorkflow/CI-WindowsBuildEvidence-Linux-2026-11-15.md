# Windows Build failure evidence

Supporting task: `nexora-gu7.38.11.1`; base `41fc189018a338a291c3dae5c0c0ba3d8273f0e7`.
Parent `nexora-gu7.38.11` remains open: its intermittent settings-save cause is unproven.
Local diagnostic acceptance passes. Immutable-head hosted Windows gates remain required
before delivery. No Windows execution is claimed from Linux.

## Observed gap and resulting behavior

PR #793's unchanged `aed510e218c51cd07dc1f9a1d790031986acd58c` failed initial settings save in
Push job `132767917839` (run `43699585235`, 16.95 seconds), while the independent PR job and
one controlled unchanged Push retry passed. Earlier logs identified moving geometry, but did not
retain raw draft, saved settings or the complete native gesture sequence. Those observations
establish nondeterminism, not a stale-layout, focus or save-publication cause.

Previously `Editor.abort()` dropped the input list and the desktop artifact omitted the fixture's
evidence directory. Text replacement clicks and UTF-16 messages were also absent from that list.
The corrected fixture copies evidence before normal close, failure termination and project deletion,
including startup-window failure. A secondary capture error cannot bypass process drain. The
desktop artifact's existing always-upload step now includes `Tests/Editor/windows-build-workflow/`.
An added regression first reproduced repeated cleanup replacing pre-termination evidence with
post-exit bytes. Once the process has exited, repeated abort now preserves its successful prior
capture; a still-live process or failed earlier capture can capture again. This Python-only
correction preceded its full-suite case; the compiled sources and running native reload were unchanged.

The application emits escaped raw UTF-8 settings for delivered requests and save/reload outcomes.
Its private test-build accessor copies draft and displayed buffers after drawing, with frame,
ready/open/status observations. It owns all strings and grants no input, request or file authority.
Normal CLI JSON escaping is reused, preserving path behavior. No public or stable ABI changes.

The fixture records input intent before delivery, each posted message/result, triple-click text
replacement and intended text, owned client geometry/DPI/focus and numeric foreground ownership.
It never collects foreign window titles or pixels. Saved-byte copies are limited to 256 KiB;
stderr parsing to its last 4 MiB, inputs/native events to 2048/8192, observations to 128 per kind
and malformed rows to 32. Missing/unreadable/truncated states, offsets, captured-byte hashes and
file metadata are explicit. Captured hashes do not claim an atomic filesystem transaction.

## Acceptance and limits

The seven portable cases check close/abort ordering by changing the saved file during drain,
startup ownership, cleanup despite capture failure, bounded malformed/missing data and the
unchanged three-click/UTF-16 protocol including surrogate pairs. Both ordinary and optimized
Python runs pass. The actual Windows fixture additionally requires matching Unicode draft,
displayed fields, delivered Save, applied Save and saved bytes; a third idle owned Editor exercises
the real abort path and checks unchanged authored files and no child processes.

The full-suite Linux native Clip picker also verifies the new application stream: eight draft
observations and two delivered/applied saves parse as JSON, with matching draft/displayed fields
for SceneOnly and ScriptProject. Its actual log hashes are retained in the machine receipt. This
checks the shared C++ observer locally; Windows focus/input and abort receipts require native CI.

The first complete Linux gate ran **555 cases**, **554 passed / one failed**, zero skips,
**4561.17 seconds**. The unchanged `editor.linux_native_play_pointer_motion` failed a captured-wheel
pixel assertion in 4.64 seconds; the new evidence case passed. An unchanged CTest repetition then
passed four times before a 2.94-second pointer-move pixel failure. Both retained logs show the
corresponding input reached gameplay. Subsequent observation variants and twelve plain unchanged
controls passed. Pipe capacities and draw/pixel observations do not establish a root cause; no
pointer fixture, assertion, sleep or 45-second bound was changed. Independent open investigation
`nexora-gu7.38.13` retains these failures and controls. A second complete configure/build/unfiltered
gate is required before delivery; passing controls are not a repair or a substitute for that gate.

All original Unicode/mode/save/reopen, native game frames/window/console, repeated Stop, compiler
rollback, stale-source discard, cancellation and authored-byte assertions remain. Input sleeps,
operation/startup deadlines and the 360-second CTest limit remain unchanged. Observations can
affect timing; successful diagnostic runs do not prove the original intermittent defect repaired.
The separate Stop investigation, standalone reload ownership, workflow **5/9**, graphical Editor
**0/8**, TwoD **9/11**, physical and clean-host acceptance remain unchanged.

Full `linux-development` configure/build/unfiltered CTest passes **555/555**, zero failures/skips,
**4578.40 seconds**. Full Monolithic `linux-shipping` and the test-free graphical Development
Editor build/CLI startup pass. Both changed C++ translation units also pass actual GNU 15 commands;
format and changed-document checks pass. The [machine receipt](CI-WindowsBuildEvidence-Cloud.json)
records source hashes, scopes and commands. Shipping deliberately disables the Editor; an initial
invalid graphical Shipping request was rejected at configure and retained separately. The corrected
Shipping and separate Development test-free graphical gates pass.
Immutable-head hosted Windows native receipts and the uploaded artifact remain required before merge.
