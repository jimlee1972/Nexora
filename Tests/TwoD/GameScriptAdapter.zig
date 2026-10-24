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
