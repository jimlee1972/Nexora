const abi_version: u32 = 1;

const GameplayHost = extern struct {
    struct_size: u32,
    abi_version: u32,
    context: ?*anyopaque,
    log: ?*const fn (?*anyopaque, u32, [*]const u8, u32) callconv(.c) void,
};

const GameModule = extern struct {
    struct_size: u32,
    abi_version: u32,
    module_state: ?*anyopaque,
    initialize: ?*const fn (*?*anyopaque, *const GameplayHost) callconv(.c) i32,
    update: ?*const fn (?*anyopaque, f64) callconv(.c) void,
    shutdown: ?*const fn (?*anyopaque) callconv(.c) void,
};

const State = extern struct {
    update_count: u32 = 0,
    elapsed_seconds: f64 = 0,
};

var state = State{};

fn initialize(module_state: *?*anyopaque, host: *const GameplayHost) callconv(.c) i32 {
    if (host.abi_version != abi_version or host.struct_size < @sizeOf(GameplayHost)) return -1;
    state = State{};
    module_state.* = @ptrCast(&state);
    if (host.log) |log| {
        const message = "Zig gameplay initialized";
        log(host.context, 1, message.ptr, @intCast(message.len));
    }
    return 0;
}

fn update(module_state: ?*anyopaque, delta_seconds: f64) callconv(.c) void {
    const opaque_state = module_state orelse return;
    const current: *State = @ptrCast(@alignCast(opaque_state));
    current.update_count += 1;
    current.elapsed_seconds += delta_seconds;
}

fn shutdown(module_state: ?*anyopaque) callconv(.c) void {
    _ = module_state;
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
    };
    return 0;
}

export fn NexoraGameModuleUpdateCount() u32 {
    return state.update_count;
}

export fn NexoraGameModuleElapsedSeconds() f64 {
    return state.elapsed_seconds;
}
