const std = @import("std");
const nx = @import("nx");
const c = @cImport({
    @cInclude("nexora/gameplay_services.h");
});
test "all effects C and Zig field offsets agree" {
    inline for (.{
        .{ c.NexoraGameplayTwoDEffectsApiV1, nx.twod_effects.Api },
        .{ c.NexoraTwoDEffectsPointV1, nx.twod_effects.Point },
        .{ c.NexoraTwoDEffectsLightV1, nx.twod_effects.Light },
        .{ c.NexoraTwoDEffectsOccluderV1, nx.twod_effects.Occluder },
        .{ c.NexoraTwoDEffectsSpriteV1, nx.twod_effects.Sprite },
        .{ c.NexoraTwoDEffectsBloomV1, nx.twod_effects.Bloom },
        .{ c.NexoraTwoDEffectsFrameV1, nx.twod_effects.Frame },
        .{ c.NexoraTwoDEffectsCapabilitiesV1, nx.twod_effects.Capabilities },
        .{ c.NexoraTwoDEffectsStateV1, nx.twod_effects.State },
        .{ c.NexoraTwoDEffectsDiagnosticsV1, nx.twod_effects.Diagnostics },
        .{ c.NexoraGameplayServicesApiV1, nx.GameplayServicesApiV1 },
    }) |pair| {
        try std.testing.expectEqual(@sizeOf(pair[0]), @sizeOf(pair[1]));
        try std.testing.expectEqual(@alignOf(pair[0]), @alignOf(pair[1]));
        inline for (@typeInfo(pair[1]).@"struct".fields) |field|
            try std.testing.expectEqual(@offsetOf(pair[0], field.name), @offsetOf(pair[1], field.name));
    }
    try std.testing.expectError(error.Unsupported, (nx.twod_effects.Api{}).poolIdentity(nx.TwoDInstanceHandle{ .owner = 1, .index = 0, .generation = 1 }));
    try std.testing.expectEqual(@offsetOf(c.NexoraGameplayTwoDEffectsApiV1, "submit_frame"), @offsetOf(nx.twod_effects.Api, "submit_frame"));
    try std.testing.expect(nx.getTwoDEffects(.{ .struct_size = @offsetOf(nx.GameplayServicesApiV1, "get_twod_effects_api") }) == null);
    try std.testing.expect(nx.getTwoDEffects(.{}) == null);
    try std.testing.expectError(error.Unsupported, (nx.twod_effects.Api{}).state());
    try std.testing.expectError(error.Unsupported, (nx.twod_effects.Api{}).submit(.{}, &.{}, &.{}, &.{}));
}

const OldHost = struct {
    fn capabilities(_: ?*anyopaque, output: *nx.twod_effects.Capabilities) callconv(.c) i32 {
        output.* = .{ .features = 1 };
        return 0;
    }
    fn state(_: ?*anyopaque, output: *nx.twod_effects.State) callconv(.c) i32 {
        output.* = .{ .active_revision = 17 };
        return 0;
    }
    fn submit(_: ?*anyopaque, _: *const nx.twod_effects.Frame) callconv(.c) i32 {
        return 0;
    }
    fn get(_: ?*anyopaque, output: *nx.twod_effects.Api) callconv(.c) i32 {
        const prefix = @offsetOf(nx.twod_effects.Api, "pool_identity");
        const source = nx.twod_effects.Api{ .struct_size = prefix, .get_capabilities = capabilities, .get_state = state, .submit_frame = submit };
        @memcpy(std.mem.asBytes(output)[0..prefix], std.mem.asBytes(&source)[0..prefix]);
        return 0;
    }
    fn pool(_: ?*anyopaque, owner: u64, index: u32, generation: u32, output: *u64) callconv(.c) i32 {
        if (owner != 0x1122334455667788 or index != 4 or generation != 19) return -1;
        output.* = 42;
        return 0;
    }
};
test "new Zig bindings retain old effects hosts and preserve complete pool handle fields" {
    const old = nx.getTwoDEffects(.{ .get_twod_effects_api = OldHost.get }) orelse return error.MissingOldHost;
    try std.testing.expect(old.valid());
    try std.testing.expectEqual(@as(u64, 17), (try old.state()).active_revision);
    const handle = nx.TwoDInstanceHandle{ .owner = 0x1122334455667788, .index = 4, .generation = 19 };
    try std.testing.expectError(error.Unsupported, old.poolIdentity(handle));
    var current = old;
    current.struct_size = @offsetOf(nx.twod_effects.Api, "get_diagnostics");
    try std.testing.expectError(error.Unsupported, current.diagnostics());
    try std.testing.expectError(error.Unsupported, current.debugView(0, 0));
    current.pool_identity = OldHost.pool;
    const sprite = try current.poolSprite(handle);
    try std.testing.expectEqual(@as(u64, 42), sprite.handle);
    try std.testing.expectEqual(@as(u32, 1), sprite.kind);
    var stale = handle;
    stale.generation -= 1;
    try std.testing.expectError(error.InvalidArgument, current.poolIdentity(stale));
}

test "companion emission codec validates borrowed links without altering the Sprite ABI" {
    var bytes = [_]u8{0} ** 68;
    std.mem.writeInt(u32, bytes[0..4], 0x4532444e, .little);
    std.mem.writeInt(u32, bytes[4..8], 1, .little);
    std.mem.writeInt(u64, bytes[8..16], 5, .little);
    std.mem.writeInt(u64, bytes[16..24], 6, .little);
    std.mem.writeInt(u64, bytes[24..32], 1, .little);
    std.mem.writeInt(u64, bytes[32..40], 2, .little);
    std.mem.writeInt(u64, bytes[40..48], 3, .little);
    std.mem.writeInt(u64, bytes[48..56], 4, .little);
    std.mem.writeInt(u32, bytes[56..60], 1, .little);
    std.mem.writeInt(u32, bytes[60..64], 17, .little);
    std.mem.writeInt(u32, bytes[64..68], 29, .little);
    const asset = try nx.twod_effects.EmissionAsset.parse(&bytes);
    try std.testing.expectEqual(@as(?u32, 29), asset.maskFor(17));
    try std.testing.expectEqual(@as(?u32, null), asset.maskFor(1));
    const sprite = try asset.apply(.{ .handle = 77 }, 17, .{ 1, 0.5, 0 }, 4);
    try std.testing.expectEqual(@as(u64, 77), sprite.handle);
    try std.testing.expectEqual(@as(u64, 3), sprite.emission_atlas_high);
    try std.testing.expectEqual(@as(u32, 29), sprite.emission_region);
    try std.testing.expectError(error.InvalidArgument, asset.apply(.{}, 1, .{ 1, 1, 1 }, 1));
    for (0..bytes.len) |length|
        try std.testing.expectError(error.InvalidArgument, nx.twod_effects.EmissionAsset.parse(bytes[0..length]));
    var trailing = [_]u8{0} ** 69;
    @memcpy(trailing[0..68], &bytes);
    try std.testing.expectError(error.InvalidArgument, nx.twod_effects.EmissionAsset.parse(&trailing));
    bytes[60] = 0;
    try std.testing.expectError(error.InvalidArgument, nx.twod_effects.EmissionAsset.parse(&bytes));
}
