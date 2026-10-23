//! Independent offline SP-05 diagnostic. Procedural WAVs and subtitle policy are not final content.
const std = @import("std");
const nx = @import("nexora");
const dialogue = nx.dialogue;
const bytes_limit = 8 * 1024 * 1024 + 64;
const Owner = struct {
    host: nx.GameplayHostV3,
    line: dialogue.Line,
    usage: nx.audio_state.Api,
    manual: bool = false,
    round: u32 = 0,
    step: u32 = 0,
    done: bool = false,
};
fn self(p: ?*anyopaque) *Owner {
    return @ptrCast(@alignCast(p.?));
}
fn create(out: *?*anyopaque, host: *const nx.GameplayHostV3) callconv(.c) i32 {
    const services = nx.getServices(host) orelse return -2;
    const files = nx.getAssets(services) orelse return -2;
    const usage = nx.getAudioState(services) orelse return -2;
    var sound = nx.audio.Api{};
    if (services.get_audio_api.?(services.context, &sound) != 0 or !sound.valid() or host.allocate == null or host.deallocate == null) return -2;
    const memory = host.allocate.?(host.context, 1, @sizeOf(Owner), @alignOf(Owner)) orelse return -3;
    const buffer = host.allocate.?(host.context, 1, bytes_limit, 64) orelse {
        host.deallocate.?(host.context, 1, memory, @sizeOf(Owner), @alignOf(Owner));
        return -3;
    };
    const o: *Owner = @ptrCast(@alignCast(memory));
    const ptr: [*]u8 = @ptrCast(buffer);
    o.* = .{ .host = host.*, .line = .{ .sound = sound, .control = usage, .files = files, .buffer = ptr[0..bytes_limit], .asset = 0x535005 }, .usage = usage };
    out.* = memory;
    return 0;
}
fn start(p: ?*anyopaque) callconv(.c) i32 {
    self(p).line.start(0);
    return 0;
}
fn update(p: ?*anyopaque, _: f64) callconv(.c) i32 {
    const o = self(p);
    o.line.tick();
    if (o.manual or o.done) return 0;
    if (o.line.phase == .loading) {
        std.Thread.sleep(1_000_000);
        return 0;
    }
    switch (o.step) {
        0 => {
            o.line.volume(0.5, false) catch return -3;
            o.line.pause(true) catch return -3;
        },
        1 => {
            o.line.volume(0.5, true) catch return -3;
        },
        2 => {
            o.line.pause(false) catch return -3;
            o.line.volume(1, false) catch return -3;
        },
        3 => {
            o.line.start(o.round);
        }, // explicit replay without touching music
        4 => {
            o.line.start(null);
            if (o.line.phase != .fallback) return -3;
        },
        5 => {
            o.round += 1;
            if (o.round < 12) {
                o.line.start(o.round);
                o.step = 0;
                return 0;
            }
            o.line.start(12); // valid blob, invalid WAV: subtitle fallback
        },
        6 => {
            if (o.line.phase != .fallback) return -3;
            o.line.start(0xffffffff);
        },
        else => {
            if (o.line.phase != .fallback) return -3;
            o.line.cancel();
            var usage = nx.audio_state.Usage{};
            if (o.usage.usage(&usage) != 0 or usage.active_bytes != 0 or usage.retired_bytes != 0 or usage.active_voices != 0) return -3;
            const text = "SP-05 PASS: 12 offline WAV lines, skip/replay/pause/mute, corrupt/missing subtitles fallback, zero PCM residency";
            if (o.host.log) |log| log(o.host.context, 1, text.ptr, text.len);
            o.done = true;
        },
    }
    o.step += 1;
    return 0;
}
fn stop(p: ?*anyopaque) callconv(.c) void {
    self(p).line.cancel();
}
fn destroy(p: ?*anyopaque) callconv(.c) void {
    const o = self(p);
    const host = o.host;
    host.deallocate.?(host.context, 1, o.line.buffer.ptr, bytes_limit, 64);
    host.deallocate.?(host.context, 1, o, @sizeOf(Owner), @alignOf(Owner));
}
// Diagnostic test control, not a public engine service or production game protocol.
export fn NexoraDialogueControl(p: ?*anyopaque, command: u32, arg: u32, value: f32) i32 {
    const o = self(p);
    switch (command) {
        0 => o.manual = true,
        1 => o.line.start(arg),
        2 => o.line.volume(value, arg != 0) catch return -1,
        3 => o.line.pause(arg != 0) catch return -1,
        4 => o.line.cancel(),
        5 => return @intFromEnum(o.line.phase),
        else => return -1,
    }
    return 0;
}
export fn NexoraGameModuleLoad(abi: u32, module: ?*nx.GameModuleV3) i32 {
    const out = module orelse return -1;
    if (abi != 3 or out.struct_size < @sizeOf(nx.GameModuleV3)) return -2;
    out.* = .{ .struct_size = @sizeOf(nx.GameModuleV3), .abi_version = 3, .capabilities = 0, .module_state = null, .create = create, .on_start = start, .fixed_update = null, .update = update, .on_stop = stop, .destroy = destroy, .save_state = null, .load_state = null };
    return 0;
}
