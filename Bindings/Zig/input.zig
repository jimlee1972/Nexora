//! Copied action input. Device controls: physical Window keys, mouse buttons 0..4,
//! gamepad buttons 0..31 and signed axes 32..47. All calls use the host owner thread.
pub const Device = enum(u32) { none = 0, keyboard = 1, mouse = 2, gamepad = 3 };
pub const Binding = extern struct {
    struct_size: u32 = @sizeOf(Binding),
    abi_version: u32 = 1,
    device: Device = .keyboard,
    device_id: u32 = 0,
    control: u32 = 0,
    scale: f32 = 1,
    deadzone: f32 = 0,
    reserved: u32 = 0,
};
pub const ActionState = extern struct {
    struct_size: u32 = @sizeOf(ActionState),
    abi_version: u32 = 1,
    sequence: u64 = 0,
    value: f32 = 0,
    held: u32 = 0,
    pressed: u32 = 0,
    released: u32 = 0,
};
pub const DeviceState = extern struct {
    struct_size: u32 = @sizeOf(DeviceState),
    abi_version: u32 = 1,
    sequence: u64 = 0,
    active_device: Device = .none,
    device_id: u32 = 0,
    focused: u32 = 0,
    reserved: u32 = 0,
};
pub const PointerState = extern struct {
    struct_size: u32 = @sizeOf(PointerState),
    abi_version: u32 = 1,
    sequence: u64 = 0,
    x: f64 = 0,
    y: f64 = 0,
    delta_x: f64 = 0,
    delta_y: f64 = 0,
    wheel_x: f64 = 0,
    wheel_y: f64 = 0,
    buttons: u32 = 0,
    focused: u32 = 0,
};
pub const Api = extern struct {
    struct_size: u32 = @sizeOf(Api),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    read_action: ?*const fn (?*anyopaque, u32, u32, *ActionState) callconv(.c) i32 = null,
    bind: ?*const fn (?*anyopaque, u32, u32, *const Binding) callconv(.c) i32 = null,
    clear_bindings: ?*const fn (?*anyopaque, u32, u32) callconv(.c) i32 = null,
    read_device: ?*const fn (?*anyopaque, u32, *DeviceState) callconv(.c) i32 = null,
    read_pointer: ?*const fn (?*anyopaque, u32, *PointerState) callconv(.c) i32 = null,

    pub fn action(self: *const Api, user: u32, id: u32, output: *ActionState) i32 {
        if (!self.valid()) return -2;
        const callback = self.read_action orelse return -2;
        return callback(self.context, user, id, output);
    }
    pub fn rebind(self: *const Api, user: u32, id: u32, binding: *const Binding) i32 {
        if (!self.valid()) return -2;
        const callback = self.bind orelse return -2;
        return callback(self.context, user, id, binding);
    }
    pub fn clear(self: *const Api, user: u32, id: u32) i32 {
        if (!self.valid()) return -2;
        const callback = self.clear_bindings orelse return -2;
        return callback(self.context, user, id);
    }
    pub fn device(self: *const Api, user: u32, output: *DeviceState) i32 {
        if (!self.valid()) return -2;
        const callback = self.read_device orelse return -2;
        return callback(self.context, user, output);
    }
    pub fn pointer(self: *const Api, user: u32, output: *PointerState) i32 {
        if (!self.valid()) return -2;
        const callback = self.read_pointer orelse return -2;
        return callback(self.context, user, output);
    }
    fn valid(self: *const Api) bool {
        return self.struct_size >= @sizeOf(Api) and self.abi_version == 1;
    }
};

