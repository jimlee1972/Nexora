# Optional gameplay lifecycle and motor output

`NexoraGameplayLifecycleApiV1` is independently negotiated through the optional services suffix
`get_lifecycle_api`. ABI V3 and original Input/Audio tables remain unchanged. On 64-bit targets,
services prefixes 56/64/72/80/88 remain valid; the full table is now 96 bytes. Hosts copy only a
complete requested prefix and preserve unknown trailing bytes. New modules check the suffix size
before reading its getter; old hosts produce an absent service (`nx.getLifecycle == null`).

All calls, native polling, event feeding, and destruction are serialized on the host owner thread.
Tables/contexts borrow their provider until after module Stop/Destroy. No function schedules a save,
worker or callback; the backend motor callback/context must outlive `GameplayLifecycle` destruction.
Query output requires the known size, preserves the caller size and future suffix, and is unchanged
on failure. Status codes are 0 success, -1 invalid argument/token, -2 unsupported size/device/backend,
and -3 wrong thread, unsafe lifecycle state or native output failure. No exception crosses the C ABI.

## State and input boundary

Flags are focused=1, minimized=2, suspended=4. Only flags==1 permits nonzero motor output.
`sequence` changes on state transitions; separate monotonic interruption/suspend/resume and per-slot
disconnect sequences retain a loss and recovery within one event batch. Reads never consume them.
A continuous native read outage generates one disconnect edge; repeated failed polls still clear
held controls but cannot trigger one new snapshot per frame. Device IDs are session slots 0..3; reconnection may change the physical device in a slot. Windows
adapter disconnect observations additionally retain same-slot backend replacement and failed input
reads. A game chooses the controlled slot in Zig; a disconnected unused pad need not pause combat.

The 2D Player feeds native events and device observations before module fixed/variable updates.
It clears action/pointer/UI edges for an interruption batch, including loss followed by recovery;
a held confirmation cannot commit a pending choice in that batch. Built-in UI navigation/confirmation
and pointer activation additionally wait for a focused neutral frame before rearming. The Player
restores input routing before each complete gamepad sample, so cleared or ignored input is never
mistaken for physical release; a newly connected controller also requires neutral rearming. Games using
their own action IDs must apply the same release-before-confirm rule to their own UI input. Zero/occluded surfaces still run
module callbacks so Zig can save and resolve close while minimized, but do not extract/draw/present.
The Player does not select a game's simulation or transaction policy. Existing modules retain their
update contract; **Zig must observe lifecycle before every fixed update**, not only variable update.

Win32 translates WM_POWERBROADCAST suspend and automatic/user resume notifications. Dedicated
minimize/restore events survive resize coalescing. These are notifications, not an OS power veto or
a promise that disk IO can finish before sleep. X11/Cocoa currently supply focus/extent/close only;
no power integration is implied. Forced process termination, crash, power loss, and OS session end
can bypass the close handshake. Keep the previous reliable snapshot and a normal checkpoint policy.

## Explicit close handshake

`set_close_guard(1)` explicitly opts in. Querying the API alone never changes legacy close behavior.
With no guard, a native close exits as before. With a guard, a native close assigns a nonzero token,
stops motors and leaves the window alive. Repeated requests retain that token. The Player cancels
RenderSurface's close flag and reacquires on the next frame. External destruction cannot be cancelled.

`resolve_close(token, 0)` cancels; `resolve_close(token, 1)` authorizes exit after gameplay returns.
Only the exact currently pending token works; stale/zero tokens fail. Guard cannot be disabled while
close is pending or exit has been accepted. Accept does **not** save or report a save success: Zig
must serialize its consistent snapshot, call Storage V1, and accept only its successful result.
On failure, display retry/cancel, retain pending drafts and the previous file. Do not retry failures
unboundedly each frame or consume a closing click as a gameplay/UI confirmation.

## Bounded rumble

Motor gains are finite [0,1], duration 1..5000ms. Both zero stops and allows duration 0. Windows
loads XInputSetState from the same System32 DLL as input; there is no worker/import-library change.
Only connected XInput slots with that function and a successful XInputGetCapabilities motor-support
query set `rumble_mask`; DirectInput/DS4, Linux joystick
and other backends report unsupported rather than pretend success. Model-specific output needs a
separate implementation and physical evidence. `connected_mask` is independent of rumble support.

Commands submit native output synchronously; failures are returned unchanged. Tick expires commands
using steady-clock time. Focus loss, minimization, suspend, close and Stop request zero immediately;
a failed zero is retried each owner tick without replaying the nonzero gain. Disconnect discards its
command; reconnect never restores it. Native adapter destruction also attempts zero before DLL unload.
The host must continue ticking; this is not a hardware watchdog and cannot guarantee output cessation
during a frozen/crashed process or OS suspension. A lost native connection may make zero impossible.

Zig maps the user's 0..100 setting to gain/100, and setting zero sends an explicit zero immediately.
The diagnostic starts at 50%, supports 0..100 through its test control, and exposes rumble-off UI.

## Zig integration

`Bindings/Zig/lifecycle.zig` supplies guarded queries/calls and caller-owned `PauseGate` policy.
Observe before simulation or committing UI, preserve original menus/pending selections in game state,
and serialize them at a consistent boundary. `saved(storage_result == 0)` records the real outcome.
`confirmResume()` is only called after a fresh explicit confirmation and resolved transactions;
focus/reconnect alone never resumes. Advance its countdown with foreground delta, capped at 100ms;
the final countdown frame contributes no battle time. On a new interruption, cancel the countdown.
No UI scale, camera, collision, gameplay resource recovery, or card submission is performed by this helper.

`Gameplay/Zig/src/lifecycle_slice.zig` is a complete V3 diagnostic module using only Zig for policy,
UI and snapshot serialization. It retains a draft marker and battle-tick counter, writes a versioned
little-endian payload through Storage, retries failed saves only on request, supports close cancellation
and save-before-accept, and reopens paused. Its marker is not a production card/transaction system.
Build `NexoraZigLifecycleSliceBuild`, export a project with that module and no assets, and launch the
2D Player with `--run-2d-project game.nx2game --backend vulkan` (or `--backend dx12`).
Use the established Player backend flags described in `Apps/TwoDPlayer/README.md` on your target.

Portable C++ tests cover tokens, complete legacy prefix canaries, wrong-thread/output preservation,
capability rejection, expiry/blur/stop, failed native zero retry and state sequences. Real Zig tests
use the actual Storage implementation and force a nonempty `.tmp` directory to reject atomic writes,
checking that the old payload survives, retry succeeds, and a fresh module reopens the pending marker.
`gameplay.zig_minimized_modules` dynamically loads the actual SP-01 display and SP-04 color
modules, repeats zero-extent updates, and verifies restored UI and clean module teardown. Both
older modules now skip only invalid display mapping/drawing while their host can process lifecycle.
The held-confirmation regression uses actual GameplayInput and the Player's shared polling/gating
helper, including the cleared first frame, multiple held samples, physical release and a fresh press.
A standalone exported Player run gates service negotiation/UI/ownership. Win32 message injection tests
translation, not actual sleep or a physical controller. See [cloud evidence](../../Tools/Build/evidence/SP06-Lifecycle-Cloud-2026-10-23.md).
Physical Windows lifecycle, final game transactions, actual motor sensation and shutdown remain open
under Beads `nexora-4j7.1`; the cloud slice does not complete SP-06 product acceptance.
