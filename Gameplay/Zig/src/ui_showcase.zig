//! SP-02 diagnostic screens, not final game art or progression. All composition is Zig.
const nx = @import("nexora");
const font_bytes = @embedFile("fixtures/ui_fixture.nxfont");
const Owner = struct {
    host: nx.GameplayHostV3,
    ui: nx.ui.Api,
    draw: nx.ui_draw.Api,
    display: nx.display.Api,
    font: u64 = 0,
    image: u64 = 0,
    page: u32 = 0,
    scale: f32 = 1,
    logged: u32 = 99,
};
fn self(p: ?*anyopaque) *Owner {
    return @ptrCast(@alignCast(p.?));
}
fn log(o: *Owner, text: []const u8) void {
    if (o.host.log) |f| f(o.host.context, 1, text.ptr, @intCast(text.len));
}
fn create(out: *?*anyopaque, host: *const nx.GameplayHostV3) callconv(.c) i32 {
    const services = nx.getServices(host) orelse return -2;
    const draw = nx.getUiDraw(services) orelse return -2;
    const display = nx.getDisplay(services) orelse return -2;
    var ui = nx.ui.Api{};
    if (services.get_ui_api == null or services.get_ui_api.?(services.context, &ui) != 0 or !ui.valid()) return -2;
    const alloc = host.allocate orelse return -2;
    if (host.deallocate == null) return -2;
    const memory = alloc(host.context, 1, @sizeOf(Owner), @alignOf(Owner)) orelse return -3;
    const o: *Owner = @ptrCast(@alignCast(memory));
    o.* = .{ .host = host.*, .ui = ui, .draw = draw, .display = display };
    out.* = memory;
    return 0;
}
fn start(p: ?*anyopaque) callconv(.c) i32 {
    const o = self(p);
    var glyphs: [256]nx.ui.Glyph = undefined;
    if (o.draw.registerBakedFont(font_bytes, &glyphs, &o.font) != 0) return -3;
    var pixels: [32 * 32 * 4]u8 = undefined;
    for (0..32) |y| for (0..32) |x| {
        const offset = (y * 32 + x) * 4;
        const border = x < 4 or y < 4 or x >= 28 or y >= 28;
        pixels[offset] = if (border) 240 else @intCast(32 + x * 5);
        pixels[offset + 1] = if (border) 200 else @intCast(32 + y * 5);
        pixels[offset + 2] = if (border) 120 else 96;
        pixels[offset + 3] = 255;
    };
    const image_result = o.draw.registerImage(0, 32, 32, &pixels, &o.image);
    if (image_result != 0) {
        _ = o.draw.remove_font.?(o.draw.context, o.font);
        o.font = 0;
        return image_result;
    }
    const aspect_result = o.display.aspect(16.0 / 9.0, 21.0 / 9.0);
    if (aspect_result != 0) {
        _ = o.draw.remove_image.?(o.draw.context, o.image);
        _ = o.draw.remove_font.?(o.draw.context, o.font);
        o.font = 0;
        o.image = 0;
    }
    return aspect_result;
}
fn panel(o: *Owner, r: nx.ui.Rect) !void {
    if (o.draw.nineSlice(.{ .image = o.image, .source = .{ .width = 32, .height = 32 }, .destination = r, .clip = r }, .{ .left = 4, .right = 4, .top = 4, .bottom = 4 }, .{ .left = 8, .right = 8, .top = 8, .bottom = 8 }) != 0) return error.Draw;
}
fn label(o: *Owner, r: nx.ui.Rect, text: []const u8, size: f32) !void {
    if (o.ui.label(o.font, size, r, text, 0xffffffff) != 0) return error.Text;
}
fn button(o: *Owner, id: u64, r: nx.ui.Rect, text: []const u8, size: f32) !u32 {
    var state: u32 = 0;
    if (o.draw.button(id, r, true, &state) != 0) return error.Input;
    if (state & nx.ui.cancelled != 0) o.page = 0;
    try panel(o, r);
    if (state & nx.ui.focused != 0) {
        if (o.ui.panel.?(o.ui.context, .{ .x = r.x, .y = r.y, .width = r.width, .height = 3 }, 0xff00ffff) != 0) return error.Draw;
    }
    try label(o, .{ .x = r.x + 8, .y = r.y, .width = @max(0, r.width - 16), .height = r.height }, text, size);
    return state;
}
fn render(o: *Owner) !void {
    const s = o.display.state() orelse return error.Display;
    if (s.pixel_width == 0 or s.pixel_height == 0) return;
    const v = s.viewport_logical;
    const unit = @min(v.width / 960, v.height / 540) * o.scale;
    if (o.ui.begin_frame.?(o.ui.context, s.logical_width, s.logical_height) != 0) return error.Frame;
    const titles = [_][]const u8{ "陰陽 生存", "選擇", "整備", "字幕", "設定" };
    if (o.logged != o.page) {
        log(o, titles[o.page]);
        o.logged = o.page;
    }
    try label(o, .{ .x = v.x + 24 * unit, .y = v.y + 12 * unit, .width = v.width - 48 * unit, .height = 56 * unit }, titles[o.page], 28 * unit);
    const area = nx.ui.Rect{ .x = v.x + 24 * unit, .y = v.y + 86 * unit, .width = v.width - 48 * unit, .height = v.height - 160 * unit };
    if (o.page == 0) {
        // Four skills, two companions and three charms. These are diagnostic atlas regions.
        for (0..9) |index| {
            const slot = nx.ui_draw.gridCell(.{ .x = area.x, .y = area.y, .width = area.width, .height = 70 * unit }, 9, 1, @intCast(index), 4 * unit) orelse return error.Layout;
            try panel(o, slot);
            const region = nx.ui.Rect{ .x = @floatFromInt((index % 2) * 16), .y = @floatFromInt((index / 2 % 2) * 16), .width = 16, .height = 16 };
            const icon = nx.ui.Rect{ .x = slot.x + 8, .y = slot.y + 8, .width = @max(0, slot.width - 16), .height = slot.height - 16 };
            if (o.draw.drawImage(.{ .image = o.image, .source = region, .destination = icon, .clip = slot }) != 0) return error.Draw;
        }
        try label(o, .{ .x = area.x, .y = area.y + 82 * unit, .width = area.width, .height = 52 * unit }, "術 術 術 術 式神 式神 御守 御守 御守", 16 * unit);
    } else if (o.page == 1 or o.page == 2) {
        const labels = [_][]const u8{ "風雷", "火水", "光影" };
        for (labels, 0..) |text, index| {
            const card = nx.ui_draw.gridCell(area, 3, 1, @intCast(index), 12 * unit) orelse return error.Layout;
            const state = try button(o, 100 + index, card, text, 24 * unit);
            if (state & nx.ui.activated != 0) log(o, "SP-02 choice activated");
            if (state & nx.ui.cancelled != 0) o.page = 0;
        }
    } else if (o.page == 3) {
        var scratch: [512]u8 = undefined;
        const wrapped = try nx.ui_draw.wrapText(o.draw, o.font, 24 * unit, 32, area.width, "紙框圖像測試：這是換行及繁中文字型驗證，並非正式遊戲美術。", &scratch);
        try panel(o, area);
        try label(o, area, wrapped, 24 * unit);
    } else {
        const r = nx.ui_draw.gridCell(area, 1, 3, 0, 12 * unit) orelse return error.Layout;
        if ((try button(o, 200, r, "設定 100 150", 24 * unit)) & nx.ui.activated != 0) {
            o.scale = if (o.scale == 1) 1.5 else 1;
            log(o, "SP-02 UI scale toggled");
        }
    }
    const footer = nx.ui.Rect{ .x = v.x + v.width - 130 * unit, .y = v.y + v.height - 44 * unit, .width = 120 * unit, .height = 36 * unit };
    const state = try button(o, 1, footer, "確認", 20 * unit);
    if (state & nx.ui.activated != 0) o.page = (o.page + 1) % 5;
    if (state & nx.ui.cancelled != 0) o.page = 0;
    if (o.ui.end_frame.?(o.ui.context) != 0) return error.Frame;
}
fn update(p: ?*anyopaque, _: f64) callconv(.c) i32 {
    render(self(p)) catch return -3;
    return 0;
}
fn stop(p: ?*anyopaque) callconv(.c) void {
    const o = self(p);
    if (o.font != 0) {
        _ = o.draw.remove_font.?(o.draw.context, o.font);
        o.font = 0;
    }
    if (o.image != 0) {
        _ = o.draw.remove_image.?(o.draw.context, o.image);
        o.image = 0;
    }
}
fn destroy(p: ?*anyopaque) callconv(.c) void {
    const o = self(p);
    const host = o.host;
    host.deallocate.?(host.context, 1, o, @sizeOf(Owner), @alignOf(Owner));
}
export fn NexoraGameModuleLoad(abi: u32, module: ?*nx.GameModuleV3) i32 {
    const out = module orelse return -1;
    if (abi != 3 or out.struct_size < @sizeOf(nx.GameModuleV3)) return -2;
    out.* = .{ .struct_size = @sizeOf(nx.GameModuleV3), .abi_version = 3, .capabilities = 0, .module_state = null, .fixed_update = null, .save_state = null, .load_state = null, .create = create, .on_start = start, .update = update, .on_stop = stop, .destroy = destroy };
    return 0;
}
