//! SP-06 diagnostic only. Draft identity and simulation counter stand in for game state.
const std = @import("std");
const nx = @import("nexora");
const font_bytes = @embedFile("fixtures/ui_fixture.nxfont");
const Owner = struct {
    host: nx.GameplayHostV3,
    lifecycle: nx.lifecycle.Api,
    storage: nx.storage.Api,
    ui: nx.ui.Api = .{},
    display: ?nx.display.Api = null,
    draw: ?nx.ui_draw.Api = null,
    font: u64 = 0,
    gate: nx.lifecycle.PauseGate = .{},
    battle_ticks: u64 = 0,
    pending_choice: u32 = 17,
    rumble_percent: u32 = 50,
};
fn self(p: ?*anyopaque) *Owner {
    return @ptrCast(@alignCast(p.?));
}
fn snapshot(o: *Owner) bool {
    var bytes: [16]u8 = .{0} ** 16;
    std.mem.writeInt(u64, bytes[0..8], o.battle_ticks, .little);
    std.mem.writeInt(u32, bytes[8..12], o.pending_choice, .little);
    const success = o.storage.saveSlot("sp06", 1, &bytes) == 0;
    o.gate.saved(success);
    return success;
}
fn observe(o: *Owner) i32 {
    var state = nx.lifecycle.State{};
    const result = o.lifecycle.state(&state);
    if (result != 0) return result;
    o.gate.observe(state);
    if (o.gate.snapshot_due and !o.gate.save_failed) _ = snapshot(o);
    return 0;
}
fn create(out: *?*anyopaque, host: *const nx.GameplayHostV3) callconv(.c) i32 {
    const services = nx.getServices(host) orelse return -2;
    const lifecycle = nx.getLifecycle(services) orelse return -2;
    var storage = nx.storage.Api{};
    if (services.get_storage_api.?(services.context, &storage) != 0 or !storage.valid()) return -2;
    if (host.allocate == null or host.deallocate == null) return -2;
    const memory = host.allocate.?(host.context, 1, @sizeOf(Owner), @alignOf(Owner)) orelse return -3;
    const o: *Owner = @ptrCast(@alignCast(memory));
    o.* = .{ .host = host.*, .lifecycle = lifecycle, .storage = storage, .display = nx.getDisplay(services), .draw = nx.getUiDraw(services) };
    if (services.get_ui_api) |get| _ = get(services.context, &o.ui);
    var bytes: [16]u8 = undefined;
    var info = nx.storage.Info{};
    if (storage.loadSlot("sp06", &bytes, &info) == 0 and info.schema_version == 1 and info.payload_size == bytes.len) {
        o.battle_ticks = std.mem.readInt(u64, bytes[0..8], .little);
        o.pending_choice = std.mem.readInt(u32, bytes[8..12], .little);
        o.gate.paused = true; // Reopen the draft, never auto-commit or resume.
    }
    out.* = memory;
    return 0;
}
fn start(p: ?*anyopaque) callconv(.c) i32 {
    const o = self(p);
    if (o.draw) |draw| {
        var glyphs: [256]nx.ui.Glyph = undefined;
        if (draw.registerBakedFont(font_bytes, &glyphs, &o.font) != 0) return -3;
    }
    if (o.host.log) |log| {
        const message = "SP-06 Zig lifecycle service negotiated; close guard active";
        log(o.host.context, 1, message.ptr, message.len);
    }
    return o.lifecycle.guardClose(true);
}
fn fixed(p: ?*anyopaque, _: f64) callconv(.c) i32 {
    const o = self(p);
    const result = observe(o);
    if (result != 0) return result;
    if (!o.gate.paused) o.battle_ticks += 1;
    return 0;
}
fn button(o: *Owner, id: u64, y: f32, text: []const u8, enabled: bool, font: u64) !bool {
    var state: u32 = 0;
    if (o.ui.menuButton(id, .{ .x = 24, .y = y, .width = 360, .height = 40 }, text, font, 18, enabled, &state) != 0) return error.Ui;
    return state & nx.ui.activated != 0;
}
fn update(p: ?*anyopaque, seconds: f64) callconv(.c) i32 {
    const o = self(p);
    const result = observe(o);
    if (result != 0) return result;
    _ = o.gate.advance(seconds);
    if (!o.ui.valid()) return 0;
    var width: f32 = 640;
    var height: f32 = 480;
    if (o.display) |display| {
        if (display.state()) |state| {
            width = state.logical_width;
            height = state.logical_height;
        }
    }
    if (width <= 0 or height <= 0) return 0;
    const font = o.font;
    if (o.ui.begin_frame.?(o.ui.context, width, height) != 0) return -3;
    const message = if (o.gate.save_failed) "Save failed Previous snapshot retained Retry or cancel close" else if (o.gate.remaining > 0) "Preparing Combat clock stopped" else if (o.gate.paused) "Paused Pending choice retained" else "SP06 lifecycle diagnostic running";
    if (o.ui.label(font, 18, .{ .x = 24, .y = 16, .width = 600, .height = 56 }, message, 0xffffffff) != 0) return -3;
    if (button(o, 1, 80, "Retry snapshot", o.gate.snapshot_due, font) catch return -3) _ = snapshot(o);
    if (button(o, 2, 128, "Resume in 3 seconds", o.gate.paused and o.gate.observed.close_token == 0, font) catch return -3) _ = o.gate.confirmResume();
    if (button(o, 3, 176, "Cancel close", o.gate.observed.close_token != 0, font) catch return -3) _ = o.lifecycle.resolveClose(o.gate.observed.close_token, false);
    if (button(o, 4, 224, "Save and close", o.gate.observed.close_token != 0, font) catch return -3) {
        if (snapshot(o)) _ = o.lifecycle.resolveClose(o.gate.observed.close_token, true);
    }
    if (button(o, 5, 272, "Test rumble supported XInput only", o.gate.observed.rumble_mask & 1 != 0 and !o.gate.paused, font) catch return -3) {
        const gain: f32 = @as(f32, @floatFromInt(o.rumble_percent)) / 100;
        _ = o.lifecycle.vibrate(0, gain, gain, if (gain == 0) 0 else 250);
    }
    if (button(o, 6, 320, "Rumble off", true, font) catch return -3) {
        o.rumble_percent = 0;
        _ = o.lifecycle.vibrate(0, 0, 0, 0);
    }
    return o.ui.end_frame.?(o.ui.context);
}
fn stop(p: ?*anyopaque) callconv(.c) void {
    const o = self(p);
    if (o.draw) |draw| {
        if (o.font != 0) _ = draw.remove_font.?(draw.context, o.font);
        o.font = 0;
    }
    for (0..4) |id| _ = o.lifecycle.vibrate(@intCast(id), 0, 0, 0);
    _ = o.lifecycle.guardClose(false);
}
fn destroy(p: ?*anyopaque) callconv(.c) void {
    const o = self(p);
    const host = o.host;
    host.deallocate.?(host.context, 1, o, @sizeOf(Owner), @alignOf(Owner));
}
// Test/diagnostic controls; not a public gameplay ABI.
export fn NexoraLifecycleControl(p: ?*anyopaque, command: u32, value: u32) i64 {
    const o = self(p);
    switch (command) {
        0 => {
            o.gate.paused = true;
            o.gate.remaining = 0;
        },
        1 => return @intFromBool(o.gate.confirmResume()),
        2 => return @intFromBool(snapshot(o)),
        3 => return o.lifecycle.resolveClose(o.gate.observed.close_token, false),
        4 => {
            if (!snapshot(o)) return -3;
            return o.lifecycle.resolveClose(o.gate.observed.close_token, true);
        },
        5 => return @intFromBool(o.gate.paused),
        6 => return @intCast(o.battle_ticks),
        7 => return o.pending_choice,
        8 => o.pending_choice = value,
        9 => return @intFromBool(o.gate.save_failed),
        10 => return @intFromBool(o.gate.remaining > 0),
        11 => {
            if (value > 100) return -1;
            o.rumble_percent = value;
            const gain: f32 = @as(f32, @floatFromInt(value)) / 100;
            return o.lifecycle.vibrate(0, gain, gain, if (value == 0) 0 else 250);
        },
        12 => {
            o.gate.controlled_device = if (value < 4) value else null;
        },
        else => return -1,
    }
    return 0;
}
export fn NexoraGameModuleLoad(abi: u32, module: ?*nx.GameModuleV3) i32 {
    const out = module orelse return -1;
    if (abi != 3 or out.struct_size < @sizeOf(nx.GameModuleV3)) return -2;
    out.* = .{ .struct_size = @sizeOf(nx.GameModuleV3), .abi_version = 3, .capabilities = 2, .module_state = null, .create = create, .on_start = start, .fixed_update = fixed, .update = update, .on_stop = stop, .destroy = destroy, .save_state = null, .load_state = null };
    return 0;
}
