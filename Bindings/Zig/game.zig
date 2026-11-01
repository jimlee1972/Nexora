//! Typed gameplay lifecycle facade. The engine owns ABI wiring and instance memory.
const std = @import("std");
const nx = @import("nexora.zig");

/// Explicit data encoding for hot reload. Never serialize Game/Context memory or native pointers.
pub const maximum_state_bytes: usize = 1024 * 1024;
const state_header_bytes: usize = 16;

pub const StateWriter = struct {
    destination: ?[]u8 = null,
    written: usize = 0,

    fn raw(self: *StateWriter, value: []const u8) !void {
        if (self.written > maximum_state_bytes or value.len > maximum_state_bytes - self.written)
            return error.InvalidArgument;
        const end = self.written + value.len;
        if (self.destination) |output| {
            if (end > output.len) return error.InvalidArgument;
            @memcpy(output[self.written..end], value);
        }
        self.written = end;
    }

    pub fn int(self: *StateWriter, comptime T: type, value: T) !void {
        comptime validateInteger(T);
        var encoded: [@sizeOf(T)]u8 = undefined;
        std.mem.writeInt(T, &encoded, value, .little);
        try self.raw(&encoded);
    }

    pub fn float(self: *StateWriter, value: f64) !void {
        if (!std.math.isFinite(value)) return error.InvalidArgument;
        try self.int(u64, @bitCast(value));
    }

    pub fn boolean(self: *StateWriter, value: bool) !void {
        try self.int(u8, if (value) 1 else 0);
    }

    pub fn bytes(self: *StateWriter, value: []const u8) !void {
        if (value.len > maximum_state_bytes) return error.InvalidArgument;
        try self.int(u32, @intCast(value.len));
        try self.raw(value);
    }
};

pub const StateReader = struct {
    source: []const u8,
    consumed: usize = 0,

    fn raw(self: *StateReader, length: usize) ![]const u8 {
        if (self.source.len > maximum_state_bytes or self.consumed > self.source.len or
            length > self.source.len - self.consumed) return error.InvalidArgument;
        const value = self.source[self.consumed..][0..length];
        self.consumed += length;
        return value;
    }

    pub fn int(self: *StateReader, comptime T: type) !T {
        comptime validateInteger(T);
        const value = try self.raw(@sizeOf(T));
        return std.mem.readInt(T, value[0..@sizeOf(T)], .little);
    }

    pub fn float(self: *StateReader) !f64 {
        const value: f64 = @bitCast(try self.int(u64));
        if (!std.math.isFinite(value)) return error.InvalidArgument;
        return value;
    }

    pub fn boolean(self: *StateReader) !bool {
        return switch (try self.int(u8)) {
            0 => false,
            1 => true,
            else => error.InvalidArgument,
        };
    }

    /// Borrowed until loadState returns. Copy into owned fields instead of retaining the slice.
    pub fn bytes(self: *StateReader) ![]const u8 {
        const length = try self.int(u32);
        return self.raw(length);
    }

    pub fn finish(self: *const StateReader) !void {
        if (self.consumed != self.source.len) return error.InvalidArgument;
    }
};

fn validateInteger(comptime T: type) void {
    switch (@typeInfo(T)) {
        .int => |info| if (info.bits != 8 and info.bits != 16 and info.bits != 32 and info.bits != 64)
            @compileError("State integers must have 8, 16, 32 or 64 bits"),
        else => @compileError("State encoding accepts integer values, never native pointers"),
    }
}

