//! Independent Zig-only consumer for SP-01, usable in the standalone Player.
const nx = @import("nexora");
const allow_resize: u32 = 1; // Set to 0 for the fixed-window native acceptance variant.
const Owner = struct {
    host: nx.GameplayHostV3,
    display: nx.display.Api,
    ui: nx.ui.Api,
    font: u64 = 0,
    frames: u32 = 0,
    native_requests: bool = false,
    ui_scale: f32 = 1,
    transition: u32 = 0,
    verified: bool = false,
};
fn owner(p: ?*anyopaque) *Owner {
    return @ptrCast(@alignCast(p.?));
}
fn create(output: *?*anyopaque, host: *const nx.GameplayHostV3) callconv(.c) i32 {
    const services = nx.getServices(host) orelse return -2;
    const display = nx.getDisplay(services) orelse return -2;
    var ui = nx.ui.Api{};
    if (services.get_ui_api.?(services.context, &ui) != 0) return -2;
    const allocate = host.allocate orelse return -2;
    if (host.deallocate == null) return -2;
    const memory = allocate(host.context, 1, @sizeOf(Owner), @alignOf(Owner)) orelse return -3;
    const o: *Owner = @ptrCast(@alignCast(memory));
    o.* = .{ .host = host.*, .display = display, .ui = ui };
    output.* = memory;
    return 0;
}
fn start(p: ?*anyopaque) callconv(.c) i32 {
    const o = owner(p);
    const state = o.display.state() orelse return -3;
    if (state.pixel_width == 0 or state.pixel_height == 0 or o.display.aspect(16.0 / 9.0, 21.0 / 9.0) != 0) return -3;
    // Small explicit raster fixture, not a production font or image HUD.
    const pixels = [_]u8{ 255, 255, 255, 255 };
    const glyph = nx.ui.Glyph{ .codepoint = 'A', .advance = 12, .width = 12, .height = 16, .u1 = 1, .v1 = 1 };
    const font = nx.ui.Font{ .width = 1, .height = 1, .texture_id = 77, .pixel_size = 16, .line_height = 16, .rgba = &pixels, .rgba_length = 4, .glyphs = @ptrCast(&glyph), .glyph_count = 1 };
    return o.ui.register_font.?(o.ui.context, &font, &o.font);
}
fn update(p: ?*anyopaque, _: f64) callconv(.c) i32 {
    const o = owner(p);
    const s = o.display.state() orelse return -3;
    if (s.pixel_width == 0 or s.pixel_height == 0) return 0;
    const c = nx.display.Camera{ .rotation = 0.3 };
    const pixel = nx.display.worldToPixel(s, c, .{ .x = 2, .y = 1 }) orelse return -3;
    const restored = nx.display.pixelToWorld(s, c, pixel) orelse return -3;
    if (@abs(restored.x - 2) > 0.001 or @abs(restored.y - 1) > 0.001) return -3;
    const logical = nx.display.pixelToLogical(s, pixel) orelse return -3;
    if (o.frames == 0) {
        const result = o.display.request(.{ .mode = 1, .resizable = allow_resize });
        if (result != 0 and result != -2) return result;
        o.native_requests = result == 0;
        if (o.native_requests) o.transition = 1;
    } else if (o.transition == 1 and s.mode == 1) {
        const result = o.display.request(.{ .mode = 0, .resizable = allow_resize });
        if (result != 0) return result;
        o.transition = 2;
    } else if (o.transition == 2 and s.mode == 0) {
        o.transition = 3;
    }
    if (o.frames > 600 and o.native_requests and o.transition != 3) return -3;
    if (o.ui.begin_frame.?(o.ui.context, s.logical_width, s.logical_height) != 0) return -3;
    if (o.ui.panel.?(o.ui.context, .{ .x = @floatCast(logical.x - 4), .y = @floatCast(logical.y - 4), .width = 8, .height = 8 }, 0xff00ffff) != 0) return -3;
    var state: u32 = 0;
    if (o.ui.menuButton(1, .{ .x = s.viewport_logical.x + 16, .y = s.viewport_logical.y + 16, .width = 96 * o.ui_scale, .height = 40 * o.ui_scale }, "A", o.font, 16 * o.ui_scale, true, &state) != 0) return -3;
    if ((state & nx.ui.activated) != 0) {
        o.ui_scale = if (o.ui_scale == 1) 1.5 else 1;
        const text = "SP-01 display button activated; UI scale toggled";
        if (o.host.log) |log| log(o.host.context, 1, text.ptr, text.len);
    }
    if (o.ui.end_frame.?(o.ui.context) != 0) return -3;
    if (o.frames >= 2 and !o.verified and (!o.native_requests or o.transition == 3)) {
        o.verified = true;
        const text = "SP-01 Zig display consumer verified";
        if (o.host.log) |log| log(o.host.context, 1, text.ptr, text.len);
    }
    o.frames += 1;
    return 0;
}
fn stop(_: ?*anyopaque) callconv(.c) void {}
fn destroy(p: ?*anyopaque) callconv(.c) void {
    const o = owner(p);
    const host = o.host;
    host.deallocate.?(host.context, 1, o, @sizeOf(Owner), @alignOf(Owner));
}
export fn NexoraGameModuleLoad(abi: u32, module: ?*nx.GameModuleV3) i32 {
    const out = module orelse return -1;
    if (abi != 3 or out.struct_size < @sizeOf(nx.GameModuleV3)) return -2;
    out.* = .{ .struct_size = @sizeOf(nx.GameModuleV3), .abi_version = 3, .capabilities = 0, .module_state = null, .create = create, .on_start = start, .fixed_update = null, .update = update, .on_stop = stop, .destroy = destroy, .save_state = null, .load_state = null };
    return 0;
}