/// Stable physical keys shared with native Window events.
pub const Key = struct {
    pub const unknown: u32 = 0;
    pub const tab: u32 = 1;
    pub const left_arrow: u32 = 2;
    pub const right_arrow: u32 = 3;
    pub const up_arrow: u32 = 4;
    pub const down_arrow: u32 = 5;
    pub const page_up: u32 = 6;
    pub const page_down: u32 = 7;
    pub const home: u32 = 8;
    pub const end: u32 = 9;
    pub const insert: u32 = 10;
    pub const delete: u32 = 11;
    pub const backspace: u32 = 12;
    pub const space: u32 = 13;
    pub const enter: u32 = 14;
    pub const escape: u32 = 15;
    pub const apostrophe: u32 = 16;
    pub const comma: u32 = 17;
    pub const minus: u32 = 18;
    pub const period: u32 = 19;
    pub const slash: u32 = 20;
    pub const semicolon: u32 = 21;
    pub const equal: u32 = 22;
    pub const left_bracket: u32 = 23;
    pub const backslash: u32 = 24;
    pub const right_bracket: u32 = 25;
    pub const grave_accent: u32 = 26;
    pub const caps_lock: u32 = 27;
    pub const scroll_lock: u32 = 28;
    pub const num_lock: u32 = 29;
    pub const print_screen: u32 = 30;
    pub const pause: u32 = 31;
    pub const keypad0: u32 = 32;
    pub const keypad1: u32 = 33;
    pub const keypad2: u32 = 34;
    pub const keypad3: u32 = 35;
    pub const keypad4: u32 = 36;
    pub const keypad5: u32 = 37;
    pub const keypad6: u32 = 38;
    pub const keypad7: u32 = 39;
    pub const keypad8: u32 = 40;
    pub const keypad9: u32 = 41;
    pub const keypad_decimal: u32 = 42;
    pub const keypad_divide: u32 = 43;
    pub const keypad_multiply: u32 = 44;
    pub const keypad_subtract: u32 = 45;
    pub const keypad_add: u32 = 46;
    pub const keypad_enter: u32 = 47;
    pub const keypad_equal: u32 = 48;
    pub const left_shift: u32 = 49;
    pub const left_control: u32 = 50;
    pub const left_alt: u32 = 51;
    pub const left_super: u32 = 52;
    pub const right_shift: u32 = 53;
    pub const right_control: u32 = 54;
    pub const right_alt: u32 = 55;
    pub const right_super: u32 = 56;
    pub const menu: u32 = 57;
    pub const digit0: u32 = 58;
    pub const digit1: u32 = 59;
    pub const digit2: u32 = 60;
    pub const digit3: u32 = 61;
    pub const digit4: u32 = 62;
    pub const digit5: u32 = 63;
    pub const digit6: u32 = 64;
    pub const digit7: u32 = 65;
    pub const digit8: u32 = 66;
    pub const digit9: u32 = 67;
    pub const a: u32 = 68;
    pub const b: u32 = 69;
    pub const c: u32 = 70;
    pub const d: u32 = 71;
    pub const e: u32 = 72;
    pub const f: u32 = 73;
    pub const g: u32 = 74;
    pub const h: u32 = 75;
    pub const i: u32 = 76;
    pub const j: u32 = 77;
    pub const k: u32 = 78;
    pub const l: u32 = 79;
    pub const m: u32 = 80;
    pub const n: u32 = 81;
    pub const o: u32 = 82;
    pub const p: u32 = 83;
    pub const q: u32 = 84;
    pub const r: u32 = 85;
    pub const s: u32 = 86;
    pub const t: u32 = 87;
    pub const u: u32 = 88;
    pub const v: u32 = 89;
    pub const w: u32 = 90;
    pub const x: u32 = 91;
    pub const y: u32 = 92;
    pub const z: u32 = 93;
    pub const f1: u32 = 94;
    pub const f2: u32 = 95;
    pub const f3: u32 = 96;
    pub const f4: u32 = 97;
    pub const f5: u32 = 98;
    pub const f6: u32 = 99;
    pub const f7: u32 = 100;
    pub const f8: u32 = 101;
    pub const f9: u32 = 102;
    pub const f10: u32 = 103;
    pub const f11: u32 = 104;
    pub const f12: u32 = 105;
    pub const left = left_arrow;
    pub const right = right_arrow;
    pub const up = up_arrow;
    pub const down = down_arrow;
};
