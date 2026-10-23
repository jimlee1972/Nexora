//! Optional owner-thread lifecycle/close/rumble interface. Input coordinates are unchanged.
const std = @import("std");
pub const State = extern struct {
    struct_size: u32 = @sizeOf(State),
    flags: u32 = 1,
    sequence: u64 = 0,
    interruption_sequence: u64 = 0,
    suspend_sequence: u64 = 0,
    resume_sequence: u64 = 0,
    close_token: u64 = 0,
    disconnect_sequence: [4]u64 = .{0} ** 4,
    connected_mask: u32 = 0,
    rumble_mask: u32 = 0,
};
pub const Api = extern struct {
    struct_size: u32 = @sizeOf(Api),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    get_state: ?*const fn (?*anyopaque, *State) callconv(.c) i32 = null,
    set_close_guard: ?*const fn (?*anyopaque, u32) callconv(.c) i32 = null,
    resolve_close: ?*const fn (?*anyopaque, u64, u32) callconv(.c) i32 = null,
    rumble: ?*const fn (?*anyopaque, u32, f32, f32, u32) callconv(.c) i32 = null,
    pub fn valid(self: *const Api) bool {
        return self.struct_size >= @sizeOf(Api) and self.abi_version == 1 and self.context != null and self.get_state != null and self.set_close_guard != null and self.resolve_close != null and self.rumble != null;
    }
    pub fn state(self: *const Api, out: *State) i32 {
        if (!self.valid()) return -2;
        return self.get_state.?(self.context, out);
    }
    pub fn guardClose(self: *const Api, enabled: bool) i32 {
        if (!self.valid()) return -2;
        return self.set_close_guard.?(self.context, @intFromBool(enabled));
    }
    pub fn resolveClose(self: *const Api, token: u64, accept: bool) i32 {
        if (!self.valid()) return -2;
        return self.resolve_close.?(self.context, token, @intFromBool(accept));
    }
    pub fn vibrate(self: *const Api, device: u32, low: f32, high: f32, milliseconds: u32) i32 {
        if (!self.valid()) return -2;
        return self.rumble.?(self.context, device, low, high, milliseconds);
    }
};
/// Caller-owned policy helper: observe BEFORE any fixed simulation or UI commit.
/// Retain pending choices/transactions in the game state; this helper never submits them.
/// Snapshot the game at a consistent boundary and pass the actual storage result to saved().
pub const PauseGate = struct {
    observed: State = .{},
    controlled_device: ?u32 = null,
    paused: bool = false,
    snapshot_due: bool = false,
    save_failed: bool = false,
    remaining: f64 = 0,
    pub fn observe(self: *PauseGate, current: State) void {
        const disconnected = if (self.controlled_device) |id| id >= 4 or
            current.disconnect_sequence[@min(id, 3)] != self.observed.disconnect_sequence[@min(id, 3)] else false;
        const interrupted = current.interruption_sequence != self.observed.interruption_sequence or
            current.close_token != self.observed.close_token and current.close_token != 0 or disconnected;
        if (interrupted) {
            self.paused = true;
            self.snapshot_due = true;
            self.remaining = 0;
        }
        if (!self.ready(current)) {
            self.paused = true;
            self.remaining = 0;
        }
        self.observed = current;
    }
    fn ready(self: *const PauseGate, state_: State) bool {
        if (state_.flags != 1 or state_.close_token != 0) return false;
        if (self.controlled_device) |id| {
            if (id >= 4 or state_.connected_mask & (@as(u32, 1) << @as(u5, @intCast(id))) == 0) return false;
        }
        return true;
    }
    pub fn saved(self: *PauseGate, success: bool) void {
        self.snapshot_due = !success;
        self.save_failed = !success;
    }
    /// Invoke only for a fresh explicit confirmation, after transactions/drafts are resolved.
    pub fn confirmResume(self: *PauseGate) bool {
        if (!self.paused or self.snapshot_due or self.save_failed or !self.ready(self.observed)) return false;
        self.remaining = 3;
        return true;
    }
    /// Real foreground time only. Returns false throughout countdown including its final frame,
    /// so the final countdown delta cannot become battle time. No background catch-up.
    pub fn advance(self: *PauseGate, seconds: f64) bool {
        if (!std.math.isFinite(seconds) or seconds < 0) return false;
        if (!self.paused) return true;
        if (self.remaining > 0 and self.ready(self.observed) and !self.snapshot_due) {
            self.remaining = @max(0, self.remaining - @min(seconds, 0.1));
            if (self.remaining == 0) self.paused = false;
        }
        return false;
    }
};
