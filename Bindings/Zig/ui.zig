//! Optional host-owned HUD, menu and UTF-8 text drawing. Font handles enumerate supplied atlases.
pub const Rect = extern struct { x: f32 = 0, y: f32 = 0, width: f32 = 0, height: f32 = 0 };
pub const hovered: u32 = 1;
pub const focused: u32 = 2;
pub const activated: u32 = 4;
pub const cancelled: u32 = 8;
pub const Glyph = extern struct {
    codepoint: u32 = 0,
    advance: f32 = 0,
    x: f32 = 0,
    y: f32 = 0,
    width: f32 = 0,
    height: f32 = 0,
    u0: f32 = 0,
    v0: f32 = 0,
    u1: f32 = 0,
    v1: f32 = 0,
};
pub const Font = extern struct {
    struct_size: u32 = @sizeOf(Font),
    width: u32 = 0,
    height: u32 = 0,
    reserved: u32 = 0,
    texture_id: u64 = 0,
    pixel_size: f32 = 0,
    line_height: f32 = 0,
    rgba: ?[*]const u8 = null,
    rgba_length: u64 = 0,
    glyphs: ?[*]const Glyph = null,
    glyph_count: u32 = 0,
    reserved2: u32 = 0,
};
pub const Api = extern struct {
    struct_size: u32 = @sizeOf(Api),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    begin_frame: ?*const fn (?*anyopaque, f32, f32) callconv(.c) i32 = null,
    panel: ?*const fn (?*anyopaque, Rect, u32) callconv(.c) i32 = null,
    text: ?*const fn (?*anyopaque, u64, f32, Rect, ?[*]const u8, u32, u32) callconv(.c) i32 = null,
    button: ?*const fn (?*anyopaque, u64, Rect, ?[*]const u8, u32, u64, f32, u32, *u32) callconv(.c) i32 = null,
    end_frame: ?*const fn (?*anyopaque) callconv(.c) i32 = null,
    get_font: ?*const fn (?*anyopaque, u32, *u64) callconv(.c) i32 = null,
    has_glyph: ?*const fn (?*anyopaque, u64, u32, *u32) callconv(.c) i32 = null,

    register_font: ?*const fn (?*anyopaque, *const Font, *u64) callconv(.c) i32 = null,

    pub fn valid(self: *const Api) bool {
        return self.struct_size >= @sizeOf(Api) and self.abi_version == 1 and
            self.begin_frame != null and self.panel != null and self.text != null and
            self.button != null and self.end_frame != null and self.get_font != null and self.has_glyph != null and self.register_font != null;
    }
    /// Upload a real pre-rasterized atlas, e.g. RGBA bytes from @embedFile plus cooked glyph metrics.
    /// The caller reserves texture_id outside Sprite generations; buffers can expire after return.
    pub fn registerFont(self: *const Api, texture_id: u64, width: u32, height: u32, pixel_size: f32, line_height: f32, rgba: []const u8, glyphs: []const Glyph, handle: *u64) i32 {
        if (!self.valid() or glyphs.len > 65536 or rgba.len > 16 * 1024 * 1024) return -1;
        const font = Font{ .texture_id = texture_id, .width = width, .height = height, .pixel_size = pixel_size, .line_height = line_height, .rgba = rgba.ptr, .rgba_length = rgba.len, .glyphs = glyphs.ptr, .glyph_count = @intCast(glyphs.len) };
        return self.register_font.?(self.context, &font, handle);
    }
    /// Register a BakeWindowsFont.ps1 NXFONT1 atlas, including @embedFile bytes.
    /// Scratch glyphs are caller-owned work storage; the host copies both buffers before return.
    /// Errors preserve handle; scratch may be overwritten after header/buffer admission.
    pub fn registerBakedFont(self: *const Api, texture_id: u64, bytes: []const u8, scratch: []Glyph, handle: *u64) i32 {
        if (!self.valid()) return -1;
        return registerBakedFontData(self.context, self.register_font.?, texture_id, bytes, scratch, handle);
    }
    pub fn label(self: *const Api, font: u64, size: f32, bounds: Rect, utf8: []const u8, rgba: u32) i32 {
        if (!self.valid() or utf8.len > 65536) return -1;
        return self.text.?(self.context, font, size, bounds, utf8.ptr, @intCast(utf8.len), rgba);
    }
    pub fn menuButton(self: *const Api, id: u64, bounds: Rect, utf8: []const u8, font: u64, size: f32, enabled: bool, state: *u32) i32 {
        if (!self.valid() or utf8.len > 65536) return -1;
        return self.button.?(self.context, id, bounds, utf8.ptr, @intCast(utf8.len), font, size, @intFromBool(enabled), state);
    }
};

fn bakedU32(bytes: []const u8, offset: usize) u32 {
    return @import("std").mem.readInt(u32, bytes[offset..][0..4], .little);
}
fn bakedFloat(bytes: []const u8, offset: usize) f32 {
    return @bitCast(bakedU32(bytes, offset));
}

/// Shared NXFONT1 decoder for the historical and releasable registration tables.
pub fn registerBakedFontData(context: ?*anyopaque, register: *const fn (?*anyopaque, *const Font, *u64) callconv(.c) i32, texture_id: u64, bytes: []const u8, scratch: []Glyph, handle: *u64) i32 {
    const std = @import("std");
    if (bytes.len < 28 or !std.mem.eql(u8, bytes[0..8], "NXFONT1\x00")) return -1;
    const width = bakedU32(bytes, 8);
    const height = bakedU32(bytes, 12);
    const count = bakedU32(bytes, 24);
    if (width == 0 or width > 4096 or height == 0 or height > 4096 or
        count == 0 or count > 65536 or count > scratch.len) return -1;
    const rgba_size = @as(u64, width) * height * 4;
    const rgba_start = 28 + @as(u64, count) * 40;
    if (rgba_size > 16 * 1024 * 1024 or rgba_start + rgba_size != bytes.len) return -1;
    for (scratch[0..count], 0..) |*glyph, index| {
        const offset = 28 + index * 40;
        glyph.* = .{ .codepoint = bakedU32(bytes, offset), .advance = bakedFloat(bytes, offset + 4), .x = bakedFloat(bytes, offset + 8), .y = bakedFloat(bytes, offset + 12), .width = bakedFloat(bytes, offset + 16), .height = bakedFloat(bytes, offset + 20), .u0 = bakedFloat(bytes, offset + 24), .v0 = bakedFloat(bytes, offset + 28), .u1 = bakedFloat(bytes, offset + 32), .v1 = bakedFloat(bytes, offset + 36) };
    }
    const desc = Font{ .texture_id = texture_id, .width = width, .height = height, .pixel_size = bakedFloat(bytes, 16), .line_height = bakedFloat(bytes, 20), .rgba = bytes[@intCast(rgba_start)..].ptr, .rgba_length = rgba_size, .glyphs = scratch.ptr, .glyph_count = count };
    return register(context, &desc, handle);
}
