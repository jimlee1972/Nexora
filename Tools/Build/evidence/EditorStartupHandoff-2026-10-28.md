# Windows Editor hidden startup and teardown repair

## Report and scope

The delivered `d8ce8168` Windows package could expose its Project Browser behind a splash stuck
at Preparing workspace; the user observed a Windows hung-window ghost. No hang dump or current
process was available. This repair removes a source-level Win32 wait cycle; it does not claim
that the user's physical machine or that exact hang was reproduced in Linux.
Base: `0c718ad0fd90b82eba6d853269d73c09476d0cd1` (includes PRs 724, 725, 728 and 729).
The approved artwork and complete private Windows Build/Play dependencies remain unchanged.

## Cause and contract

`Ready()` previously posted close then unconditionally joined a splash thread whose HWND was
owned by the main thread's HWND. Native DestroyWindow/activation messages can synchronously
call that owner while it waits in join. The main host also used an initially-visible descriptor
before GPU initialization. Z-order alone did not fix either contract.

The main starts hidden using the existing Window option forwarded through RenderSurface.
The dedicated splash has no cross-thread owner, no global topmost flag and no activation.
Stage delivery is posted. Close/join waits on an explicit thread-completion event while servicing
only incoming sent messages; it never dispatches posted Editor commands during teardown.
Native helper tests force a synchronous main notification during Ready and exceptional cleanup;
an unconditional join would hit the external CTest timeout.

The first hidden DX12 Acquire bypasses compositor-capacity waiting; the submitted draw's GPU
fence completes before Present returns. Occluded remains a distinct platform result. The app
accepts only this initial hidden completion for reveal instead of waiting forever for visibility
it must itself provide. It closes the splash then shows the prepared main on its own thread.
The first actual SHOW client capture must contain non-uniform, dark Project Browser/UI content,
including light content in multiple spatial regions; uniform black and white clients fail.
A GPU fence alone is not treated as proof of screen pixels.

## Regression and package admission

`WindowsStartupIntegration.py` runs the actual EXE twice with **no arguments**, isolated normal
LOCALAPPDATA and a populated default recent-project store. There is no `--frames`, graphical
mode or recents-path override in either interactive launch. Native SHOW event timestamps are
compared with flushed in-process hidden/close boundaries; asynchronous CREATE callbacks are
not used to infer historical visibility from current window state. The observer requires at
least one responsive splash probe and screen capture, checks main responsiveness, exercises
native title-bar mouse input/foreground, posts WM_CLOSE and checks zero exit and no orphan HWND.
Startup is bounded by 35 seconds, WM_NULL by 500 ms, shutdown by ten seconds and CTest by 150
seconds. Failed fixtures/logs/events and first-SHOW/main/splash BMPs are retained.
The full package verifier repeats this against staged and actual ZIP-extracted installations,
beside shipped-tool script Build/native game and 1,200-frame Editor Play/source conservation.

## Latest-main integration prerequisites

The unmodified base could not build with TwoD enabled: SpriteGeometry was defined inside an
unrelated anonymous namespace, optional source IDs were passed as raw AssetIds, and two source-aware
ExtractSprites definitions collided at link. It also compared disengaged optionals with zero IDs,
rejecting ordinary Editor sprites/animation. The repair retains Scene.cpp's pooled source-aware
extraction and the codec/standalone resolver, fixes optional presence checks without changing
source UUIDs, and keeps the legacy resolver-free overload's rejection of unresolved sources.
Two existing source-free wire/restore assertions now test absent optionals explicitly. No module,
source feature, fixture budget or test assertion is disabled. Existing source-scene, gameplay,
authoring, play-assets and context regressions remain admission requirements.

## Validation receipt

The initial default Linux configuration completed 250 tests with seven failures: the missing
cloud clang dependency and six latest-main TwoD optional/legacy integration regressions. After
installing clang and repairing those contracts, all seven focused regressions passed (3.60 s).
The graphical full build passed. The complete serial 392-case CTest traversal then finished in
1855.28 s: 384 passed, seven existing Slang-off conditional skips, one aggregate hierarchy lifecycle
timeout at 300.16 s (exit 8). No program source changed during that traversal. Using the existing
local-host `NEXORA_EDITOR_HIERARCHY_TIMEOUT_SCALE=2`, both affected unchanged fixtures passed:
deep hierarchy 69.10 s, lifecycle benchmark 249.45 s, total 318.57 s, exit 0. Both actual durations
also fit their original 180/300-second deadlines. Inputs, loop counts, assertions and frame budgets
are unchanged; the full failed log and rerun remain distinct. Combined latest coverage is 385
passed/seven conditional skips/no unresolved failed cases, **not** an uninterrupted green full run.
Latest-head hosted full CI (with its ordinary deadlines) remains mandatory before merge.
`cmake --preset linux-shipping` and `cmake --build --preset linux-shipping` also passed
(108 targets, default Minimal/Monolithic profile, Editor disabled by Shipping policy). Brand alpha/ICO/ZIP validation, all seven
packager unit tests, all 16 documentation-routing tests, changed Markdown/link routing and touched
C++ formatting pass locally. Hosted Windows and remaining full-gate results are recorded in the
PR after completion. Original failures
are retained in external command logs. Physical Windows clean-host, DPI/multi-monitor/IME and
complete Editor milestone acceptance remain open (0/8).

Beads `nexora-5d2` synchronization is separately blocked by `no common ancestor` on pull. The
original native backup and pre-remote-repair backup are retained; no shared Beads history is
initialized, replaced, forced or edited as part of this product repair.
