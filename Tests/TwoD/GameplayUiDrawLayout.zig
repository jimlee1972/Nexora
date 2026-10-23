const std = @import("std");
const nx = @import("nx");
const c = @cImport({
    @cInclude("nexora/gameplay_services.h");
});
test "image UI and every services prefix C/Zig layout" {
    inline for (.{
        .{ c.NexoraUiImageV1, nx.ui_draw.Image },
        .{ c.NexoraUiImageDrawV1, nx.ui_draw.Draw },
        .{ c.NexoraUiInsets, nx.ui_draw.Insets },
        .{ c.NexoraGameplayUiDrawApiV1, nx.ui_draw.Api },
        .{ c.NexoraGameplayServicesApiV1, nx.GameplayServicesApiV1 },
    }) |pair| {
        try std.testing.expectEqual(@sizeOf(pair[0]), @sizeOf(pair[1]));
        try std.testing.expectEqual(@alignOf(pair[0]), @alignOf(pair[1]));
        inline for (@typeInfo(pair[1]).@"struct".fields) |field| {
            try std.testing.expectEqual(@offsetOf(pair[0], field.name), @offsetOf(pair[1], field.name));
        }
    }
    const original = nx.GameplayServicesApiV1{ .struct_size = nx.services_v1_base_size };
    try std.testing.expect(nx.getUiDraw(original) == null);
    const sp01 = nx.GameplayServicesApiV1{ .struct_size = @offsetOf(nx.GameplayServicesApiV1, "get_ui_draw_api") };
    try std.testing.expect(nx.getUiDraw(sp01) == null);
}
fn sp01Display(_: ?*anyopaque, out: *nx.display.Api) callconv(.c) i32 {
    out.* = .{};
    return 0;
}
test "new Zig retains SP-01 display getter without image UI suffix" {
    const services = nx.GameplayServicesApiV1{
        .struct_size = @offsetOf(nx.GameplayServicesApiV1, "get_ui_draw_api"),
        .get_display_api = sp01Display,
    };
    try std.testing.expect(nx.getDisplay(services) != null);
    try std.testing.expect(nx.getUiDraw(services) == null);
}
