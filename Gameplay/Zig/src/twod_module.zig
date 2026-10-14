//! Engine-owned 2D sample: Zig controls copied state, never windows or GPU objects.
const nx = @import("nexora");
const std = @import("std");
const State = extern struct {
    schema: u32 = 1,
    events: u32 = 0,
    scene: u64 = 0,
    entity: u64 = 0,
    queries: u64 = 0,
    elapsed: f64 = 0,
    handles: [32]nx.TwoDInstanceHandle = @splat(.{}),
};
const Owner = struct { host: *const nx.GameplayHostV3, api: nx.TwoDApiV1, state: State = .{} };
fn owner(p: ?*anyopaque) ?*Owner {
    return @ptrCast(@alignCast(p orelse return null));
}

fn create(output: *?*anyopaque, host: *const nx.GameplayHostV3) callconv(.c) i32 {
    const api = nx.getTwoD(host) orelse return -2;
    if ((host.capabilities & 4) == 0 or host.deallocate == null or host.spawn_entity == null or
        host.load_scene == null or host.activate_scene == null or host.capture_input == null or
        host.read_component == null or host.write_component == null) return -2;
    const allocate = host.allocate orelse return -2;
    const memory = allocate(host.context, 1, @sizeOf(Owner), @alignOf(Owner)) orelse return -3;
    const value: *Owner = @ptrCast(@alignCast(memory));
    value.* = .{ .host = host, .api = api };
    output.* = memory;
    return 0;
}
fn start(p: ?*anyopaque) callconv(.c) i32 {
    const o = owner(p) orelse return -1;
    if (o.state.entity != 0) return 0; // Migrated IDs and pool handles remain host-owned.
    const h = o.host;
    const a = &o.api;
    const name = "Zig 2D";
    if (h.load_scene.?(h.context, name.ptr, name.len, 0, &o.state.scene) != 0 or
        h.activate_scene.?(h.context, o.state.scene) != 0) return -3;
    const descriptor = nx.EntitySpawnDescriptor{};
    if (h.spawn_entity.?(h.context, o.state.scene, &descriptor, &o.state.entity) != 0) return -3;
    const sprite = nx.TwoDSprite{ .atlas_high = 1, .atlas_low = 2, .region = 1 };
    const collider = nx.TwoDCollider{};
    const camera = nx.TwoDCamera{ .vertical_size = 8 };
    if (a.set_sprite.?(a.context, o.state.entity, &sprite) != 0 or
        a.set_collider.?(a.context, o.state.entity, &collider) != 0 or
        a.set_camera.?(a.context, &camera) != 0 or
        a.play_animation.?(a.context, o.state.entity, 3, 4, 1) != 0) return -3;
    const tile = nx.TwoDTile{ .x = -2, .y = -1, .layer = -1, .region = 1, .collision = 1 };
    if (a.paint_tiles.?(a.context, @ptrCast(&tile), 1) != 0 or
        a.set_chunk_loaded.?(a.context, -1, -1, 0) != 0 or
        a.set_chunk_loaded.?(a.context, -1, -1, 1) != 0) return -3;
    var read_tile = nx.TwoDTile{};
    var read_camera = nx.TwoDCamera{};
    var read_collider = nx.TwoDCollider{};
    if (a.get_tile.?(a.context, -2, -1, -1, &read_tile) != 0 or read_tile.collision != 1 or
        a.get_camera.?(a.context, &read_camera) != 0 or read_camera.vertical_size != 8 or
        a.get_collider.?(a.context, o.state.entity, &read_collider) != 0 or
        read_collider.radius != 0.5) return -3;
    const temporary = nx.TwoDInstance{ .sprite = sprite };
    var temporary_handle = nx.TwoDInstanceHandle{};
    if (a.create_instances.?(a.context, @ptrCast(&temporary), 1, @ptrCast(&temporary_handle)) != 0 or
        a.destroy_instances.?(a.context, @ptrCast(&temporary_handle), 1) != 0) return -3;
    var instances: [32]nx.TwoDInstance = undefined;
    for (&instances, 0..) |*v, i| {
        v.* = .{ .sprite = sprite };
        v.sprite.size = .{ .x = 0.125, .y = 0.125 };
        v.transform.position = .{ .x = @as(f64, @floatFromInt(i)) * 0.125 - 2, .y = -2 };
    }
    if (a.create_instances.?(a.context, &instances, instances.len, &o.state.handles) != 0) return -3;
    return 0;
}
fn fixedUpdate(p: ?*anyopaque, delta: f64) callconv(.c) i32 {
    const o = owner(p) orelse return -1;
    const h = o.host;
    const a = &o.api;
    var input = nx.InputSnapshot{};
    var transform = nx.TransformV2{};
    var sprite = nx.TwoDSprite{};
    const component = nx.componentType("Nexora.TransformV2");
    if (h.capture_input.?(h.context, 0, &input) != 0 or
        h.read_component.?(h.context, o.state.entity, component, &transform, @sizeOf(nx.TransformV2)) != 0 or
        a.get_sprite.?(a.context, o.state.entity, &sprite) != 0) return -3;
    transform.position.x += input.move_x * delta;
    transform.position.y += input.move_y * delta;
    sprite.flags = 8 | @as(u32, if (input.move_x < 0) 1 else 0);
    if (h.write_component.?(h.context, o.state.entity, component, &transform, @sizeOf(nx.TransformV2)) != 0 or
        a.set_sprite.?(a.context, o.state.entity, &sprite) != 0 or
        a.pause_animation.?(a.context, o.state.entity, input.buttons & 1) != 0) return -3;
    var events: [8]nx.TwoDEvent = undefined;
    var result = nx.TwoDAnimationResult{};
    if (a.advance_animation.?(a.context, o.state.entity, delta, &events, events.len, &result) != 0) return -3;
    o.state.events += result.event_count;
    for (events[0..result.event_count]) |event| {
        if (event.length != 4 or !std.mem.eql(u8, event.name[0..event.length], "step")) return -3;
    }
    const shape = nx.TwoDShape{ .geometry = .{ .offset = .{
        .x = @floatCast(transform.position.x),
        .y = @floatCast(transform.position.y),
    }, .radius = 2 } };
    var count: u32 = 0;
    if (a.rebuild_queries.?(a.context, 0) != 0 or
        a.overlap.?(a.context, 0, &shape, null, 0, &count) != 0 or count == 0) return -3;
    o.state.queries += count;
    const swept = nx.TwoDSweep{
        .from = .{ .x = shape.geometry.offset.x - 1, .y = shape.geometry.offset.y },
        .to = .{ .x = shape.geometry.offset.x + 1, .y = shape.geometry.offset.y },
    };
    var hits: [1]nx.TwoDHit = undefined;
    if (a.sweep.?(a.context, 0, &swept, &hits, hits.len, &count) != 0 or count != 1 or
        hits[0].id != o.state.entity or hits[0].fraction < 0 or hits[0].fraction > 1) return -3;
    o.state.elapsed += delta;
    return 0;
}
fn update(p: ?*anyopaque, delta: f64) callconv(.c) i32 {
    const o = owner(p) orelse return -1;
    const a = &o.api;
    var values: [32]nx.TwoDInstance = undefined;
    if (a.read_instances.?(a.context, &o.state.handles, values.len, &values) != 0) return -3;
    for (&values) |*v| v.transform.position.x += delta * 0.125;
    return a.write_instances.?(a.context, &o.state.handles, values.len, &values);
}
fn stop(_: ?*anyopaque) callconv(.c) void {}
fn destroy(p: ?*anyopaque) callconv(.c) void {
    const o = owner(p) orelse return;
    o.host.deallocate.?(o.host.context, 1, p, @sizeOf(Owner), @alignOf(Owner));
}
fn save(p: ?*anyopaque, data: ?*anyopaque, size: u32) callconv(.c) u32 {
    const o = owner(p) orelse return 0;
    if (data == null or size < @sizeOf(State)) return @sizeOf(State);
    const bytes: [*]u8 = @ptrCast(data.?);
    @memcpy(bytes[0..@sizeOf(State)], std.mem.asBytes(&o.state));
    return @sizeOf(State);
}
fn restore(p: ?*anyopaque, data: ?*const anyopaque, size: u32) callconv(.c) i32 {
    const o = owner(p) orelse return -1;
    if (data == null or size != @sizeOf(State)) return -1;
    const bytes: [*]const u8 = @ptrCast(data.?);
    var candidate: State = undefined;
    @memcpy(std.mem.asBytes(&candidate), bytes[0..@sizeOf(State)]);
    if (candidate.schema != 1 or !std.math.isFinite(candidate.elapsed) or candidate.elapsed < 0 or
        candidate.entity == 0 or candidate.scene == 0) return -1;
    var sprite = nx.TwoDSprite{};
    var instances: [32]nx.TwoDInstance = undefined;
    if (o.api.get_sprite.?(o.api.context, candidate.entity, &sprite) != 0 or
        o.api.read_instances.?(o.api.context, &candidate.handles, 32, &instances) != 0) return -1;
    o.state = candidate;
    return 0;
}
export fn NexoraGameModuleLoad(requested: u32, out: ?*nx.GameModuleV3) i32 {
    if (requested != nx.abi_version) return -2;
    const module = out orelse return -1;
    if (module.struct_size < @sizeOf(nx.GameModuleV3)) return -2;
    module.* = .{ .struct_size = @sizeOf(nx.GameModuleV3), .abi_version = nx.abi_version, .capabilities = 3, .module_state = null, .create = create, .on_start = start, .fixed_update = fixedUpdate, .update = update, .on_stop = stop, .destroy = destroy, .save_state = save, .load_state = restore };
    return 0;
}

// The actual Zig compiler exposes every wire size/field offset for the C++ layout gate.
export fn NexoraTwoDLayout(index: u32) u64 {
    const types = .{ nx.TwoDSprite, nx.TwoDCollider, nx.TwoDCamera, nx.TwoDShape, nx.TwoDSweep, nx.TwoDHit, nx.TwoDEvent, nx.TwoDAnimationResult, nx.TwoDTile, nx.TwoDInstanceHandle, nx.TwoDInstance, nx.TwoDApiV1 };
    comptime var slot = 0;
    inline for (types) |T| {
        if (index == slot) return @sizeOf(T);
        slot += 1;
        inline for (@typeInfo(T).@"struct".fields) |field| {
            if (index == slot) return @offsetOf(T, field.name);
            slot += 1;
        }
    }
    return 0xffffffffffffffff;
}
export fn NexoraTwoDProbe(host: *const nx.GameplayHostV3) i32 {
    return if (nx.getTwoD(host) != null) 1 else 0;
}
