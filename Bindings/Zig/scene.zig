//! Packaged authored-scene requests. Execution is deferred until the host frame boundary.
const std = @import("std");
pub const Id = struct { high: u64, low: u64 };
pub const Mode = enum(u32) { replace = 0, additive = 1 };
pub const State = enum(u32) { queued = 1, applied = 2, failed = 3, cancelled = 4 };
pub const Error = error{ InvalidArgument, Unsupported, Lifecycle, Busy, NotFound, Capacity };
fn check(code: i32) Error!void {
    return switch (code) {
        0 => {},
        -1 => error.InvalidArgument,
        -2 => error.Unsupported,
        -4 => error.Busy,
        -5 => error.NotFound,
        -6 => error.Capacity,
        else => error.Lifecycle,
    };
}
pub const Transition = extern struct {
    struct_size: u32 = @sizeOf(Transition),
    state: u32 = 0,
    request: u64 = 0,
    scene: u64 = 0,
    result: i32 = 0,
    reserved: u32 = 0,
};
pub const Instance = extern struct {
    struct_size: u32 = @sizeOf(Instance),
    flags: u32 = 0,
    scene: u64 = 0,
    asset_high: u64 = 0,
    asset_low: u64 = 0,
    camera: u64 = 0,
};
pub const Api = extern struct {
    struct_size: u32 = @sizeOf(Api),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    resolve_scene: ?*const fn (?*anyopaque, [*]const u8, u32, *u64, *u64) callconv(.c) i32 = null,
    request_load: ?*const fn (?*anyopaque, u64, u64, u32, u32, *u64) callconv(.c) i32 = null,
    request_unload: ?*const fn (?*anyopaque, u64, *u64) callconv(.c) i32 = null,
    get_status: ?*const fn (?*anyopaque, u64, *Transition) callconv(.c) i32 = null,
    get_active_scene: ?*const fn (?*anyopaque, *u64) callconv(.c) i32 = null,
    get_scene_info: ?*const fn (?*anyopaque, u64, *Instance) callconv(.c) i32 = null,
    pub fn valid(self: *const Api) bool {
        return self.struct_size >= @sizeOf(Api) and self.abi_version == 1 and self.context != null and
            self.resolve_scene != null and self.request_load != null and self.request_unload != null and
            self.get_status != null and self.get_active_scene != null and self.get_scene_info != null;
    }
    pub fn resolve(self: *const Api, path: []const u8) Error!Id {
        if (!self.valid()) return error.Unsupported;
        if (path.len == 0 or path.len > 1024 or path.len > std.math.maxInt(u32)) return error.InvalidArgument;
        var id: Id = undefined;
        try check(self.resolve_scene.?(self.context, path.ptr, @intCast(path.len), &id.high, &id.low));
        return id;
    }
    pub fn load(self: *const Api, id: Id, mode: Mode, persistent: bool) Error!u64 {
        if (!self.valid()) return error.Unsupported;
        var request: u64 = 0;
        try check(self.request_load.?(self.context, id.high, id.low, @intFromEnum(mode), @intFromBool(persistent), &request));
        return request;
    }
    pub fn loadPath(self: *const Api, path: []const u8, mode: Mode, persistent: bool) Error!u64 {
        return self.load(try self.resolve(path), mode, persistent);
    }
    pub fn unload(self: *const Api, instance: u64) Error!u64 {
        if (!self.valid()) return error.Unsupported;
        var request: u64 = 0;
        try check(self.request_unload.?(self.context, instance, &request));
        return request;
    }
    pub fn status(self: *const Api, request: u64) Error!Transition {
        if (!self.valid()) return error.Unsupported;
        var out = Transition{};
        try check(self.get_status.?(self.context, request, &out));
        return out;
    }
    pub fn active(self: *const Api) Error!u64 {
        if (!self.valid()) return error.Unsupported;
        var out: u64 = 0;
        try check(self.get_active_scene.?(self.context, &out));
        return out;
    }
    pub fn info(self: *const Api, instance: u64) Error!Instance {
        if (!self.valid()) return error.Unsupported;
        var out = Instance{};
        try check(self.get_scene_info.?(self.context, instance, &out));
        return out;
    }
};
