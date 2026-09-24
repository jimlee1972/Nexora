const nexora = @import("nexora");
const abi_version = nexora.abi_version;

fn hashName(comptime text: []const u8) u64 {
    var hash: u64 = 14695981039346656037;
    for (text) |byte| {
        hash ^= byte;
        hash *%= 1099511628211;
    }
    return hash;
}

const primary_entity: u64 = 0;
const transform_component_type: u64 = hashName("Nexora.Transform");

const GameplayHost = nexora.GameplayHostV3;
const GameModule = nexora.GameModuleV3;

const State = extern struct {
    update_count: u32 = 0,
    fixed_update_count: u32 = 0,
    start_count: u32 = 0,
    elapsed_seconds: f64 = 0,
    scene: u64 = 0,
    primary_entity: u64 = 0,
};

const Allocation = extern struct {
    state: State = .{},
    host: *const GameplayHost,
};

const Transform = extern struct {
    x: f64 = 0,
    y: f64 = 0,
    z: f64 = 0,
};

var observed_update_count: u32 = 0;
var observed_fixed_update_count: u32 = 0;
var observed_start_count: u32 = 0;
var observed_elapsed_seconds: f64 = 0;

fn create(module_state: *?*anyopaque, host: *const GameplayHost) callconv(.c) i32 {
    if (host.abi_version != abi_version or host.struct_size < @sizeOf(GameplayHost)) return -1;
    if ((host.capabilities & 4) == 0) return -2;
    const allocate = host.allocate orelse return -2;
    const allocation = allocate(host.context, @intFromEnum(nexora.AllocationOwner.gameplay_state),
        @sizeOf(Allocation), @alignOf(Allocation)) orelse return -3;
    const created: *Allocation = @ptrCast(@alignCast(allocation));
    created.* = .{ .host = host };
    observed_update_count = 0;
    observed_fixed_update_count = 0;
    observed_start_count = 0;
    observed_elapsed_seconds = 0;
    module_state.* = allocation;
    if (host.log) |log| {
        const message = "Zig gameplay initialized";
        log(host.context, 1, message.ptr, @intCast(message.len));
    }
    return 0;
}

fn saveState(module_state: ?*anyopaque, data: ?*anyopaque, data_size: u32) callconv(.c) u32 {
    const required: u32 = @sizeOf(State);
    if (data == null or data_size < required) return required;
    const owner: *const Allocation = @ptrCast(@alignCast(module_state orelse return 0));
    const destination: *State = @ptrCast(@alignCast(data.?));
    destination.* = owner.state;
    return required;
}

fn loadState(module_state: ?*anyopaque, data: ?*const anyopaque, data_size: u32) callconv(.c) i32 {
    if (data_size != @sizeOf(State)) return -1;
    const owner: *Allocation = @ptrCast(@alignCast(module_state orelse return -2));
    const source: *const State = @ptrCast(@alignCast(data orelse return -3));
    owner.state = source.*;
    observed_update_count = source.update_count;
    observed_fixed_update_count = source.fixed_update_count;
    observed_start_count = source.start_count;
    observed_elapsed_seconds = source.elapsed_seconds;
    return 0;
}

fn onStart(module_state: ?*anyopaque) callconv(.c) i32 {
    const opaque_state = module_state orelse return -3;
    const owner: *Allocation = @ptrCast(@alignCast(opaque_state));
    const current = &owner.state;
    current.start_count += 1;
    observed_start_count = current.start_count;
    const host = owner.host;
    if ((host.capabilities & nexora.scene_api_capability) != 0 and current.scene == 0) {
        const load_scene = host.load_scene orelse return -2;
        const activate_scene = host.activate_scene orelse return -2;
        const spawn_entity = host.spawn_entity orelse return -2;
        const despawn_entity = host.despawn_entity orelse return -2;
        const resolve_asset = host.resolve_asset orelse return -2;
        var scene: u64 = 0;
        const name = "Zig Showcase Hub";
        if (load_scene(host.context, name.ptr, @intCast(name.len), 1, &scene) != 0 or
            activate_scene(host.context, scene) != 0) return -3;
        var mesh = nexora.AssetHandle{};
        var material = nexora.AssetHandle{};
        if (resolve_asset(host.context, 0x4e45584f5241, 0x2001, &mesh) != 0 or
            resolve_asset(host.context, 0x4e45584f5241, 0x1001, &material) != 0) return -3;
        var descriptor = nexora.EntitySpawnDescriptor{ .components = 1, .position = .{ .x = 0, .y = 2, .z = 6 }, .camera_fov_degrees = 60 };
        var entity: u64 = 0;
        if (spawn_entity(host.context, scene, &descriptor, &entity) != 0) return -3;
        descriptor = .{ .components = 2, .position = .{ .x = 2, .y = 4, .z = 2 }, .light_intensity = 2 };
        if (spawn_entity(host.context, scene, &descriptor, &entity) != 0) return -3;
        const positions = [_]f64{ 0, -2, 2 };
        for (positions, 0..) |x, index| {
            descriptor = .{ .components = 4 | 8, .position = .{ .x = x }, .mesh = .{ .value = mesh.value + @as(u64, @intCast(index)) }, .material = material, .bounds_minimum = .{ .x = x - 0.5, .y = -0.5, .z = -0.5 }, .bounds_maximum = .{ .x = x + 0.5, .y = 0.5, .z = 0.5 } };
            if (spawn_entity(host.context, scene, &descriptor, &entity) != 0) return -3;
            if (index == 0) current.primary_entity = entity;
        }
        descriptor = .{};
        if (spawn_entity(host.context, scene, &descriptor, &entity) != 0 or despawn_entity(host.context, entity) != 0) return -3;
        current.scene = scene;
    }
    return 0;
}

