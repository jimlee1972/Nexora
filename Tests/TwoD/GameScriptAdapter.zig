const std = @import("std");
const nx = @import("nx");

const Fixture = struct {
    memory: [4096]u8 align(64) = undefined,
    allocations: usize = 0,
    frees: usize = 0,
    size: u64 = 0,
    alignment: u64 = 0,
    balanced: bool = true,
    fail: bool = false,
    events: u32 = 0,

    fn allocate(pointer: ?*anyopaque, tag: u64, size: u64, alignment: u64) callconv(.c) ?*anyopaque {
        const self: *Fixture = @ptrCast(@alignCast(pointer.?));
        if (self.fail or size > self.memory.len or alignment > 64) return null;
        self.balanced = self.balanced and tag == 1;
        self.allocations += 1;
        self.size = size;
        self.alignment = alignment;
        return @ptrCast(&self.memory);
    }

    fn free(pointer: ?*anyopaque, tag: u64, memory: ?*anyopaque, size: u64, alignment: u64) callconv(.c) void {
        const self: *Fixture = @ptrCast(@alignCast(pointer.?));
        self.balanced = self.balanced and tag == 1 and memory == @as(*anyopaque, @ptrCast(&self.memory)) and
            size == self.size and alignment == self.alignment;
        self.frees += 1;
    }

    fn host(self: *Fixture) nx.GameplayHostV3 {
        var api = std.mem.zeroes(nx.GameplayHostV3);
        api.struct_size = @sizeOf(nx.GameplayHostV3);
        api.abi_version = nx.abi_version;
        api.context = self;
        api.allocate = allocate;
        api.deallocate = free;
        return api;
    }
};

const Game = struct {
    ticks: u32 = 0,

    pub fn init(_: *Game, context: *const nx.game.Context) !void {
        const fixture: *Fixture = @ptrCast(@alignCast(context.host.context.?));
        fixture.events += 1;
    }
    pub fn fixedUpdate(self: *Game, _: *const nx.game.Context, _: f64) !void {
        self.ticks += 1;
    }
    pub fn stop(self: *Game, context: *const nx.game.Context) void {
        const fixture: *Fixture = @ptrCast(@alignCast(context.host.context.?));
        fixture.events += self.ticks;
    }
    pub fn deinit(_: *Game, context: *const nx.game.Context) void {
        const fixture: *Fixture = @ptrCast(@alignCast(context.host.context.?));
        fixture.events += 10;
    }
};

fn load(comptime Script: type) !nx.GameModuleV3 {
    var module: nx.GameModuleV3 = undefined;
    module.struct_size = @sizeOf(nx.GameModuleV3);
    try std.testing.expectEqual(@as(i32, 0), nx.game.Module(Script).load(nx.abi_version, &module));
    return module;
}

test "typed lifecycle preserves per-instance state and host ownership" {
    var fixture = Fixture{};
    const host = fixture.host();
    const module = try load(Game);
    var state: ?*anyopaque = null;
    try std.testing.expectEqual(@as(i32, 0), module.create.?(&state, &host));
    try std.testing.expectEqual(@as(i32, 0), module.on_start.?(state));
    try std.testing.expectEqual(@as(i32, 0), module.fixed_update.?(state, 1.0 / 60.0));
    try std.testing.expectEqual(@as(i32, 0), module.update.?(state, 0));
    module.on_stop.?(state);
    module.destroy.?(state);
    try std.testing.expectEqual(@as(u32, 12), fixture.events);
    try std.testing.expectEqual(@as(usize, 1), fixture.allocations);
    try std.testing.expectEqual(fixture.allocations, fixture.frees);
    try std.testing.expect(fixture.balanced);
}

test "failed script initialization releases SDK allocation and clears output" {
    const Failing = struct {
        pub fn init(_: *@This(), _: *const nx.game.Context) !void {
            return error.Unsupported;
        }
    };
    var fixture = Fixture{};
    const host = fixture.host();
    const module = try load(Failing);
    var state: ?*anyopaque = &fixture;
    try std.testing.expectEqual(@as(i32, -2), module.create.?(&state, &host));
    try std.testing.expect(state == null);
    try std.testing.expectEqual(@as(usize, 1), fixture.frees);
    try std.testing.expect(fixture.balanced);
}

test "missing allocator and allocation failure reject before script initialization" {
    var fixture = Fixture{ .fail = true };
    var host = fixture.host();
    const module = try load(Game);
    var state: ?*anyopaque = &fixture;
    try std.testing.expectEqual(@as(i32, -3), module.create.?(&state, &host));
    try std.testing.expect(state == null);
    host.deallocate = null;
    try std.testing.expectEqual(@as(i32, -2), module.create.?(&state, &host));
    try std.testing.expectEqual(@as(u32, 0), fixture.events);
    try std.testing.expectEqual(@as(usize, 0), fixture.allocations);
}

