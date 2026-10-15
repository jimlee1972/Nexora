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
    pub fn label(self: *const Api, font: u64, size: f32, bounds: Rect, utf8: []const u8, rgba: u32) i32 {
        if (!self.valid() or utf8.len > 65536) return -1;
        return self.text.?(self.context, font, size, bounds, utf8.ptr, @intCast(utf8.len), rgba);
    }
    pub fn menuButton(self: *const Api, id: u64, bounds: Rect, utf8: []const u8, font: u64, size: f32, enabled: bool, state: *u32) i32 {
        if (!self.valid() or utf8.len > 65536) return -1;
        return self.button.?(self.context, id, bounds, utf8.ptr, @intCast(utf8.len), font, size, @intFromBool(enabled), state);
    }
};
