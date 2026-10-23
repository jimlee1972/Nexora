//! SP-03 diagnostic transition workload. No production rules or final artwork.
const nx = @import("nexora");
const std = @import("std");
const count = 1800;
const Owner = struct {
    host: nx.GameplayHostV3,
    assets: nx.assets.Api,
    api: nx.TwoDApiV1,
    ticket: u64 = 0,
    atlas: u64 = 0,
    clip: u64 = 0,
    scene: u64 = 0,
    entity: u64 = 0,
    step: u32 = 0,
    round: u32 = 0,
    frames: u32 = 0,
    events: u32 = 0,
    live: bool = false,
    done: bool = false,
    instances: [count]nx.TwoDInstance = undefined,
    handles: [count]nx.TwoDInstanceHandle = undefined,
};
fn self(p: ?*anyopaque) *Owner {
    return @ptrCast(@alignCast(p.?));
}
fn create(out: *?*anyopaque, host: *const nx.GameplayHostV3) callconv(.c) i32 {
    const services = nx.getServices(host) orelse return -2;
    const assets = nx.getAssets(services) orelse return -2;
    const api = nx.getTwoD(host) orelse return -2;
    const alloc = host.allocate orelse return -2;
    if (host.deallocate == null or host.spawn_entity == null or host.despawn_entity == null or
        host.load_scene == null or host.activate_scene == null) return -2;
    const memory = alloc(host.context, 1, @sizeOf(Owner), @alignOf(Owner)) orelse return -3;
    const o: *Owner = @ptrCast(@alignCast(memory));
    o.* = .{ .host = host.*, .assets = assets, .api = api };
    out.* = memory;
    return 0;
}
fn start(p: ?*anyopaque) callconv(.c) i32 {
    const o = self(p);
    const name = "Main";
    if (o.host.load_scene.?(o.host.context, name.ptr, name.len, 0, &o.scene) != 0 or
        o.host.activate_scene.?(o.host.context, o.scene) != 0) return -3;
    return o.assets.requestLoad(10, &o.ticket); // opaque bytes after the five Atlas/Clip pairs
}
fn waiting(o: *Owner) !bool {
    var state = nx.assets.LoadState{};
    if (o.assets.readState(o.ticket, &state) != 0 or state.phase == nx.assets.failed) return error.Load;
    if (state.phase != nx.assets.ready) {
        std.Thread.sleep(1_000_000); // diagnostic headless host must allow its IO worker to run
        return true;
    }
    if (state.catalog_index < 10) {
        if (state.high == 88) o.atlas = state.low else if (state.high == 89) o.clip = state.low else return error.Identity;
    }
    return false;
}
fn release(o: *Owner) !void {
    if (o.assets.releaseRequest(o.ticket) != 0) return error.Release;
    o.ticket = 0;
}
fn run(o: *Owner) !void {
    if (o.done) return;
    if (o.step == 0) {
        if (try waiting(o)) return;
        var bytes: [3]u8 = undefined;
        var length: u64 = 0;
        if (o.assets.copyBytes(o.ticket, &bytes, &length) != 0 or length != 3 or !std.mem.eql(u8, &bytes, "SP3")) return error.Bytes;
        try release(o);
        if (o.assets.requestLoad(0, &o.ticket) != 0) return error.Request;
        o.step = 1;
    } else if (o.step == 1) {
        if (try waiting(o)) return;
        try release(o);
        if (o.assets.requestLoad((o.round % 5) * 2 + 1, &o.ticket) != 0) return error.Request;
        o.step = 2;
    } else if (o.step == 2) {
        if (try waiting(o)) return;
        try release(o);
        for (&o.instances, 0..) |*instance, i| instance.* = .{
            .transform = .{ .position = .{ .x = @as(f64, @floatFromInt(i % 60)) / 6 - 5, .y = @as(f64, @floatFromInt(i / 60)) / 6 - 2.5 } },
            .sprite = .{ .atlas_high = 88, .atlas_low = o.atlas, .region = 1, .size = .{ .x = 0.1, .y = 0.1 } },
        };
        if (o.api.create_instances.?(o.api.context, &o.instances, count, &o.handles) != 0) return error.Pool;
        o.live = true;
        const spawn = nx.EntitySpawnDescriptor{};
        if (o.host.spawn_entity.?(o.host.context, o.scene, &spawn, &o.entity) != 0) return error.Spawn;
        const sprite = nx.TwoDSprite{ .atlas_high = 88, .atlas_low = o.atlas, .region = 1 };
        if (o.api.set_sprite.?(o.api.context, o.entity, &sprite) != 0 or
            o.api.play_animation.?(o.api.context, o.entity, 89, o.clip, 1) != 0) return error.Animation;
        o.frames = 0;
        o.step = 3;
    } else if (o.step == 3) {
        var events: [2]nx.TwoDEvent = undefined;
        var result = nx.TwoDAnimationResult{};
        if (o.api.advance_animation.?(o.api.context, o.entity, 0.1, &events, 2, &result) != 0 or
            result.event_count != 1 or events[0].length != 4 or !std.mem.eql(u8, events[0].name[0..4], "step")) return error.Events;
        o.events += result.event_count;
        o.frames += 1;
        if (o.frames < 2) return;
        if (o.api.read_instances.?(o.api.context, &o.handles, count, &o.instances) != 0 or
            o.api.destroy_instances.?(o.api.context, &o.handles, count) != 0) return error.Pool;
        o.live = false;
        if (o.api.read_instances.?(o.api.context, &o.handles, count, &o.instances) != -1) return error.Stale;
        if (o.host.despawn_entity.?(o.host.context, o.entity) != 0) return error.Despawn;
        o.entity = 0;
        if (o.assets.remove(88, o.atlas) != -3 or o.assets.remove(89, o.clip) != 0 or
            o.assets.remove(88, o.atlas) != 0) return error.Dependency;
        o.atlas = 0;
        o.clip = 0;
        o.step = 4;
    } else {
        var usage = nx.assets.Usage{};
        if (o.assets.usage(&usage) != 0 or usage.active_bytes != 0 or usage.staging_bytes != 0) return error.Leak;
        if (usage.retired_bytes != 0) return;
        o.round += 1;
        if (o.round == 15) {
            if (o.events != 30 or usage.peak_content_bytes >= 17 * 1024 * 1024) return error.Budget;
            const text = "SP-03 PASS: 15 transitions, 1800 pooled instances, 30 animation events, zero resident/staging bytes";
            if (o.host.log) |log| log(o.host.context, 1, text.ptr, text.len);
            o.done = true;
            return;
        }
        if (o.assets.requestLoad((o.round % 5) * 2, &o.ticket) != 0) return error.Request;
        o.step = 1;
    }
}
fn update(p: ?*anyopaque, _: f64) callconv(.c) i32 {
    const o = self(p);
    run(o) catch |err| {
        const message = @errorName(err);
        if (o.host.log) |log| log(o.host.context, 2, message.ptr, @intCast(message.len));
        return -3;
    };
    return 0;
}
fn stop(p: ?*anyopaque) callconv(.c) void {
    const o = self(p);
    if (o.ticket != 0) _ = o.assets.releaseRequest(o.ticket);
    if (o.live) _ = o.api.destroy_instances.?(o.api.context, &o.handles, count);
    if (o.entity != 0) _ = o.host.despawn_entity.?(o.host.context, o.entity);
    if (o.clip != 0) _ = o.assets.remove(89, o.clip);
    if (o.atlas != 0) _ = o.assets.remove(88, o.atlas);
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
