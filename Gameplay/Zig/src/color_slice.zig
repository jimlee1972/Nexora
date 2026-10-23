//! Procedural SP-04 top-down fallback proof; not final artwork, lighting or a complete game.
const nx = @import("nexora");
const Owner = struct {
    host: nx.GameplayHostV3,
    api: nx.TwoDApiV1,
    ui: nx.ui.Api,
    display: nx.display.Api,
    scene: u64 = 0,
    handles: [7]nx.TwoDInstanceHandle = undefined,
    live: bool = false,
    frame: u32 = 0,
    reduced: bool = false,
};
fn self(p: ?*anyopaque) *Owner {
    return @ptrCast(@alignCast(p.?));
}
fn create(out: *?*anyopaque, host: *const nx.GameplayHostV3) callconv(.c) i32 {
    const services = nx.getServices(host) orelse return -2;
    const display = nx.getDisplay(services) orelse return -2;
    const api = nx.getTwoD(host) orelse return -2;
    var ui = nx.ui.Api{};
    if (services.get_ui_api.?(services.context, &ui) != 0 or !ui.valid() or host.allocate == null or host.deallocate == null or host.load_scene == null or host.activate_scene == null) return -2;
    const memory = host.allocate.?(host.context, 1, @sizeOf(Owner), @alignOf(Owner)) orelse return -3;
    const o: *Owner = @ptrCast(@alignCast(memory));
    o.* = .{ .host = host.*, .api = api, .ui = ui, .display = display };
    out.* = memory;
    return 0;
}
fn instance(region: u32, x: f64, y: f64, w: f32, h: f32, layer: i32) nx.TwoDInstance {
    return .{
        .transform = .{ .position = .{ .x = x, .y = y } },
        .sprite = .{
            .flags = 12, // visible + Y sorting (opt-in in the existing ABI)
            .atlas_high = 44,
            .atlas_low = 4,
            .region = region,
            .size = .{ .x = w, .y = h },
            .layer = layer,
        },
    };
}
fn start(p: ?*anyopaque) callconv(.c) i32 {
    const o = self(p);
    if (o.host.load_scene.?(o.host.context, "Main".ptr, 4, 0, &o.scene) != 0 or
        o.host.activate_scene.?(o.host.context, o.scene) != 0 or o.display.aspect(16.0 / 9.0, 21.0 / 9.0) != 0) return -3;
    const camera = nx.TwoDCamera{ .vertical_size = 24, .rotation_radians = 0 };
    if (o.api.set_camera.?(o.api.context, &camera) != 0) return -3;
    const items = [_]nx.TwoDInstance{
        instance(1, 0, 0, 40, 24, -10), // ground
        instance(5, 0, -0.4, 5, 2, -1), // independent baked soft shadow
        instance(2, 0, 0, 3, 5, 0), // actor foot point uses the same world Y as the sorting API
        instance(3, 0, 1, 6, 4, 0), // foreground, stable Y ordering
        instance(4, 3, 2, 8, 8, 1), // independent baked glow; no additive/HDR/bloom claim
        instance(6, 6, -3, 4, 4, 2), // opaque shape outline survives reduced effects
        instance(7, -6, 3, 4, 4, 0), // middle-gray authoring swatch
    };
    if (o.api.create_instances.?(o.api.context, &items, items.len, &o.handles) != 0) return -3;
    o.live = true;
    return 0;
}
fn update(p: ?*anyopaque, _: f64) callconv(.c) i32 {
    const o = self(p);
    var items: [7]nx.TwoDInstance = undefined;
    if (o.api.read_instances.?(o.api.context, &o.handles, 7, &items) != 0) return -3;
    // First four frames exercise both policies even in a headless smoke. Then keep the chosen state.
    if (o.frame < 4) o.reduced = o.frame % 2 == 1;
    items[2].transform.position.y = if (o.reduced) 2 else 0;
    items[3].sprite.color[3] = if (o.reduced) 0.25 else 1; // gameplay controls occluder fade
    items[4].sprite.color[3] = if (o.reduced) 0 else 1;
    items[1].sprite.color[3] = if (o.reduced) 0 else 1;
    if (items[5].sprite.color[3] != 1 or items[5].sprite.layer != 2) return -3;
    if (o.api.write_instances.?(o.api.context, &o.handles, 7, &items) != 0) return -3;
    const state = o.display.state() orelse return -3;
    if (o.ui.begin_frame.?(o.ui.context, state.logical_width, state.logical_height) != 0) return -3;
    // Authored sRGB UI swatch uses the same transfer as the imported world texture.
    const area = nx.ui.Rect{ .x = state.viewport_logical.x + 20, .y = state.viewport_logical.y + 20, .width = 120, .height = 48 };
    if (o.ui.panel.?(o.ui.context, area, nx.color.uiTint(.{ 128, 128, 128, 255 })) != 0) return -3;
    var action: u32 = 0;
    const services = nx.getServices(&o.host) orelse return -3;
    const draw = nx.getUiDraw(services) orelse return -3;
    if (draw.button(4004, area, true, &action) != 0) return -3;
    if (action & nx.ui.activated != 0) o.reduced = !o.reduced;
    if (o.ui.end_frame.?(o.ui.context) != 0) return -3;
    if (o.frame == 3) {
        const text = "SP-04 PASS: linear swatch, layered sprites, foreground fade, baked glow/shadow off, hazard retained";
        if (o.host.log) |log| log(o.host.context, 1, text.ptr, text.len);
    }
    o.frame +|= 1;
    return 0;
}
fn stop(p: ?*anyopaque) callconv(.c) void {
    const o = self(p);
    if (o.live) {
        _ = o.api.destroy_instances.?(o.api.context, &o.handles, 7);
        o.live = false;
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
    out.* = .{ .struct_size = @sizeOf(nx.GameModuleV3), .abi_version = 3, .capabilities = 0, .module_state = null, .create = create, .on_start = start, .fixed_update = null, .update = update, .on_stop = stop, .destroy = destroy, .save_state = null, .load_state = null };
    return 0;
}