pub const Context = struct {
    host: nx.GameplayHostV3,

    pub fn isReloadCandidate(self: *const Context) bool {
        return (self.host.capabilities & nx.reload_candidate_capability) != 0;
    }

    pub fn services(self: *const Context) !nx.GameplayServicesApiV1 {
        return nx.getServices(&self.host) orelse error.Unsupported;
    }
    pub fn effects(self: *const Context) !nx.twod_effects.Api {
        return nx.getTwoDEffects(try self.services()) orelse error.Unsupported;
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
    const migration = @hasDecl(Game, "saveState") and @hasDecl(Game, "loadState");
    if (@hasDecl(Game, "saveState") != @hasDecl(Game, "loadState"))
        @compileError("Game must provide both saveState and loadState, or neither");
    if (migration and (!@hasDecl(Game, "state_version") or Game.state_version == 0))
        @compileError("Migrating Game must declare a nonzero u32 state_version");
    const reload_lifecycle = @hasDecl(Game, "reload_lifecycle") and Game.reload_lifecycle;
    if (reload_lifecycle and !migration)
        @compileError("Reload lifecycle requires paired saveState/loadState hooks");
    return struct {
        const Instance = struct {
            context: Context,
            game: Game = .{},
            initialized: bool = false,
            restored: bool = false,
        };
        const owner_tag = @intFromEnum(nx.AllocationOwner.gameplay_state);

        fn instance(pointer: ?*anyopaque) *Instance {
            // Only the host calls these callbacks after successful create and before destroy.
            return @ptrCast(@alignCast(pointer.?));
        }

        fn create(out: *?*anyopaque, host: *const nx.GameplayHostV3) callconv(.c) i32 {
            out.* = null;
            if (host.struct_size < nx.host_v3_base_size or host.abi_version != nx.abi_version) return -2;
            if ((host.capabilities & nx.reload_candidate_capability) != 0 and !reload_lifecycle) return -2;
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
            if (state.context.isReloadCandidate()) {
                if (@hasDecl(Game, "prepareReload")) {
                    state.game.prepareReload(&state.context) catch |err| {
                        deallocate(host.context, owner_tag, memory, @sizeOf(Instance), @alignOf(Instance));
                        return resultCode(err);
                    };
                    state.initialized = true;
                }
            } else {
                if (@hasDecl(Game, "init")) {
                    state.game.init(&state.context) catch |err| {
                        deallocate(host.context, owner_tag, memory, @sizeOf(Instance), @alignOf(Instance));
                        return resultCode(err);
                    };
                }
                state.initialized = true;
            }
            out.* = memory;
            return 0;
        }

        fn start(pointer: ?*anyopaque) callconv(.c) i32 {
            const state = instance(pointer);
            if (state.context.isReloadCandidate()) {
                if (!state.restored) return -3;
                if (@hasDecl(Game, "reloaded")) state.game.reloaded(&state.context) catch |err| return resultCode(err);
                return 0;
            }
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
            if (@hasDecl(Game, "stop")) {
                if (state.initialized) state.game.stop(&state.context);
            }
        }

        fn destroy(pointer: ?*anyopaque) callconv(.c) void {
            const state = instance(pointer);
            if (@hasDecl(Game, "deinit")) {
                if (state.initialized) state.game.deinit(&state.context);
            }
            const host = state.context.host;
            // create requires this callback; use the same allocator, tag, size and alignment.
            host.deallocate.?(host.context, owner_tag, state, @sizeOf(Instance), @alignOf(Instance));
        }

        fn saveState(pointer: ?*anyopaque, data: ?*anyopaque, size: u32) callconv(.c) u32 {
            if (!migration) return 0;
            const state = instance(pointer);
            var writer = StateWriter{ .destination = if (data) |output|
                @as([*]u8, @ptrCast(output))[0..size]
            else
                null };
            writer.raw("NXGS") catch return 0;
            writer.int(u32, 1) catch return 0;
            writer.int(u32, Game.state_version) catch return 0;
            writer.int(u32, 0) catch return 0;
            const game: *const Game = &state.game;
            game.saveState(&writer) catch return 0;
            if (writer.destination) |output| {
                std.mem.writeInt(u32, output[12..16], @intCast(writer.written - state_header_bytes), .little);
            }
            return @intCast(writer.written);
        }

        fn loadState(pointer: ?*anyopaque, data: ?*const anyopaque, size: u32) callconv(.c) i32 {
            if (!migration) return -2;
            const state = instance(pointer);
            state.restored = false;
            if (data == null or size < state_header_bytes or size > maximum_state_bytes) return -1;
            var reader = StateReader{ .source = @as([*]const u8, @ptrCast(data.?))[0..size] };
            const magic = reader.raw(4) catch return -1;
            if (!std.mem.eql(u8, magic, "NXGS")) return -1;
            if ((reader.int(u32) catch return -1) != 1 or
                (reader.int(u32) catch return -1) != Game.state_version) return -2;
            if ((reader.int(u32) catch return -1) != size - state_header_bytes) return -1;
            state.game.loadState(&reader) catch |err| return resultCode(err);
            // A successful hook owns its loaded fields even if trailing-data admission fails.
            state.initialized = true;
            reader.finish() catch return -1;
            state.restored = true;
            return 0;
        }

        pub fn load(abi: u32, output: ?*nx.GameModuleV3) i32 {
            const module = output orelse return -1;
            if (abi != nx.abi_version or module.struct_size < @sizeOf(nx.GameModuleV3)) return -2;
            module.* = .{
                .struct_size = @sizeOf(nx.GameModuleV3),
                .abi_version = nx.abi_version,
                .capabilities = (if (@hasDecl(Game, "fixedUpdate")) @as(u64, 1 << 1) else 0) |
                    (if (migration) @as(u64, 1) else 0) |
                    (if (reload_lifecycle) nx.reload_lifecycle_capability else 0),
                .module_state = null,
                .create = create,
                .on_start = start,
                .fixed_update = if (@hasDecl(Game, "fixedUpdate")) fixedUpdate else null,
                .update = update,
                .on_stop = stop,
                .destroy = destroy,
                .save_state = if (migration) saveState else null,
                .load_state = if (migration) loadState else null,
            };
            return 0;
        }
    };
}
