const std = @import("std");
const nx = @import("nx");
const c = @cImport({
    @cInclude("nexora/gameplay_services.h");
});
test "all effects C and Zig field offsets agree" {
    inline for (.{
        .{ c.NexoraTwoDEffectsPointV1, nx.twod_effects.Point },
        .{ c.NexoraTwoDEffectsLightV1, nx.twod_effects.Light },
        .{ c.NexoraTwoDEffectsOccluderV1, nx.twod_effects.Occluder },
        .{ c.NexoraTwoDEffectsSpriteV1, nx.twod_effects.Sprite },
        .{ c.NexoraTwoDEffectsBloomV1, nx.twod_effects.Bloom },
        .{ c.NexoraTwoDEffectsFrameV1, nx.twod_effects.Frame },
        .{ c.NexoraTwoDEffectsCapabilitiesV1, nx.twod_effects.Capabilities },
        .{ c.NexoraTwoDEffectsStateV1, nx.twod_effects.State },
        .{ c.NexoraGameplayServicesApiV1, nx.GameplayServicesApiV1 },
    }) |pair| {
        try std.testing.expectEqual(@sizeOf(pair[0]), @sizeOf(pair[1]));
        try std.testing.expectEqual(@alignOf(pair[0]), @alignOf(pair[1]));
        inline for (@typeInfo(pair[1]).@"struct".fields) |field|
            try std.testing.expectEqual(@offsetOf(pair[0], field.name), @offsetOf(pair[1], field.name));
    }
    try std.testing.expectEqual(@sizeOf(c.NexoraGameplayTwoDEffectsApiV1), @sizeOf(nx.twod_effects.Api));
    try std.testing.expectEqual(@offsetOf(c.NexoraGameplayTwoDEffectsApiV1, "submit_frame"), @offsetOf(nx.twod_effects.Api, "submit_frame"));
    try std.testing.expect(nx.getTwoDEffects(.{ .struct_size = @offsetOf(nx.GameplayServicesApiV1, "get_twod_effects_api") }) == null);
    try std.testing.expect(nx.getTwoDEffects(.{}) == null);
    try std.testing.expectError(error.Unsupported, (nx.twod_effects.Api{}).state());
    try std.testing.expectError(error.Unsupported, (nx.twod_effects.Api{}).submit(.{}, &.{}, &.{}, &.{}));
}
