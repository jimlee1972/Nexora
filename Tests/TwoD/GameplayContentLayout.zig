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
}
