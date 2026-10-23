const std = @import("std");
const nx = @import("nx");
const c = @cImport({
    @cInclude("nexora/gameplay_services.h");
});
test "display and extended services C/Zig layout" {
    inline for (.{
        .{ c.NexoraDisplayRectV1, nx.display.Rect },
        .{ c.NexoraDisplayStateV1, nx.display.State },
        .{ c.NexoraDisplayWindowV1, nx.display.Window },
        .{ c.NexoraGameplayDisplayApiV1, nx.display.Api },
        .{ c.NexoraGameplayServicesApiV1, nx.GameplayServicesApiV1 },
    }) |pair| {
        try std.testing.expectEqual(@sizeOf(pair[0]), @sizeOf(pair[1]));
        try std.testing.expectEqual(@alignOf(pair[0]), @alignOf(pair[1]));
        inline for (@typeInfo(pair[1]).@"struct".fields) |field| {
            try std.testing.expectEqual(@offsetOf(pair[0], field.name), @offsetOf(pair[1], field.name));
        }
    }
    const old = nx.GameplayServicesApiV1{ .struct_size = nx.services_v1_base_size };
    try std.testing.expect(nx.getDisplay(old) == null);
    try std.testing.expect(nx.getDisplay(.{}) == null);
}
