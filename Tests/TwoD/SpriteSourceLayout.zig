const std = @import("std");
const nx = @import("nx");
const c = @cImport({
    @cInclude("nexora/nexora.h");
});
test "source wires and both optional suffixes match C without changing legacy strides" {
    inline for (.{
        .{ nx.TwoDSprite, c.NexoraTwoDSprite },
        .{ nx.TwoDInstance, c.NexoraTwoDInstance },
        .{ nx.TwoDSourceSpriteV1, c.NexoraTwoDSourceSpriteV1 },
        .{ nx.TwoDSourceInstanceV1, c.NexoraTwoDSourceInstanceV1 },
        .{ nx.TwoDSourceApiV1, c.NexoraTwoDSourceApiV1 },
        .{ nx.TwoDApiV1, c.NexoraTwoDApiV1 },
        .{ nx.GameplayServicesApiV1, c.NexoraGameplayServicesApiV1 },
    }) |pair| {
        try std.testing.expectEqual(@sizeOf(pair[1]), @sizeOf(pair[0]));
        try std.testing.expectEqual(@alignOf(pair[1]), @alignOf(pair[0]));
        inline for (@typeInfo(pair[0]).@"struct".fields) |field|
            try std.testing.expectEqual(@offsetOf(pair[1], field.name), @offsetOf(pair[0], field.name));
    }
    try std.testing.expectEqual(@as(usize, 64), @sizeOf(nx.TwoDSprite));
    try std.testing.expectEqual(@as(usize, 144), @sizeOf(nx.TwoDInstance));
    try std.testing.expectEqual(@offsetOf(c.NexoraTwoDApiV1, "get_source_api"), nx.twod_v1_base_size);
}
fn getSources(_: ?*anyopaque, api: *nx.TwoDSourceApiV1) callconv(.c) i32 {
    api.* = .{ .set_sprite = set, .get_sprite = get, .create_instances = create, .read_instances = read, .write_instances = write };
    return 0;
}
fn set(_: ?*anyopaque, _: u64, _: ?*const nx.TwoDSourceSpriteV1) callconv(.c) i32 {
    return 0;
}
fn get(_: ?*anyopaque, _: u64, _: *nx.TwoDSourceSpriteV1) callconv(.c) i32 {
    return 0;
}
fn create(_: ?*anyopaque, _: ?[*]const nx.TwoDSourceInstanceV1, _: u32, _: ?[*]nx.TwoDInstanceHandle) callconv(.c) i32 {
    return 0;
}
fn read(_: ?*anyopaque, _: ?[*]const nx.TwoDInstanceHandle, _: u32, _: ?[*]nx.TwoDSourceInstanceV1) callconv(.c) i32 {
    return 0;
}
fn write(_: ?*anyopaque, _: ?[*]const nx.TwoDInstanceHandle, _: u32, _: ?[*]const nx.TwoDSourceInstanceV1) callconv(.c) i32 {
    return 0;
}
fn getContent(_: ?*anyopaque, api: *nx.content.SpriteApi) callconv(.c) i32 {
    api.* = .{ .register_sprite_source = register };
    return 0;
}
fn register(_: ?*anyopaque, _: ?*const anyopaque, _: u64, high: *u64, low: *u64) callconv(.c) i32 {
    high.* = 11;
    low.* = 12;
    return 0;
}
test "old prefixes never read optional getters and missing capabilities stay absent" {
    var legacy = nx.TwoDApiV1{ .struct_size = nx.twod_v1_base_size, .get_source_api = getSources };
    try std.testing.expect(nx.getTwoDSources(legacy) == null);
    legacy.struct_size = @sizeOf(nx.TwoDApiV1);
    try std.testing.expect((nx.getTwoDSources(legacy) orelse return error.MissingSourceApi).valid());
    legacy.abi_version = 2;
    try std.testing.expect(nx.getTwoDSources(legacy) == null);
    try std.testing.expect(nx.getTwoDSources(.{}) == null);
    var services = nx.GameplayServicesApiV1{ .struct_size = @offsetOf(nx.GameplayServicesApiV1, "get_sprite_content_api"), .get_sprite_content_api = getContent };
    try std.testing.expect(nx.getSpriteContent(services) == null);
    services.struct_size = @sizeOf(nx.GameplayServicesApiV1);
    const content = nx.getSpriteContent(services) orelse return error.MissingContent;
    var high: u64 = 0;
    var low: u64 = 0;
    try std.testing.expectEqual(@as(i32, 0), content.registerSpriteSource("source", &high, &low));
    try std.testing.expectEqual(@as(u64, 11), high);
    try std.testing.expectEqual(@as(u64, 12), low);
    services.abi_version = 2;
    try std.testing.expect(nx.getSpriteContent(services) == null);
    try std.testing.expect(nx.getSpriteContent(.{}) == null);
}
