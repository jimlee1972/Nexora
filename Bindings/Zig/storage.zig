//! Durable, host-owned slot IO. Bytes/schema belong to gameplay; unrelated to hot reload.
//! Borrow Api only for the service lifetime and call it on the host owner thread.
pub const abi_version: u32 = 1;
pub const Result = enum(i32) {
    ok = 0,
    invalid_argument = -10,
    not_found = -11,
    buffer_too_small = -12,
    corrupt = -13,
    io_error = -14,
    limit_exceeded = -15,
    wrong_thread = -16,
};
pub const Info = extern struct {
    struct_size: u32 = @sizeOf(Info),
    abi_version: u32 = 1,
    schema_version: u32 = 0,
    payload_size: u32 = 0,
};
pub const Api = extern struct {
    struct_size: u32 = @sizeOf(Api),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    save: ?*const fn (?*anyopaque, [*]const u8, u32, u32, ?*const anyopaque, u32) callconv(.c) i32 = null,
    load: ?*const fn (?*anyopaque, [*]const u8, u32, ?*anyopaque, u32, *Info) callconv(.c) i32 = null,
    remove: ?*const fn (?*anyopaque, [*]const u8, u32) callconv(.c) i32 = null,
    exists: ?*const fn (?*anyopaque, [*]const u8, u32, *u32) callconv(.c) i32 = null,

    pub fn valid(self: *const Api) bool {
        return self.struct_size >= @sizeOf(Api) and self.abi_version == 1 and self.context != null and
            self.save != null and self.load != null and self.remove != null and self.exists != null;
    }
    /// Keys: 1..64 ASCII alphanumeric/_/-. Host enforces configured payload bound.
    pub fn saveSlot(self: *const Api, key: []const u8, schema: u32, payload: []const u8) i32 {
        if (!self.valid() or key.len > 64 or payload.len > 0xffffffff) return @intFromEnum(Result.invalid_argument);
        return self.save.?(self.context, key.ptr, @intCast(key.len), schema, payload.ptr, @intCast(payload.len));
    }
    /// Validates checksum and scope before returning required size and gameplay schema.
    pub fn probe(self: *const Api, key: []const u8, info: *Info) i32 {
        if (!self.valid() or key.len > 64) return @intFromEnum(Result.invalid_argument);
        return self.load.?(self.context, key.ptr, @intCast(key.len), null, 0, info);
    }
    /// Errors preserve both payload and info; probe first when allocating a buffer.
    pub fn loadSlot(self: *const Api, key: []const u8, payload: []u8, info: *Info) i32 {
        if (!self.valid() or key.len > 64 or payload.len > 0xffffffff) return @intFromEnum(Result.invalid_argument);
        return self.load.?(self.context, key.ptr, @intCast(key.len), payload.ptr, @intCast(payload.len), info);
    }
    pub fn removeSlot(self: *const Api, key: []const u8) i32 {
        if (!self.valid() or key.len > 64) return @intFromEnum(Result.invalid_argument);
        return self.remove.?(self.context, key.ptr, @intCast(key.len));
    }
    pub fn slotExists(self: *const Api, key: []const u8, output: *u32) i32 {
        if (!self.valid() or key.len > 64) return @intFromEnum(Result.invalid_argument);
        return self.exists.?(self.context, key.ptr, @intCast(key.len), output);
    }
};

test "invalid service preserves outputs and all helpers reject" {
    const std = @import("std");
    const api: Api = .{};
    var info: Info = .{ .schema_version = 42 };
    var bytes = [_]u8{17};
    var exists: u32 = 99;
    const invalid = @intFromEnum(Result.invalid_argument);
    try std.testing.expectEqual(invalid, api.saveSlot("run", 1, &bytes));
    try std.testing.expectEqual(invalid, api.probe("run", &info));
    try std.testing.expectEqual(invalid, api.loadSlot("run", &bytes, &info));
    try std.testing.expectEqual(invalid, api.removeSlot("run"));
    try std.testing.expectEqual(invalid, api.slotExists("run", &exists));
    try std.testing.expectEqual(@as(u32, 42), info.schema_version);
    try std.testing.expectEqual(@as(u8, 17), bytes[0]);
    try std.testing.expectEqual(@as(u32, 99), exists);
}
