const abi_version: u32 = 3;

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

const GameplayHost = extern struct {
    struct_size: u32,
    abi_version: u32,
    capabilities: u64,
    context: ?*anyopaque,
    log: ?*const fn (?*anyopaque, u32, [*]const u8, u32) callconv(.c) void,
    read_component: ?*const fn (?*anyopaque, u64, u64, ?*anyopaque, u32) callconv(.c) i32,
    write_component: ?*const fn (?*anyopaque, u64, u64, ?*const anyopaque, u32) callconv(.c) i32,
};

const GameModule = extern struct {
    struct_size: u32,
    abi_version: u32,
    capabilities: u64,
    module_state: ?*anyopaque,
    create: ?*const fn (*?*anyopaque, *const GameplayHost) callconv(.c) i32,
    on_start: ?*const fn (?*anyopaque) callconv(.c) i32,
    fixed_update: ?*const fn (?*anyopaque, f64) callconv(.c) i32,
    update: ?*const fn (?*anyopaque, f64) callconv(.c) i32,
    on_stop: ?*const fn (?*anyopaque) callconv(.c) void,
    destroy: ?*const fn (?*anyopaque) callconv(.c) void,
    save_state: ?*const fn (?*anyopaque, ?*anyopaque, u32) callconv(.c) u32,
    load_state: ?*const fn (?*anyopaque, ?*const anyopaque, u32) callconv(.c) i32,
};

const State = extern struct {
    update_count: u32 = 0,
    fixed_update_count: u32 = 0,
    start_count: u32 = 0,
    elapsed_seconds: f64 = 0,
};

const Transform = extern struct {
    x: f64 = 0,
    y: f64 = 0,
    z: f64 = 0,
};

var state = State{};
var host_api: ?*const GameplayHost = null;

fn create(module_state: *?*anyopaque, host: *const GameplayHost) callconv(.c) i32 {
    if (host.abi_version != abi_version or host.struct_size < @sizeOf(GameplayHost)) return -1;
    state = State{};
    host_api = host;
    module_state.* = @ptrCast(&state);
    if (host.log) |log| {
        const message = "Zig gameplay initialized";
        log(host.context, 1, message.ptr, @intCast(message.len));
    }
    return 0;
}

fn saveState(module_state: ?*anyopaque, data: ?*anyopaque, data_size: u32) callconv(.c) u32 {
    const required: u32 = @sizeOf(State);
    if (data == null or data_size < required) return required;
    const source: *const State = @ptrCast(@alignCast(module_state orelse return 0));
    const destination: *State = @ptrCast(@alignCast(data.?));
    destination.* = source.*;
    return required;
}

fn loadState(module_state: ?*anyopaque, data: ?*const anyopaque, data_size: u32) callconv(.c) i32 {
    if (data_size != @sizeOf(State)) return -1;
    const destination: *State = @ptrCast(@alignCast(module_state orelse return -2));
    const source: *const State = @ptrCast(@alignCast(data orelse return -3));
    destination.* = source.*;
    return 0;
}

fn onStart(module_state: ?*anyopaque) callconv(.c) i32 {
    const opaque_state = module_state orelse return -3;
    const current: *State = @ptrCast(@alignCast(opaque_state));
    current.start_count += 1;
    return 0;
}

fn fixedUpdate(module_state: ?*anyopaque, delta_seconds: f64) callconv(.c) i32 {
    _ = delta_seconds;
    const opaque_state = module_state orelse return -3;
    const current: *State = @ptrCast(@alignCast(opaque_state));
    current.fixed_update_count += 1;
    return 0;
}

fn update(module_state: ?*anyopaque, delta_seconds: f64) callconv(.c) i32 {
    const opaque_state = module_state orelse return -3;
    const current: *State = @ptrCast(@alignCast(opaque_state));
    current.update_count += 1;
    current.elapsed_seconds += delta_seconds;
    if (host_api) |host| {
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
    host_api = null;
}

fn destroy(module_state: ?*anyopaque) callconv(.c) void {
    _ = module_state;
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
    return state.update_count;
}

export fn NexoraGameModuleElapsedSeconds() f64 {
    return state.elapsed_seconds;
}

export fn NexoraGameModuleFixedUpdateCount() u32 {
    return state.fixed_update_count;
}

export fn NexoraGameModuleStartCount() u32 {
    return state.start_count;
}
