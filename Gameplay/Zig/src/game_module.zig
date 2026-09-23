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
            if (read(host.context, primary_entity, transform_component_type, &transform,
                @sizeOf(Transform)) == 0) {
                transform.x += delta_seconds;
                if (host.write_component) |write| {
                    _ = write(host.context, primary_entity, transform_component_type, &transform,
                        @sizeOf(Transform));
                }
            }
        }
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