fn fixedUpdate(module_state: ?*anyopaque, delta_seconds: f64) callconv(.c) i32 {
    _ = delta_seconds;
    const opaque_state = module_state orelse return -3;
    const owner: *Allocation = @ptrCast(@alignCast(opaque_state));
    const current = &owner.state;
    current.fixed_update_count += 1;
    observed_fixed_update_count = current.fixed_update_count;
    return 0;
}

fn update(module_state: ?*anyopaque, delta_seconds: f64) callconv(.c) i32 {
    const opaque_state = module_state orelse return -3;
    const owner: *Allocation = @ptrCast(@alignCast(opaque_state));
    const current = &owner.state;
    current.update_count += 1;
    current.elapsed_seconds += delta_seconds;
    observed_update_count = current.update_count;
    observed_elapsed_seconds = current.elapsed_seconds;
    const host = owner.host;
    {
        var transform = Transform{};
        if (host.read_component) |read| {
            if (read(host.context, if (current.primary_entity != 0) current.primary_entity else primary_entity, transform_component_type, &transform,
                @sizeOf(Transform)) == 0) {
                transform.x += delta_seconds;
                if (host.write_component) |write| {
                    _ = write(host.context, if (current.primary_entity != 0) current.primary_entity else primary_entity, transform_component_type, &transform,
                        @sizeOf(Transform));
                }
            }
        }
    }
    if ((host.capabilities & nexora.scene_api_capability) != 0) {
        var input = nexora.InputSnapshot{};
        if (host.capture_input) |capture| if (capture(host.context, 0, &input) != 0) return -3;
        var hit = nexora.RaycastHit{};
        const ray = nexora.RaycastRequest{ .origin = .{ .z = 5 }, .direction = .{ .z = -1 }, .distance = 10 };
        if (host.raycast) |raycast| if (raycast(host.context, &ray, &hit) != 0) return -3;
        const line = nexora.DebugLine{ .start = ray.origin, .end = hit.point, .rgba = 0xff00ffff };
        if (host.debug_draw_line) |draw| if (draw(host.context, &line) != 0) return -3;
        var diagnostics = nexora.FrameDiagnostics{};
        if (host.get_diagnostics) |get| if (get(host.context, &diagnostics) != 0) return -3;
    }
    return 0;
}

fn onStop(module_state: ?*anyopaque) callconv(.c) void {
    _ = module_state;
}

fn destroy(module_state: ?*anyopaque) callconv(.c) void {
    const allocation = module_state orelse return;
    const owner: *Allocation = @ptrCast(@alignCast(allocation));
    const host = owner.host;
    if (host.deallocate) |deallocate| {
        deallocate(host.context, @intFromEnum(nexora.AllocationOwner.gameplay_state), allocation,
            @sizeOf(Allocation), @alignOf(Allocation));
    }
}

export fn NexoraGameModuleLoad(requested_abi: u32, module: ?*GameModule) i32 {
    if (requested_abi != abi_version) return -1;
    const output = module orelse return -2;
    if (output.struct_size < @sizeOf(GameModule)) return -3;
    output.* = .{
        .struct_size = @intCast(@sizeOf(GameModule)),
        .abi_version = abi_version,
        .capabilities = 1 | 2,
        .module_state = null,
        .create = create,
        .on_start = onStart,
        .fixed_update = fixedUpdate,
        .update = update,
        .on_stop = onStop,
        .destroy = destroy,
        .save_state = saveState,
        .load_state = loadState,
    };
    return 0;
}

export fn NexoraGameModuleUpdateCount() u32 {
    return observed_update_count;
}

export fn NexoraGameModuleElapsedSeconds() f64 {
    return observed_elapsed_seconds;
}

export fn NexoraGameModuleFixedUpdateCount() u32 {
    return observed_fixed_update_count;
}

export fn NexoraGameModuleStartCount() u32 {
    return observed_start_count;
}
