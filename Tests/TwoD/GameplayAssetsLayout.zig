const std = @import("std");
const nx = @import("nx");
const c = @cImport({
    @cInclude("nexora/gameplay_services.h");
});
test "asset IO and every services prefix C/Zig layout" {
    inline for (.{
        .{ c.NexoraAssetCatalogEntryV1, nx.assets.CatalogEntry },
        .{ c.NexoraAssetLoadStateV1, nx.assets.LoadState },
        .{ c.NexoraAssetUsageV1, nx.assets.Usage },
        .{ c.NexoraGameplayAssetsApiV1, nx.assets.Api },
        .{ c.NexoraAudioUsageV1, nx.audio_state.Usage },
        .{ c.NexoraGameplayAudioStateApiV1, nx.audio_state.Api },
        .{ c.NexoraLifecycleStateV1, nx.lifecycle.State },
        .{ c.NexoraGameplayLifecycleApiV1, nx.lifecycle.Api },
        .{ c.NexoraGameplayServicesApiV1, nx.GameplayServicesApiV1 },
    }) |pair| {
        try std.testing.expectEqual(@sizeOf(pair[0]), @sizeOf(pair[1]));
        try std.testing.expectEqual(@alignOf(pair[0]), @alignOf(pair[1]));
        inline for (@typeInfo(pair[1]).@"struct".fields) |field| {
            try std.testing.expectEqual(@offsetOf(pair[0], field.name), @offsetOf(pair[1], field.name));
        }
    }
    const sp03 = nx.GameplayServicesApiV1{ .struct_size = @offsetOf(nx.GameplayServicesApiV1, "get_audio_state_api") };
    try std.testing.expect(nx.getAudioState(sp03) == null);
    const sp05 = nx.GameplayServicesApiV1{ .struct_size = @offsetOf(nx.GameplayServicesApiV1, "get_lifecycle_api") };
    try std.testing.expect(nx.getLifecycle(sp05) == null);
    const original = nx.GameplayServicesApiV1{ .struct_size = nx.services_v1_base_size };
    try std.testing.expect(nx.getAssets(original) == null);
    const sp01 = nx.GameplayServicesApiV1{ .struct_size = @offsetOf(nx.GameplayServicesApiV1, "get_ui_draw_api") };
    try std.testing.expect(nx.getAssets(sp01) == null);
    const sp02 = nx.GameplayServicesApiV1{ .struct_size = @offsetOf(nx.GameplayServicesApiV1, "get_assets_api") };
    try std.testing.expect(nx.getAssets(sp02) == null);
}
