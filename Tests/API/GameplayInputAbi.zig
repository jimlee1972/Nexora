const std = @import("std");
const input = @import("input");
const c = @cImport({
    @cInclude("nexora/gameplay_input.h");
});
test "input wire matches C on the current target" {
    inline for (.{
        .{ input.Binding, c.NexoraGameplayInputBindingV1 },
        .{ input.ActionState, c.NexoraGameplayActionStateV1 },
        .{ input.DeviceState, c.NexoraGameplayDeviceStateV1 },
        .{ input.PointerState, c.NexoraGameplayPointerStateV1 },
        .{ input.Api, c.NexoraGameplayInputApiV1 },
    }) |pair| {
        try std.testing.expectEqual(@sizeOf(pair[1]), @sizeOf(pair[0]));
        try std.testing.expectEqual(@alignOf(pair[1]), @alignOf(pair[0]));
        inline for (@typeInfo(pair[0]).@"struct".fields) |field|
            try std.testing.expectEqual(@offsetOf(pair[1], field.name), @offsetOf(pair[0], field.name));
    }
    try std.testing.expectEqual(c.NEXORA_INPUT_KEY_A, input.Key.a);
    try std.testing.expectEqual(c.NEXORA_INPUT_KEY_W, input.Key.w);
}
test "missing callbacks and old tables preserve outputs" {
    var api = input.Api{};
    var action = input.ActionState{ .value = 0.75 };
    var device = input.DeviceState{ .device_id = 7 };
    var pointer = input.PointerState{ .x = 19 };
    const binding = input.Binding{ .control = input.Key.w };
    try std.testing.expectEqual(@as(i32, -2), api.action(0, 1, &action));
    try std.testing.expectEqual(@as(i32, -2), api.rebind(0, 1, &binding));
    try std.testing.expectEqual(@as(i32, -2), api.clear(0, 1));
    try std.testing.expectEqual(@as(i32, -2), api.device(0, &device));
    try std.testing.expectEqual(@as(i32, -2), api.pointer(0, &pointer));
    api.struct_size = 8;
    try std.testing.expectEqual(@as(i32, -2), api.action(0, 1, &action));
    try std.testing.expectEqual(@as(f32, 0.75), action.value);
    try std.testing.expectEqual(@as(u32, 7), device.device_id);
    try std.testing.expectEqual(@as(f64, 19), pointer.x);
}
