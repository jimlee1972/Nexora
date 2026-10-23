# Gameplay action input for C and Zig

`GameplayInput` is a host-owned action-state provider; `Api()` returns a borrowed version-1
`NexoraGameplayInputApiV1` table. The owning thread calls `BeginFrame()` before feeding native
input and before invoking gameplay. Each `Submit(user, device, device_id, control, value)` updates
held actions immediately. Copies captured by gameplay retain the same sequence and edge flags
until the next frame. A press and release in one frame retain both edges. Opposing binding
values sum and clamp to [-1, 1]; action held means the resulting value is nonzero.

Each logical user (0..7) owns actions (1..256), each with at most 16 bindings. `bind` adds a control
or replaces the scale/deadzone of that same control. `clear_bindings` releases an action and lets
Zig replace all of its controls without C++ game code. A binding passes raw values only when their
absolute magnitude exceeds its deadzone; scale then controls direction. Device IDs are exact,
not wildcard matches. User isolation is enforced before evaluating controls. The active device is
the last device with changed nonzero input; releases do not steal that selection.

Stable `NEXORA_INPUT_KEY_*` and `input.Key.*` values cover every Window physical key. Mouse
button codes are left=0, right=1, middle=2, back=3, forward=4. Gamepad buttons use 0..31 and signed
axes use 32..47. These are raw adapter controls, so games should save per-device bindings rather
than assume one controller layout. The API can bind menu navigation, confirmation, cancellation
and gameplay actions using the same state and transitions.

`SetFocused(false)` clears every raw control, selects no device, cancels press edges, and releases held actions.
Unfocused input is ignored; refocusing requires fresh input and never restores stale keys.
`Disconnect(device, id)` clears that device across every user, releases affected actions and
clears its active-device selection. A native host must process focus and disconnect events before
calling gameplay. Host ownership, frame scheduling and native event translation remain engine
code; action definitions and rebinding are Zig code.

Every callback returns 0 on success, -1 for invalid descriptors/ranges, and -3 for wrong-thread or
allocation failure. Calls never defer work. Output descriptors require version 1 and at least the
known struct size. Successful copies retain the caller's size and unknown trailing bytes; failures
preserve all output bytes. The provider must outlive every module holding its table. Calls from
another thread fail and must not race with destruction. No native pointer is exposed.

## Copied mouse pointer

`read_pointer` / Zig `api.pointer` copies absolute client position, accumulated frame delta,
horizontal/vertical wheel units, five held button bits and focus state for one user. Hosts call
`SubmitPointer(user, x, y, wheelX, wheelY)` for native pointer/wheel events; button events use
`Submit` with mouse controls 0..4. Wheel-only batches retain the last known absolute position.
The first absolute sample after startup, blur or mouse disconnect anchors without displacement.
`BeginFrame` clears delta/wheel while preserving position and held buttons. Blur clears buttons,
delta/wheel and the anchor, and ignores pointer samples until refocus. Disconnect clears that
mouse's buttons and resets delta/wheel and anchor. Reads do not consume observations. Movement
saturates at +/-65,536 client pixels per frame per axis, wheel at +/-122,880 native units, and
nonfinite samples reject without mutation. Pointer movement/wheel selects the mouse device.
Tests cover accumulation, held buttons, frame reset, blur, reconnect, finite saturation, invalid
output preservation and unknown future output suffixes.

## Zig example

After negotiating the gameplay services and their input table:

```zig
const input = nexora.input;
var binding = input.Binding{ .device = .keyboard, .control = input.Key.w };
_ = api.rebind(0, 1, &binding);
binding = .{ .device = .gamepad, .control = 33, .scale = -1, .deadzone = 0.2 };
_ = api.rebind(0, 1, &binding);
var state = input.ActionState{};
if (api.action(0, 1, &state) == 0) {
    // state.value drives movement; state.pressed drives one-shot UI/gameplay events.
}
```

Bindings are ordinary values: a game's permanent-save payload can store them using its chosen
schema. This input provider does not own files or silently choose a save location.

## Native gamepad adapter and evidence

`NativeGameplayGamepad::Poll(input, user)` reads Linux `/dev/input/js0` through `js3` using
nonblocking descriptors. It discovers reconnects at most once per second, drains at most 256 events
per device per poll, handles initial axis/button state, normalizes signed axes, and calls
`Disconnect` on EOF/removal. It requires the OS to grant device-read access. Call it on its creating
thread once per input frame.

Windows dynamically loads System32 `xinput1_4.dll`, falling back to System32 `xinput9_1_0.dll`.
No XInput import library or worker is added. Each `Poll` copies all four XInput user slots and
submits their complete current state through the same provider. It intentionally does not skip
unchanged packet numbers: a full sample after focus recovery restores currently held controls.
An XInput slot transitioning to disconnected clears that slot via `Disconnect` across logical users,
so a removed controller cannot leave a held action. An available backend with no connected pads
returns true; that is not physical-controller acceptance. If both Windows backends are unavailable,
or the caller uses the wrong thread or a user outside 0..7, Poll returns false. The adapter owns the DLL until destruction; it borrows the
input provider only during Poll. One Poll submits all connected slots to its chosen logical user;
the host clears prior routing before assigning another user. Destroy on the owner thread; when
replacing an adapter while reusing input, the host disconnects its old slots first. Input submission may allocate in the
owner loop and is not a realtime callback. Public C++ consumers must rebuild for the private
Windows ownership member; no C/Zig wire layout changes.

