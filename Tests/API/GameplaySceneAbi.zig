const std = @import("std");
const scene = @import("scene");
const c = @cImport({
    @cInclude("nexora/gameplay_scene.h");
});
test "scene service C/Zig sizes offsets and unsupported admission" {
    try std.testing.expectEqual(c.NEXORA_SCENE_REPLACE, @intFromEnum(scene.Mode.replace));
    try std.testing.expectEqual(c.NEXORA_SCENE_ADDITIVE, @intFromEnum(scene.Mode.additive));
    try std.testing.expectEqual(c.NEXORA_SCENE_QUEUED, @intFromEnum(scene.State.queued));
    try std.testing.expectEqual(c.NEXORA_SCENE_APPLIED, @intFromEnum(scene.State.applied));
    try std.testing.expectEqual(c.NEXORA_SCENE_FAILED, @intFromEnum(scene.State.failed));
    try std.testing.expectEqual(c.NEXORA_SCENE_CANCELLED, @intFromEnum(scene.State.cancelled));
    inline for (.{ .{ scene.Transition, c.NexoraSceneTransitionV1 }, .{ scene.Instance, c.NexoraSceneInstanceV1 }, .{ scene.Api, c.NexoraGameplaySceneApiV1 } }) |pair| {
        try std.testing.expectEqual(@sizeOf(pair[1]), @sizeOf(pair[0]));
        try std.testing.expectEqual(@alignOf(pair[1]), @alignOf(pair[0]));
        inline for (@typeInfo(pair[0]).@"struct".fields) |field| try std.testing.expectEqual(@offsetOf(pair[1], field.name), @offsetOf(pair[0], field.name));
    }
    const api = scene.Api{};
    try std.testing.expectError(error.Unsupported, api.active());
    try std.testing.expectError(error.Unsupported, api.resolve("Title.scene"));
    try std.testing.expectError(error.Unsupported, api.load(.{ .high = 1, .low = 2 }, .replace, false));
    try std.testing.expectError(error.Unsupported, api.status(1));
    try std.testing.expectError(error.Unsupported, api.info(1));
    try std.testing.expectError(error.Unsupported, api.unload(1));
}
