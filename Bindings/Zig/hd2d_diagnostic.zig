//! Reusable DEV ONLY engine diagnostic. Controls, motion, camera and effects policy are Zig.
//! This is not production art, a combat scene, a normal-map renderer or a performance promise.
const std = @import("std");
const nx = @import("nexora.zig");
const ex = @import("twod_effects.zig");
const base_id = [2]u64{ 0x48443244, 1 };
const mask_id = [2]u64{ 0x48443244, 2 };
const base_bytes = @embedFile("diagnostic/base.nx2atlas");
const mask_bytes = @embedFile("diagnostic/mask.nx2atlas");
const link_bytes = @embedFile("diagnostic/links.nx2emission");
const font_bytes = @embedFile("diagnostic/ui_fixture.nxfont");
pub const Error = ex.Error;
fn check(code: i32) Error!void {
    return switch (code) {
        0 => {},
        -1 => error.InvalidArgument,
        -2 => error.Unsupported,
        else => error.Lifecycle,
    };
}
pub const Options = struct { automated: bool = false, capture_holds: bool = false };
pub const State = struct {
    host: nx.GameplayHostV3 = undefined,
    twod: nx.TwoDApiV1 = undefined,
    display: nx.display.Api = undefined,
    ui: nx.ui.Api = undefined,
    draw: nx.ui_draw.Api = undefined,
    content: nx.content.Api = undefined,
    assets: nx.assets.Api = undefined,
    effects: ?ex.Api = null,
    features: u32 = 0,
    handles: [10]nx.TwoDInstanceHandle = undefined,
    styles: [10]ex.Sprite = undefined,
    font: u64 = 0,
    base_registered: bool = false,
    mask_registered: bool = false,
    live: bool = false,
    started: bool = false,
    previous_camera: nx.TwoDCamera = .{},
    automated: bool = false,
    capture_holds: bool = false,
    frame: u32 = 0,
    last_phase: u32 = std.math.maxInt(u32),
    phase_mask: u32 = 0,
    timing_seen: u32 = 0,
    reloads: u32 = 0,
    time: f64 = 0,
    enabled: bool = true,
    strength_index: usize = 0,
    outlines: bool = true,
    map: bool = false,
    rotate: bool = false,
    mirror: bool = false,
    large_ui: bool = false,
    wide_camera: bool = false,
    paused: bool = false,

    pub fn start(self: *State, host: *const nx.GameplayHostV3, options: Options) Error!void {
        const services = nx.getServices(host) orelse return error.Unsupported;
        var ui = nx.ui.Api{};
        var content = nx.content.Api{};
        if (services.get_ui_api == null or services.get_content_api == null) return error.Unsupported;
        try check(services.get_ui_api.?(services.context, &ui));
        try check(services.get_content_api.?(services.context, &content));
        self.* = .{
            .host = host.*,
            .twod = nx.getTwoD(host) orelse return error.Unsupported,
            .display = nx.getDisplay(services) orelse return error.Unsupported,
            .ui = ui,
            .content = content,
            .draw = nx.getUiDraw(services) orelse return error.Unsupported,
            .assets = nx.getAssets(services) orelse return error.Unsupported,
            .effects = nx.getTwoDEffects(services),
            .automated = options.automated,
            .capture_holds = options.capture_holds,
        };
        self.started = true;
        errdefer self.stop() catch {};
        try check(self.twod.get_camera.?(self.twod.context, &self.previous_camera));
        if (self.effects) |api| self.features = (try api.capabilities()).features;
        try check(self.display.aspect(16.0 / 9.0, 21.0 / 9.0));
        var glyphs: [256]nx.ui.Glyph = undefined;
        try check(self.draw.registerBakedFont(font_bytes, &glyphs, &self.font));
        try self.spawn();
        self.log("HD2D DEV ONLY START Zig controls and test assets");
    }
    fn log(self: *State, text: []const u8) void {
        if (self.host.log) |callback| callback(self.host.context, 1, text.ptr, @intCast(text.len));
    }
    fn item(region: u32, x: f64, y: f64, width: f32, height: f32, layer: i32) nx.TwoDInstance {
        return .{ .transform = .{ .position = .{ .x = x, .y = y } }, .sprite = .{ .atlas_high = base_id[0], .atlas_low = base_id[1], .region = region, .flags = 12, .size = .{ .x = width, .y = height }, .layer = layer } };
    }
    fn spawn(self: *State) Error!void {
        var high: u64 = 0;
        var low: u64 = 0;
        try check(self.content.registerAtlas(base_bytes, &high, &low));
        self.base_registered = true;
        if (high != base_id[0] or low != base_id[1]) return error.Lifecycle;
        try check(self.content.registerAtlas(mask_bytes, &high, &low));
        self.mask_registered = true;
        if (high != mask_id[0] or low != mask_id[1]) return error.Lifecycle;
        const instances = [_]nx.TwoDInstance{
            item(1, 0, 0, 48, 28, -20),    item(3, -4, -2, 2, 3, 0),
            item(4, 0, 0, 3, 4, 0),        item(5, 5, 2, 3, 3, 0),
            item(2, -7, 4, 3, 3, 1),       item(6, 0, -4, 5, 2, 10),
            item(2, 0, 2, 3, 0.05, 20),    item(2, 0, -2, 3, 0.05, 20),
            item(2, -1.5, 0, 0.05, 4, 20), item(2, 1.5, 0, 0.05, 4, 20),
        };
        try check(self.twod.create_instances.?(self.twod.context, &instances, instances.len, &self.handles));
        self.live = true;
        if (self.features & 1 != 0) {
            const api = self.effects orelse return error.Unsupported;
            const links = try ex.EmissionAsset.parse(link_bytes);
            for (&self.styles, self.handles, 0..) |*style, handle, index| {
                style.* = api.poolSprite(handle) catch |err| switch (err) {
                    error.Unsupported => {
                        self.features = 0;
                        break;
                    },
                    else => return err,
                };
                style.groups = 3;
                if (index >= 4) {
                    style.receive_light = 0;
                    style.receive_shadow = 0;
                }
                if (self.features & 4 != 0 and (index == 1 or index == 3)) {
                    style.* = try links.apply(style.*, if (index == 1) 3 else 5, if (index == 1) .{ 1, 0.35, 0.03 } else .{ 0.05, 0.5, 1 }, if (index == 1) 3 else 4);
                }
            }
        }
    }
    fn clearContent(self: *State) Error!void {
        if (self.effects) |api| {
            try api.submit(.{}, &.{}, &.{}, &.{});
            api.debugView(0, 0) catch |err| if (err != error.Unsupported) return err;
        }
        if (self.live) {
            try check(self.twod.destroy_instances.?(self.twod.context, &self.handles, self.handles.len));
            self.live = false;
        }
        if (self.mask_registered) {
            try check(self.assets.remove(mask_id[0], mask_id[1]));
            self.mask_registered = false;
        }
        if (self.base_registered) {
            try check(self.assets.remove(base_id[0], base_id[1]));
            self.base_registered = false;
        }
    }
    pub fn reload(self: *State) Error!void {
        try self.clearContent();
        try self.spawn();
        self.reloads +|= 1;
        self.log("HD2D RELOAD PASS new pool identities and mask generations");
    }
    fn automate(self: *State) Error!void {
        const phase = @min(self.frame / 12, 11);
        if (phase == self.last_phase) return;
        self.last_phase = phase;
        self.phase_mask |= @as(u32, 1) << @intCast(phase);
        self.enabled = phase != 3 and phase != 11;
        self.strength_index = if (phase == 1) 1 else if (phase == 2 or phase == 9) 2 else 0;
        self.rotate = phase == 4 or phase == 9;
        self.mirror = phase == 5 or phase == 9;
        self.map = phase == 6;
        self.outlines = phase != 7;
        self.large_ui = phase == 9;
        self.wide_camera = phase == 9;
        if (phase == 8 or phase == 10) try self.reload();
        var buffer: [128]u8 = undefined;
        const message = std.fmt.bufPrint(&buffer, "HD2D PHASE {d} enabled={d} strength={d} features={d}", .{ phase, @intFromBool(self.enabled), self.strength_index, self.features }) catch return error.Lifecycle;
        self.log(message);
    }
    pub fn update(self: *State, delta: f64) Error!void {
        if (!self.started or !self.live or !std.math.isFinite(delta) or delta < 0) return error.Lifecycle;
        if (self.automated) try self.automate();
        if (!self.paused) self.time += @min(delta, 0.1);
        const display = self.display.state() orelse return error.Lifecycle;
        if (display.pixel_width == 0 or display.pixel_height == 0) return;
        const camera = nx.TwoDCamera{ .vertical_size = if (self.wide_camera) 28 else 24, .rotation_radians = if (self.rotate) 0.35 else 0 };
        try check(self.twod.set_camera.?(self.twod.context, &camera));
        const x: f32 = @floatCast(std.math.sin(self.time * 0.7) * 1.5);
        const y: f32 = @floatCast(std.math.cos(self.time * 0.9) * 0.5);
        const lantern_x: f32 = @floatCast(-4 + std.math.sin(self.time) * 2);
        const lantern_y: f32 = @floatCast(-2 + std.math.cos(self.time * 0.6));
        var instances: [10]nx.TwoDInstance = undefined;
        try check(self.twod.read_instances.?(self.twod.context, &self.handles, self.handles.len, &instances));
        instances[1].transform.position.x = lantern_x;
        instances[1].transform.position.y = lantern_y;
        instances[1].sprite.flags = 12 | @as(u32, if (self.mirror) 3 else 0);
        instances[2].transform.position.x = x;
        instances[2].transform.position.y = y;
        instances[2].sprite.color[3] = 0.8;
        const positions = [_][2]f32{ .{ x, y + 2 }, .{ x, y - 2 }, .{ x - 1.5, y }, .{ x + 1.5, y } };
        for (instances[6..10], positions) |*instance, position| {
            instance.transform.position.x = position[0];
            instance.transform.position.y = position[1];
            instance.sprite.color = .{ 0, 1, 1, if (self.outlines) 1 else 0 };
        }
        // The essential red outline stays opaque independently of all decoration settings.
        if (instances[5].sprite.color[3] != 1) return error.Lifecycle;
        try check(self.twod.write_instances.?(self.twod.context, &self.handles, self.handles.len, &instances));
        if (self.effects) |api| {
            var lights = [_]ex.Light{
                .{ .id = 1, .position = .{ .x = lantern_x, .y = lantern_y }, .color = .{ 1, 0.35, 0.03 }, .intensity = 4, .radius = 6, .softness = 0.6, .bias = 0.02, .groups = 1 },
                .{ .id = 2, .position = .{ .x = -3, .y = 2 }, .color = .{ 1, 0.05, 0.01 }, .intensity = 3, .radius = 6, .softness = 0.25, .bias = 0.02, .groups = 1 },
                .{ .id = 3, .position = .{ .x = 3, .y = 1 }, .color = .{ 0.03, 0.25, 1 }, .intensity = 3, .radius = 7, .softness = 0.75, .bias = 0.02, .groups = 2 },
            };
            for (&lights) |*light| light.shadow = @intFromBool(self.features & 2 != 0);
            var occluder = ex.Occluder{ .id = 1, .groups = 3, .vertex_count = 4 };
            occluder.vertices[0] = .{ .x = x - 1.5, .y = y - 2 };
            occluder.vertices[1] = .{ .x = x + 1.5, .y = y - 2 };
            occluder.vertices[2] = .{ .x = x + 1.5, .y = y + 2 };
            occluder.vertices[3] = .{ .x = x - 1.5, .y = y + 2 };
            const occluders = [_]ex.Occluder{occluder};
            const strengths = [_]f32{ 1, 0.75, 0.5 };
            try api.submit(.{ .enabled = @intFromBool(self.enabled and self.features & 1 != 0), .ambient = .{ 0.15, 0.15, 0.18 }, .strength = strengths[self.strength_index], .bloom = if (self.features & 4 != 0) .{ .intensity = 0.6, .radius = 16, .levels = 5 } else .{} }, &lights, if (self.features & 2 != 0) &occluders else &.{}, if (self.features & 1 != 0) &self.styles else &.{});
            api.debugView(@intFromBool(self.map and self.enabled and self.features & 2 != 0), 0) catch |err| {
                if (err != error.Unsupported) return err;
                self.map = false;
            };
            if (api.diagnostics()) |stats| {
                self.timing_seen |= stats.timing_mask;
                if (self.frame % 24 == 0) {
                    var buffer: [192]u8 = undefined;
                    const message = std.fmt.bufPrint(&buffer, "HD2D GPU frame={d} revision={d} mask={d} source={d} status={d} peak_bytes={d}", .{ stats.completed_frame, stats.effects_revision, stats.timing_mask, stats.timing_source, stats.timing_status, stats.peak_effect_bytes }) catch return error.Lifecycle;
                    self.log(message);
                }
            } else |err| if (err != error.Unsupported) return err;
        }
        if (self.automated and self.capture_holds and self.frame == 18) {
            var ready: [128]u8 = undefined;
            const message = std.fmt.bufPrint(&ready, "HD2D UI READY dpi={d} width={d} height={d}", .{ display.dpi_scale, display.pixel_width, display.pixel_height }) catch return error.Lifecycle;
            self.log(message);
        }
        if (self.automated and self.capture_holds and (self.frame == 31 or self.frame == 81)) {
            self.log(if (self.frame == 31) "HD2D CAPTURE scene" else "HD2D CAPTURE map");
            // Test-only capture window: completed prior GPU work continues while the
            // automation harness reads the actual native window. Never a FPS workload.
            std.Thread.sleep(1500 * std.time.ns_per_ms);
        }
        try self.drawUi(display);
        self.frame +|= 1;
    }
    fn drawUi(self: *State, display: nx.display.State) Error!void {
        const v = display.viewport_logical;
        const scale: f32 = if (self.large_ui) 1.5 else 1;
        try check(self.ui.begin_frame.?(self.ui.context, display.logical_width, display.logical_height));
        if (v.width < 80 or v.height < 80) {
            try check(self.ui.end_frame.?(self.ui.context));
            return;
        }
        const top = nx.ui.Rect{ .x = v.x + 8, .y = v.y + 8, .width = @max(1, v.width - 16), .height = 170 * scale };
        try check(self.ui.panel.?(self.ui.context, top, 0xd9101010));
        try check(self.ui.label(self.font, 20 * scale, .{ .x = top.x + 8, .y = top.y, .width = top.width - 16, .height = 28 * scale }, "DEV ONLY HD2D ZIG", 0xffffffff));
        var buffer: [128]u8 = undefined;
        const strengths = [_]u32{ 100, 75, 50 };
        const status = std.fmt.bufPrint(&buffer, "Effects {s} Strength {d} {s}", .{ if (self.enabled) "ON" else "OFF", strengths[self.strength_index], if (self.features & 1 != 0) "NATIVE" else "FALLBACK" }) catch return error.Lifecycle;
        try check(self.ui.label(self.font, 14 * scale, .{ .x = top.x + 8, .y = top.y + 28 * scale, .width = top.width - 16, .height = 24 * scale }, status, 0xffffffff));
        const labels = [_][]const u8{ "Effects", "Strength", "Outline", "Map", "Camera", "Pause", "UIScale", "Reload" };
        const width = @max(1, (top.width - 28) / 4);
        for (labels, 0..) |text, index| {
            const area = nx.ui.Rect{ .x = top.x + 8 + @as(f32, @floatFromInt(index % 4)) * (width + 4), .y = top.y + 54 * scale + @as(f32, @floatFromInt(index / 4)) * 34 * scale, .width = width, .height = 30 * scale };
            var action: u32 = 0;
            try check(self.ui.menuButton(0x484400 + index, area, text, self.font, 14 * scale, index != 3 or self.features & 2 != 0, &action));
            if (action & nx.ui.activated != 0) {
                var before: nx.TwoDCamera = .{};
                try check(self.twod.get_camera.?(self.twod.context, &before));
                switch (index) {
                    0 => self.enabled = !self.enabled,
                    1 => self.strength_index = (self.strength_index + 1) % 3,
                    2 => self.outlines = !self.outlines,
                    3 => self.map = !self.map,
                    4 => {
                        self.rotate = !self.rotate;
                        self.mirror = self.rotate;
                        self.wide_camera = self.rotate;
                    },
                    5 => self.paused = !self.paused,
                    6 => {
                        self.large_ui = !self.large_ui;
                    },
                    7 => try self.reload(),
                    else => unreachable,
                }
                var after: nx.TwoDCamera = .{};
                try check(self.twod.get_camera.?(self.twod.context, &after));
                if (index == 6 and !std.meta.eql(before, after)) return error.Lifecycle;
                var action_buffer: [96]u8 = undefined;
                const message = std.fmt.bufPrint(&action_buffer, "HD2D UI ACTION {d} camera_unchanged={d}", .{ index, @intFromBool(std.meta.eql(before, after)) }) catch return error.Lifecycle;
                self.log(message);
            }
        }
        const text = if (self.map) "Native shadow map slot 0" else "White HUD and red hazard stay visible";
        try check(self.ui.label(self.font, 12 * scale, .{ .x = top.x + 8, .y = top.y + 126 * scale, .width = top.width - 16, .height = 22 * scale }, text, 0xffffffff));
        try check(self.ui.end_frame.?(self.ui.context));
    }
    pub fn stop(self: *State) Error!void {
        if (!self.started) return;
        try self.clearContent();
        if (self.font != 0) {
            try check(self.draw.remove_font.?(self.draw.context, self.font));
            self.font = 0;
        }
        try check(self.twod.set_camera.?(self.twod.context, &self.previous_camera));
        self.started = false;
        var buffer: [128]u8 = undefined;
        const message = std.fmt.bufPrint(&buffer, "HD2D STOP PASS phase_mask={d} reloads={d} timing_mask={d} features={d}", .{ self.phase_mask, self.reloads, self.timing_seen, self.features }) catch return error.Lifecycle;
        self.log(message);
    }
};