Windows also enumerates attached DirectInput game controllers at most once per second, using
the system `dinput8` and `dxguid` libraries (private Windows-only linkage). The adapter owns its
DirectInput interface and device COM references until removal/destruction on the creating thread;
it does not initialize or change the host COM apartment. Nonexclusive background polling does not
take exclusive device access. GameplayInput still discards all samples while unfocused. Polling
and a bounded single reacquisition attempt run on the caller thread, without a worker or wait loop.
Acquisition clears old controls and defers the first snapshot to the following input frame,
avoiding an initial zero cache being interpreted as half-pressed triggers.
Failed reads immediately disconnect the slot; successful full snapshots restore held state after
focus recovery. Enumeration failure retains existing device ownership for a later retry.

Four shared Windows slots remain 0..3. Connected XInput users reserve their matching slot;
DirectInput pads occupy free slots, remain stable while connected and may move after reconnection.
An XInput connection taking an occupied slot first disconnects/releases its DirectInput owner.
DirectInput paths containing the Windows `IG_` marker are excluded to avoid duplicate XInput
submission. Device IDs are session slots, not persistent hardware identities; saved bindings must
be applied to the selected slot. A full four-slot adapter ignores extra devices until a slot frees.

Sony DUALSHOCK 4 VID 054c/PID 05c4 or 09cc uses a model profile. Other DirectInput joysticks
expose raw buttons (0..11 and 16..31), first POV directions at 12..15, and X/Y/Z/Rx/Ry/Rz/slider0/
slider1 at 32..39. Axes are configured to [-32768,32767] and normalized to [-1,1]; absent controls
are zero. Controls 40..47 are zero. Games can rebind generic layouts; no universal model mapping
or compatibility claim is made. Bluetooth, clones, DualSense, rumble and vendor controls require
their own measured evidence and are not implied by the PS4 USB run.

Windows XInput/DUALSHOCK 4 controls are mapped by the adapter; Linux indices remain raw driver values:

| Controls | Windows XInput / DUALSHOCK 4 mapping |
| --- | --- |
| 0..3 | A/Cross, B/Circle, X/Square, Y/Triangle |
| 4..7 | Left shoulder, right shoulder, Back/Share, Start/Options |
| 8..11 | Left/right thumb click, reserved/PS, reserved/touchpad click |
| 12..15 | D-pad up, down, left, right |
| 32..37 | Left X, left Y, left trigger, right X, right Y, right trigger |

DS4 digital L2/R2 are additionally available at 16/17; 18..31 and 38..47 are cleared.

Signed stick endpoints normalize to [-1,1], with Y down positive; triggers are [0,1]. Application
bindings choose scale/deadzone; the adapter applies no hidden deadzone. XInput Guide/vendor controls,
rumble, battery and platform remapping are outside this slice. Other
platforms return false and require their own adapter; no controller is simulated. Actual
hardware/gamepad-model compatibility is not claimed by Linux cloud unit tests.

`runtime.gameplay_action_input` covers rebinding, held and both edge states, repeated reads,
multiple contributing devices, active-device switching, deadzones, focus loss/recovery, disconnect,
user bounds, invalid numeric values, future output tails and wrong-thread rejection.
`api.gameplay_input_zig_abi` checks C/Zig sizes, alignment and every field offset and invokes all
Zig convenience guards. Native gamepad physical-device acceptance remains a hardware gate.


`runtime.gameplay_gamepad_routing` feeds copied XInput field samples through the actual provider:
button/D-pad routing, reserved zeros, stick endpoints/fractions, independent triggers, held/edge
behavior, focus recovery with unchanged state, disconnect, slot isolation, invalid user/device
preservation and wrong-thread rejection. Linux runs this decoder/state test without simulating a
native controller. Windows DLL polling and physical input still require target-host evidence.

The same test also covers copied DirectInput DS4 face/system/POV and six-axis routing, raw generic
controls, full-snapshot clearing on slot reuse, focus recovery, disconnect, bounds and wrong-thread
rejection. Those decoder tests do not establish physical-device acceptance.

## Optional motor output and lifecycle

The additive [Lifecycle V1 service](GameplayLifecycle.md) provides connected/rumble masks, per-slot
disconnect sequences and bounded motor requests. Windows XInput has native motor output; current
DirectInput/DS4 and Linux joystick adapters explicitly report unsupported. This supersedes the
rumble-outside-this-slice statement above only for XInput; physical/model acceptance is still open.
C++ consumers of `NativeGameplayGamepad` must rebuild for the added private observation state.
No Input V1 field or pixel-coordinate convention changed. Player losses clear unsafe held controls
immediately; UI activation is gated across the whole interruption batch, while fresh movement after
foreground restoration is preserved.
