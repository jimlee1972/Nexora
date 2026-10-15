const std = @import("std");
const ui = @import("ui");
const c = @cImport({
    @cInclude("nexora/gameplay_ui.h");
});
test "gameplay UI C/Zig layout" {
    try std.testing.expectEqual(@sizeOf(c.NexoraUiRect), @sizeOf(ui.Rect));
    try std.testing.expectEqual(@alignOf(c.NexoraUiRect), @alignOf(ui.Rect));
    try std.testing.expectEqual(@sizeOf(c.NexoraGameplayUiApiV1), @sizeOf(ui.Api));
    try std.testing.expectEqual(@alignOf(c.NexoraGameplayUiApiV1), @alignOf(ui.Api));
    inline for (.{ "struct_size", "abi_version", "context", "begin_frame", "panel", "text", "button", "end_frame", "get_font", "has_glyph", "register_font" }) |name| {
        try std.testing.expectEqual(@offsetOf(c.NexoraGameplayUiApiV1, name), @offsetOf(ui.Api, name));
    }
    try std.testing.expectEqual(@sizeOf(c.NexoraUiGlyph), @sizeOf(ui.Glyph));
    try std.testing.expectEqual(@sizeOf(c.NexoraUiFontV1), @sizeOf(ui.Font));
    try std.testing.expectEqual(@alignOf(c.NexoraUiFontV1), @alignOf(ui.Font));
    inline for (.{ "struct_size", "width", "height", "reserved", "texture_id", "pixel_size", "line_height", "rgba", "rgba_length", "glyphs", "glyph_count", "reserved2" }) |name| {
        try std.testing.expectEqual(@offsetOf(c.NexoraUiFontV1, name), @offsetOf(ui.Font, name));
    }
    const api = ui.Api{};
    try std.testing.expect(!api.valid());
}
