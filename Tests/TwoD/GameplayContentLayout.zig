const std = @import("std");
const content = @import("content");
const c = @cImport({
    @cInclude("nexora/gameplay_content.h");
});
test "gameplay cooked content C/Zig layout" {
    try std.testing.expectEqual(@sizeOf(c.NexoraContentAsset), @sizeOf(content.Asset));
    try std.testing.expectEqual(@alignOf(c.NexoraContentAsset), @alignOf(content.Asset));
    inline for (.{ "high", "low", "kind", "count", "width", "height", "atlas_high", "atlas_low" }) |name| {
        try std.testing.expectEqual(@offsetOf(c.NexoraContentAsset, name), @offsetOf(content.Asset, name));
    }
    try std.testing.expectEqual(@sizeOf(c.NexoraGameplayContentApiV1), @sizeOf(content.Api));
    inline for (.{ "struct_size", "abi_version", "context", "register_atlas", "register_clip", "list_assets", "describe_asset" }) |name| {
        try std.testing.expectEqual(@offsetOf(c.NexoraGameplayContentApiV1, name), @offsetOf(content.Api, name));
    }
    const api = content.Api{};
    try std.testing.expect(!api.valid());
    try std.testing.expectEqual(@sizeOf(c.NexoraGameplaySpriteContentApiV1), @sizeOf(content.SpriteApi));
    try std.testing.expectEqual(@alignOf(c.NexoraGameplaySpriteContentApiV1), @alignOf(content.SpriteApi));
    inline for (.{ "struct_size", "abi_version", "context", "register_sprite_source" }) |name| {
        try std.testing.expectEqual(@offsetOf(c.NexoraGameplaySpriteContentApiV1, name), @offsetOf(content.SpriteApi, name));
    }
    try std.testing.expectEqual(@as(u32, c.NEXORA_CONTENT_SPRITE_SOURCE), content.sprite_source_kind);
    const sources = content.SpriteApi{};
    try std.testing.expect(!sources.valid());
    var high: u64 = 55;
    var low: u64 = 66;
    try std.testing.expectEqual(@as(i32, -1), sources.registerSpriteSource("payload", &high, &low));
    try std.testing.expectEqual(@as(u64, 55), high);
    try std.testing.expectEqual(@as(u64, 66), low);
}

fn registerSource(context: ?*anyopaque, bytes: ?*const anyopaque, length: u64, high: *u64, low: *u64) callconv(.c) i32 {
    const calls: *u32 = @ptrCast(@alignCast(context.?));
    calls.* += 1;
    if (bytes == null or length != 7) return -2;
    high.* = 11;
    low.* = 12;
    return 0;
}

test "optional Sprite content capability admission and forwarding" {
    var calls: u32 = 0;
    var api = content.SpriteApi{ .context = &calls, .register_sprite_source = registerSource };
    var high: u64 = 55;
    var low: u64 = 66;
    api.struct_size -= 1;
    try std.testing.expect(!api.valid());
    try std.testing.expectEqual(@as(i32, -1), api.registerSpriteSource("payload", &high, &low));
    api.struct_size = @sizeOf(content.SpriteApi);
    api.abi_version = 2;
    try std.testing.expect(!api.valid());
    try std.testing.expectEqual(@as(i32, -1), api.registerSpriteSource("payload", &high, &low));
    api.abi_version = 1;
    try std.testing.expectEqual(@as(i32, -1), api.registerSpriteSource("", &high, &low));
    try std.testing.expectEqual(@as(u32, 0), calls);
    try std.testing.expectEqual(@as(u64, 55), high);
    try std.testing.expectEqual(@as(u64, 66), low);
    try std.testing.expect(api.valid());
    try std.testing.expectEqual(@as(i32, 0), api.registerSpriteSource("payload", &high, &low));
    try std.testing.expectEqual(@as(u32, 1), calls);
    try std.testing.expectEqual(@as(u64, 11), high);
    try std.testing.expectEqual(@as(u64, 12), low);
    try std.testing.expectEqual(@as(i32, -2), api.registerSpriteSource("bad", &high, &low));
    try std.testing.expectEqual(@as(u32, 2), calls);
}
