//! Typed gameplay lifecycle facade. The engine owns ABI wiring and instance memory.
const std = @import("std");
const nx = @import("nexora.zig");

pub const Context = struct {
    host: nx.GameplayHostV3,

    pub fn services(self: *const Context) !nx.GameplayServicesApiV1 {
        return nx.getServices(&self.host) orelse error.Unsupported;
    }
    pub fn scenes(self: *const Context) !nx.scene.Api {
        return nx.getScene(try self.services()) orelse error.Unsupported;
    }

    pub fn display(self: *const Context) !nx.display.Api {
        return nx.getDisplay(try self.services()) orelse error.Unsupported;
    }

    pub fn input(self: *const Context) !nx.input.Api {
        const api = try self.services();
        const get = api.get_input_api orelse return error.Unsupported;
        var result = nx.input.Api{};
        if (get(api.context, &result) != 0) return error.Unsupported;
        return result;
    }

    pub fn ui(self: *const Context) !nx.ui.Api {
        const api = try self.services();
        const get = api.get_ui_api orelse return error.Unsupported;
        var result = nx.ui.Api{};
        if (get(api.context, &result) != 0 or !result.valid()) return error.Unsupported;
        return result;
    }

    pub fn activateScene(self: *const Context, name: []const u8) !void {
        if (name.len > std.math.maxInt(u32)) return error.InvalidArgument;
        const load = self.host.load_scene orelse return error.Unsupported;
        const activate = self.host.activate_scene orelse return error.Unsupported;
        var scene: u64 = 0;
        if (load(self.host.context, name.ptr, @intCast(name.len), 1, &scene) != 0 or
            activate(self.host.context, scene) != 0) return error.HostFailure;
    }

    pub fn log(self: *const Context, comptime format: []const u8, args: anytype) void {
        const write = self.host.log orelse return;
        var buffer: [512]u8 = undefined;
        // Best-effort diagnostics never abort gameplay on a formatting overflow.
        const text = std.fmt.bufPrint(&buffer, format, args) catch return;
        write(self.host.context, 1, text.ptr, @intCast(text.len));
    }
};

fn resultCode(err: anyerror) i32 {
    return switch (err) {
        error.InvalidArgument => -1,
        error.Unsupported => -2,
        else => -3,
    };
}

/// Game must be default-initializable. All lifecycle methods are optional.
pub fn Module(comptime Game: type) type {
    return struct {
        const Instance = struct { context: Context, game: Game = .{} };
        const owner_tag = @intFromEnum(nx.AllocationOwner.gameplay_state);

        fn instance(pointer: ?*anyopaque) *Instance {
            // Only the host calls these callbacks after successful create and before destroy.
            return @ptrCast(@alignCast(pointer.?));
        }

        fn create(out: *?*anyopaque, host: *const nx.GameplayHostV3) callconv(.c) i32 {
            out.* = null;
            if (host.struct_size < nx.host_v3_base_size or host.abi_version != nx.abi_version) return -2;
            const allocate = host.allocate orelse return -2;
            const deallocate = host.deallocate orelse return -2;
            const memory = allocate(host.context, owner_tag, @sizeOf(Instance), @alignOf(Instance)) orelse return -3;
            // Host allocation honors the requested size and alignment.
            const state: *Instance = @ptrCast(@alignCast(memory));
            // Copy only the negotiated prefix; older hosts need not expose newer services.
            var copied_host = std.mem.zeroes(nx.GameplayHostV3);
            const host_bytes = @min(host.struct_size, @sizeOf(nx.GameplayHostV3));
            @memcpy(std.mem.asBytes(&copied_host)[0..host_bytes], std.mem.asBytes(host)[0..host_bytes]);
            state.* = .{ .context = .{ .host = copied_host } };
            if (@hasDecl(Game, "init")) {
                state.game.init(&state.context) catch |err| {
                    deallocate(host.context, owner_tag, memory, @sizeOf(Instance), @alignOf(Instance));
                    return resultCode(err);
                };
            }
            out.* = memory;
            return 0;
        }

        fn start(pointer: ?*anyopaque) callconv(.c) i32 {
            const state = instance(pointer);
            if (@hasDecl(Game, "start")) state.game.start(&state.context) catch |err| return resultCode(err);
            return 0;
        }

        fn update(pointer: ?*anyopaque, delta_seconds: f64) callconv(.c) i32 {
            const state = instance(pointer);
            if (@hasDecl(Game, "update")) state.game.update(&state.context, delta_seconds) catch |err| return resultCode(err);
            return 0;
        }

        fn fixedUpdate(pointer: ?*anyopaque, delta_seconds: f64) callconv(.c) i32 {
            const state = instance(pointer);
            if (@hasDecl(Game, "fixedUpdate")) state.game.fixedUpdate(&state.context, delta_seconds) catch |err| return resultCode(err);
            return 0;
        }

        fn stop(pointer: ?*anyopaque) callconv(.c) void {
            const state = instance(pointer);
            if (@hasDecl(Game, "stop")) state.game.stop(&state.context);
        }

        fn destroy(pointer: ?*anyopaque) callconv(.c) void {
            const state = instance(pointer);
            if (@hasDecl(Game, "deinit")) state.game.deinit(&state.context);
            const host = state.context.host;
            // create requires this callback; use the same allocator, tag, size and alignment.
            host.deallocate.?(host.context, owner_tag, state, @sizeOf(Instance), @alignOf(Instance));
        }

        pub fn load(abi: u32, output: ?*nx.GameModuleV3) i32 {
            const module = output orelse return -1;
            if (abi != nx.abi_version or module.struct_size < @sizeOf(nx.GameModuleV3)) return -2;
            module.* = .{
                .struct_size = @sizeOf(nx.GameModuleV3),
                .abi_version = nx.abi_version,
                .capabilities = if (@hasDecl(Game, "fixedUpdate")) 1 << 1 else 0,
                .module_state = null,
                .create = create,
                .on_start = start,
                .fixed_update = if (@hasDecl(Game, "fixedUpdate")) fixedUpdate else null,
                .update = update,
                .on_stop = stop,
                .destroy = destroy,
                .save_state = null,
                .load_state = null,
            };
            return 0;
        }
    };
}
