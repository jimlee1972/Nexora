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
thread once per input frame. Other platforms return false and require their own adapter; no
controller is simulated. Actual hardware/gamepad-model compatibility is not claimed by Linux cloud
unit tests.

`runtime.gameplay_action_input` covers rebinding, held and both edge states, repeated reads,
multiple contributing devices, active-device switching, deadzones, focus loss/recovery, disconnect,
user bounds, invalid numeric values, future output tails and wrong-thread rejection.
`api.gameplay_input_zig_abi` checks C/Zig sizes, alignment and every field offset and invokes all
Zig convenience guards. Native gamepad physical-device acceptance remains a hardware gate.