test "empty script supports the V3 base prefix without advertising fixed update" {
    var fixture = Fixture{};
    var host = fixture.host();
    host.struct_size = nx.host_v3_base_size;
    const module = try load(struct {});
    try std.testing.expect(module.fixed_update == null and module.capabilities == 0);
    try std.testing.expect(module.save_state == null and module.load_state == null);
    var state: ?*anyopaque = null;
    try std.testing.expectEqual(@as(i32, 0), module.create.?(&state, &host));
    try std.testing.expectEqual(@as(i32, 0), module.on_start.?(state));
    try std.testing.expectEqual(@as(i32, 0), module.update.?(state, 0));
    module.on_stop.?(state);
    module.destroy.?(state);
    try std.testing.expect(fixture.balanced and fixture.allocations == fixture.frees);
    host.abi_version = 0;
    try std.testing.expectEqual(@as(i32, -2), module.create.?(&state, &host));
}

const Migrating = struct {
    pub const state_version: u32 = 7;
    counter: u64 = 0,
    scene: u64 = 31,
    entity: u64 = 45,
    elapsed: f64 = 1.25,
    active: bool = true,
    label: [3]u8 = .{ 'n', 'x', '1' },

    pub fn update(self: *Migrating, _: *const nx.game.Context, _: f64) !void {
        self.counter += 1;
    }

    pub fn saveState(self: *const Migrating, writer: *nx.game.StateWriter) !void {
        try writer.int(u64, self.counter);
        try writer.int(u64, self.scene);
        try writer.int(u64, self.entity);
        try writer.float(self.elapsed);
        try writer.boolean(self.active);
        try writer.bytes(&self.label);
    }

    pub fn loadState(self: *Migrating, reader: *nx.game.StateReader) !void {
        const counter = try reader.int(u64);
        const scene = try reader.int(u64);
        const entity = try reader.int(u64);
        const elapsed = try reader.float();
        const active = try reader.boolean();
        const label = try reader.bytes();
        if (scene == 0 or entity == 0 or label.len != 3) return error.InvalidArgument;
        try reader.finish();
        self.* = .{ .counter = counter, .scene = scene, .entity = entity, .elapsed = elapsed, .active = active, .label = label[0..3].* };
    }
};

test "typed migration frames restore explicit data without serializing host pointers" {
    var first = Fixture{};
    var second = Fixture{};
    const a = first.host();
    const b = second.host();
    const module = try load(Migrating);
    try std.testing.expectEqual(@as(u64, 1), module.capabilities);
    var old: ?*anyopaque = null;
    var candidate: ?*anyopaque = null;
    try std.testing.expectEqual(@as(i32, 0), module.create.?(&old, &a));
    defer module.destroy.?(old);
    try std.testing.expectEqual(@as(i32, 0), module.create.?(&candidate, &b));
    defer module.destroy.?(candidate);
    for (0..32) |_| try std.testing.expectEqual(@as(i32, 0), module.update.?(old, 0.1));
    const count = module.save_state.?(old, null, 0);
    try std.testing.expectEqual(@as(u32, 56), count);
    var bytes: [56]u8 = undefined;
    try std.testing.expectEqual(count, module.save_state.?(old, &bytes, bytes.len));
    try std.testing.expectEqualStrings("NXGS", bytes[0..4]);
    try std.testing.expectEqual(@as(u64, 32), std.mem.readInt(u64, bytes[16..24], .little));
    try std.testing.expectEqual(@as(u32, 0), module.save_state.?(old, &bytes, bytes.len - 1));
    try std.testing.expectEqual(count, module.save_state.?(old, &bytes, bytes.len));
    try std.testing.expectEqual(@as(i32, 0), module.load_state.?(candidate, &bytes, bytes.len));
    var restored: [56]u8 = undefined;
    try std.testing.expectEqual(count, module.save_state.?(candidate, &restored, restored.len));
    try std.testing.expectEqualSlices(u8, &bytes, &restored);
    try std.testing.expectEqual(@as(usize, 1), first.allocations);
    try std.testing.expectEqual(@as(usize, 1), second.allocations);
}

test "typed migration rejects corrupt schema, lengths, truncation and invalid values" {
    var fixture = Fixture{};
    const host = fixture.host();
    const module = try load(Migrating);
    var state: ?*anyopaque = null;
    try std.testing.expectEqual(@as(i32, 0), module.create.?(&state, &host));
    defer module.destroy.?(state);
    var valid: [56]u8 = undefined;
    try std.testing.expectEqual(@as(u32, 56), module.save_state.?(state, &valid, valid.len));
    for (0..valid.len) |length| {
        try std.testing.expect(module.load_state.?(state, &valid, @intCast(length)) != 0);
    }
    for ([_]usize{ 0, 4, 8, 12, 48, 49 }) |offset| {
        var corrupt = valid;
        corrupt[offset] = 255;
        try std.testing.expect(module.load_state.?(state, &corrupt, corrupt.len) != 0);
    }
    var trailing: [57]u8 = undefined;
    @memcpy(trailing[0..56], &valid);
    trailing[56] = 0;
    std.mem.writeInt(u32, trailing[12..16], 41, .little);
    try std.testing.expect(module.load_state.?(state, &trailing, trailing.len) != 0);
    try std.testing.expectEqual(@as(i32, -1), module.load_state.?(state, null, valid.len));
    try std.testing.expectEqual(@as(i32, -1), module.load_state.?(state, &valid, nx.game.maximum_state_bytes + 1));
    var bounded = nx.game.StateWriter{ .written = nx.game.maximum_state_bytes };
    try std.testing.expectError(error.InvalidArgument, bounded.int(u8, 1));
    try std.testing.expectError(error.InvalidArgument, bounded.float(std.math.nan(f64)));
}
