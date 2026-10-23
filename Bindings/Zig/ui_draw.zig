//! Optional image UI; compose layout, screens and rules in Zig. Coordinates are logical pixels.
const std = @import("std");
const ui = @import("ui.zig");
pub const Image = extern struct {
    struct_size: u32 = @sizeOf(Image),
    width: u32 = 0,
    height: u32 = 0,
    reserved: u32 = 0,
    texture_id: u64 = 0,
    rgba: ?[*]const u8 = null,
    rgba_length: u64 = 0,
};
pub const Draw = extern struct {
    struct_size: u32 = @sizeOf(Draw),
    rgba: u32 = 0xffffffff,
    image: u64 = 0,
    source: ui.Rect = .{},
    destination: ui.Rect = .{},
    clip: ui.Rect = .{},
};
pub const Insets = extern struct { left: f32 = 0, top: f32 = 0, right: f32 = 0, bottom: f32 = 0 };
pub const Api = extern struct {
    struct_size: u32 = @sizeOf(Api),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    register_image: ?*const fn (?*anyopaque, *const Image, *u64) callconv(.c) i32 = null,
    remove_image: ?*const fn (?*anyopaque, u64) callconv(.c) i32 = null,
    image: ?*const fn (?*anyopaque, *const Draw) callconv(.c) i32 = null,
    nine_slice: ?*const fn (?*anyopaque, *const Draw, Insets, Insets) callconv(.c) i32 = null,
    interact: ?*const fn (?*anyopaque, u64, ui.Rect, u32, *u32) callconv(.c) i32 = null,
    get_glyph: ?*const fn (?*anyopaque, u64, u32, *ui.Glyph) callconv(.c) i32 = null,
    register_font: ?*const fn (?*anyopaque, *const ui.Font, *u64) callconv(.c) i32 = null,
    remove_font: ?*const fn (?*anyopaque, u64) callconv(.c) i32 = null,
    pub fn valid(self: *const Api) bool {
        return self.struct_size >= @sizeOf(Api) and self.abi_version == 1 and self.register_image != null and
            self.remove_image != null and self.image != null and self.nine_slice != null and self.interact != null and self.get_glyph != null and self.register_font != null and self.remove_font != null;
    }
    pub fn registerBakedFont(self: *const Api, bytes: []const u8, scratch: []ui.Glyph, out: *u64) i32 {
        if (!self.valid()) return -2;
        return ui.registerBakedFontData(self.context, self.register_font.?, 0, bytes, scratch, out);
    }
    pub fn registerImage(self: *const Api, texture: u64, width: u32, height: u32, rgba: []const u8, out: *u64) i32 {
        if (!self.valid() or rgba.len > 16 * 1024 * 1024) return -1;
        const desc = Image{ .texture_id = texture, .width = width, .height = height, .rgba = rgba.ptr, .rgba_length = rgba.len };
        return self.register_image.?(self.context, &desc, out);
    }
    pub fn drawImage(self: *const Api, desc: Draw) i32 {
        if (!self.valid()) return -2;
        return self.image.?(self.context, &desc);
    }
    pub fn nineSlice(self: *const Api, desc: Draw, source: Insets, destination: Insets) i32 {
        if (!self.valid()) return -2;
        return self.nine_slice.?(self.context, &desc, source, destination);
    }
    pub fn button(self: *const Api, id: u64, rect: ui.Rect, enabled: bool, out: *u32) i32 {
        if (!self.valid()) return -2;
        return self.interact.?(self.context, id, rect, @intFromBool(enabled), out);
    }
};
/// Uniform slots inside the supplied safe frame. Invalid/too-small areas have no mapping.
pub fn gridCell(area: ui.Rect, columns: u32, rows: u32, index: u32, gap: f32) ?ui.Rect {
    if (columns == 0 or rows == 0 or @as(u64, index) >= @as(u64, columns) * rows or
        !std.math.isFinite(gap) or gap < 0 or !std.math.isFinite(area.x) or !std.math.isFinite(area.y) or
        !std.math.isFinite(area.width) or !std.math.isFinite(area.height)) return null;
    const w = (area.width - gap * @as(f32, @floatFromInt(columns - 1))) / @as(f32, @floatFromInt(columns));
    const h = (area.height - gap * @as(f32, @floatFromInt(rows - 1))) / @as(f32, @floatFromInt(rows));
    if (w <= 0 or h <= 0) return null;
    return .{ .x = area.x + @as(f32, @floatFromInt(index % columns)) * (w + gap), .y = area.y + @as(f32, @floatFromInt(index / columns)) * (h + gap), .width = w, .height = h };
}
/// UTF-8 wrapping by supplied advances. Prefer spaces, split overlong words at scalar boundaries.
/// No shaping or locale punctuation rules; caller controls size, line height and clipping via UI V1.
/// Scratch may change on failure; the caller's original text is never modified.
pub fn wrapText(api: Api, font: u64, size: f32, pixel_size: f32, width: f32, text: []const u8, scratch: []u8) ![]const u8 {
    if (!api.valid() or !std.math.isFinite(size) or !std.math.isFinite(pixel_size) or !std.math.isFinite(width) or
        size <= 0 or pixel_size <= 0 or width <= 0 or text.len > 65536) return error.InvalidArgument;
    const view = std.unicode.Utf8View.init(text) catch return error.InvalidUtf8;
    var it = view.iterator();
    var length: usize = 0;
    var line_width: f32 = 0;
    var line_start: usize = 0;
    var space: ?usize = null;
    var space_width: f32 = 0;
    while (it.nextCodepointSlice()) |bytes| {
        const cp = std.unicode.utf8Decode(bytes) catch return error.InvalidUtf8;
        if (cp == 0) return error.InvalidArgument;
        if (cp == '\n') {
            if (length == scratch.len) return error.NoSpaceLeft;
            scratch[length] = '\n';
            length += 1;
            line_start = length;
            line_width = 0;
            space = null;
            continue;
        }
        var glyph = ui.Glyph{};
        if (api.get_glyph.?(api.context, font, cp, &glyph) != 0) return error.MissingGlyph;
        const advance = glyph.advance * size / pixel_size;
        if (!std.math.isFinite(advance) or advance < 0 or advance > width) return error.TooNarrow;
        if (line_width + advance > width and length > line_start) {
            if (cp == ' ') {
                if (length == scratch.len) return error.NoSpaceLeft;
                scratch[length] = '\n';
                length += 1;
                line_start = length;
                line_width = 0;
                space = null;
                continue;
            }
            if (space) |at| {
                scratch[at] = '\n';
                line_start = at + 1;
                line_width -= space_width;
                space = null;
            }
            if (line_width + advance > width) {
                if (length == scratch.len) return error.NoSpaceLeft;
                scratch[length] = '\n';
                length += 1;
                line_start = length;
                line_width = 0;
                space = null;
            }
        }
        if (bytes.len > scratch.len - length) return error.NoSpaceLeft;
        @memcpy(scratch[length..][0..bytes.len], bytes);
        if (cp == ' ') {
            space = length;
            space_width = line_width + advance;
        }
        length += bytes.len;
        line_width += advance;
        if (length > 65536) return error.NoSpaceLeft;
    }
    return scratch[0..length];
}
test "safe-frame slot layout includes offsets and rejects invalid extents" {
    const area = ui.Rect{ .x = 30, .y = 90, .width = 300, .height = 100 };
    const cell = gridCell(area, 3, 1, 2, 15).?;
    try std.testing.expectEqual(@as(f32, 240), cell.x);
    try std.testing.expectEqual(@as(f32, 90), cell.width);
    try std.testing.expect(gridCell(area, 0, 1, 0, 0) == null);
    try std.testing.expect(gridCell(.{}, 2, 2, 0, 2) == null);
    try std.testing.expect(gridCell(area, 3, 1, 3, 0) == null);
}

