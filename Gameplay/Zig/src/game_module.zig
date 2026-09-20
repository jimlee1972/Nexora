const abi_version: u32 = 2;

const GameplayHost = extern struct {
    struct_size: u32,
    abi_version: u32,
    context: ?*anyopaque,
    log: ?*const fn (?*anyopaque, u32, [*]const u8, u32) callconv(.c) void,
    read_component: ?*const fn (?*anyopaque, u64, u64, ?*anyopaque, u32) callconv(.c) i32,
    write_component: ?*const fn (?*anyopaque, u64, u64, ?*const anyopaque, u32) callconv(.c) i32,
    subscribe_event: ?*const fn (?*anyopaque, u64) callconv(.c) i32,
    set_tick_enabled: ?*const fn (?*anyopaque, u32) callconv(.c) void,
};

const GameModule = extern struct {
    struct_size: u32,
    abi_version: u32,
    module_state: ?*anyopaque,
    initialize: ?*const fn (*?*anyopaque, *const GameplayHost) callconv(.c) i32,
    update: ?*const fn (?*anyopaque, f64) callconv(.c) void,
    shutdown: ?*const fn (?*anyopaque) callconv(.c) void,
    save_state: ?*const fn (?*anyopaque, ?*anyopaque, u32) callconv(.c) u32,
    load_state: ?*const fn (?*anyopaque, ?*const anyopaque, u32) callconv(.c) i32,
};

const State = extern struct {
    update_count: u32 = 0,
    elapsed_seconds: f64 = 0,
};

var state = State{};
var host_api: ?*const GameplayHost = null;

fn initialize(module_state: *?*anyopaque, host: *const GameplayHost) callconv(.c) i32 {
    if (host.abi_version != abi_version or host.struct_size < @sizeOf(GameplayHost)) return -1;
    state = State{};
    host_api = host;
    module_state.* = @ptrCast(&state);
    if (host.subscribe_event) |subscribe| {
        if (subscribe(host.context, 0x1001) != 0) return -2;
    }
    if (host.set_tick_enabled) |set_tick| set_tick(host.context, 1);
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

fn update(module_state: ?*anyopaque, delta_seconds: f64) callconv(.c) void {
    const opaque_state = module_state orelse return;
    const current: *State = @ptrCast(@alignCast(opaque_state));
    current.update_count += 1;
    current.elapsed_seconds += delta_seconds;
    if (host_api) |host| {
        var value: u32 = 0;
        if (host.read_component) |read| {
            if (read(host.context, 1, 0x2001, &value, @sizeOf(u32)) == 0) {
                value += 1;
                if (host.write_component) |write| {
                    _ = write(host.context, 1, 0x2001, &value, @sizeOf(u32));
                }
            }
        }
    }
}

fn shutdown(module_state: ?*anyopaque) callconv(.c) void {
    _ = module_state;
    host_api = null;
}

export fn NexoraGameModuleLoad(requested_abi: u32, module: ?*GameModule) i32 {
    if (requested_abi != abi_version) return -1;
    const output = module orelse return -2;
    if (output.struct_size < @sizeOf(GameModule)) return -3;
    output.* = .{
        .struct_size = @intCast(@sizeOf(GameModule)),
        .abi_version = abi_version,
        .module_state = null,
        .initialize = initialize,
        .update = update,
        .shutdown = shutdown,
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