fn metric(_: ?*anyopaque, _: u64, cp: u32, out: *ui.Glyph) callconv(.c) i32 {
    if (cp == 0x2603) return -2;
    out.* = .{ .codepoint = cp, .advance = 10 };
    return 0;
}
fn noRegister(_: ?*anyopaque, _: *const Image, _: *u64) callconv(.c) i32 {
    return -2;
}
fn noFont(_: ?*anyopaque, _: *const ui.Font, _: *u64) callconv(.c) i32 {
    return -2;
}
fn noRemove(_: ?*anyopaque, _: u64) callconv(.c) i32 {
    return -2;
}
fn noImage(_: ?*anyopaque, _: *const Draw) callconv(.c) i32 {
    return -2;
}
fn noNine(_: ?*anyopaque, _: *const Draw, _: Insets, _: Insets) callconv(.c) i32 {
    return -2;
}
fn noButton(_: ?*anyopaque, _: u64, _: ui.Rect, _: u32, _: *u32) callconv(.c) i32 {
    return -2;
}
test "UTF-8 wraps with supplied advances, missing glyphs and bounded scratch" {
    const api = Api{ .register_image = noRegister, .remove_image = noRemove, .image = noImage, .nine_slice = noNine, .interact = noButton, .get_glyph = metric, .register_font = noFont, .remove_font = noRemove };
    var scratch: [128]u8 = undefined;
    try std.testing.expectEqualStrings("aa\nbb", try wrapText(api, 1, 16, 16, 20, "aa bb", &scratch));
    try std.testing.expectEqualStrings("陰陽\n術式", try wrapText(api, 1, 24, 16, 30, "陰陽術式", &scratch));
    try std.testing.expectEqualStrings("a b\nccc", try wrapText(api, 1, 16, 16, 40, "a b ccc", &scratch));
    try std.testing.expectError(error.MissingGlyph, wrapText(api, 1, 16, 16, 30, "☃", &scratch));
    try std.testing.expectError(error.InvalidUtf8, wrapText(api, 1, 16, 16, 30, "\xff", &scratch));
    try std.testing.expectError(error.NoSpaceLeft, wrapText(api, 1, 16, 16, 30, "abc", scratch[0..1]));
    try std.testing.expectError(error.TooNarrow, wrapText(api, 1, 16, 16, 2, "a", &scratch));
}
