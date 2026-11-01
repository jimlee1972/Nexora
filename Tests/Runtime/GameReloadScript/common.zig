const nx = @import("nexora");

pub const Mode = enum { normal, restore_fail, init_fail, start_fail, schema_change };
var job_state: ?*anyopaque = null;
var job_call: ?*const fn (?*anyopaque) void = null;

// Test-only symbol: the host retains a work lease through this entire call. The native
// instance pointer is never encoded by saveState and is cleared before module destruction.
export fn NexoraReloadFixtureJob() callconv(.c) void {
    if (job_call) |call| call(job_state);
}

pub fn Script(comptime mode: Mode) type {
    return struct {
        const Self = @This();
        pub const state_version: u32 = if (mode == .schema_change) 2 else 1;
        counter: u64 = 0,
        scene: u64 = 0,
        entity: u64 = 0,

        pub fn init(self: *Self, context: *const nx.game.Context) !void {
            job_state = @ptrCast(self);
            job_call = job;
            if (mode == .init_fail or mode == .restore_fail) {
                // Inject partial World effects. The embedding transaction must discard them.
                try context.activateScene("Rejected");
            }
            if (mode == .init_fail) {
                job_state = null;
                job_call = null;
                return error.InjectedInitialization;
            }
        }

        fn job(pointer: ?*anyopaque) void {
            const self: *Self = @ptrCast(@alignCast(pointer.?));
            self.counter += 100;
        }

        fn write(self: *const Self, context: *const nx.game.Context) !void {
            const callback = context.host.write_component orelse return error.Unsupported;
            const position = nx.Vec3{ .x = @floatFromInt(self.counter) };
            if (callback(context.host.context, self.entity, nx.componentType("Nexora.Transform"), &position, @sizeOf(nx.Vec3)) != 0) return error.HostFailure;
        }

        pub fn start(self: *Self, context: *const nx.game.Context) !void {
            const host = &context.host;
            if (self.scene == 0) {
                const name = "Reload";
                const load = host.load_scene orelse return error.Unsupported;
                const activate = host.activate_scene orelse return error.Unsupported;
                const spawn = host.spawn_entity orelse return error.Unsupported;
                const descriptor = nx.EntitySpawnDescriptor{};
                if (load(host.context, name.ptr, name.len, 1, &self.scene) != 0 or
                    activate(host.context, self.scene) != 0 or
                    spawn(host.context, self.scene, &descriptor, &self.entity) != 0)
                    return error.HostFailure;
            }
            var position = nx.Vec3{};
            const read = host.read_component orelse return error.Unsupported;
            if (read(host.context, self.entity, nx.componentType("Nexora.Transform"), &position, @sizeOf(nx.Vec3)) != 0) return error.HostFailure;
            if (mode == .start_fail) {
                const destroy = host.despawn_entity orelse return error.Unsupported;
                if (destroy(host.context, self.entity) != 0) return error.HostFailure;
                return error.InjectedStart;
            }
            try self.write(context);
        }

        pub fn update(self: *Self, context: *const nx.game.Context, _: f64) !void {
            self.counter += 1;
            try self.write(context);
        }

        pub fn stop(self: *Self, context: *const nx.game.Context) void {
            if (context.host.despawn_entity) |destroy|
                _ = destroy(context.host.context, self.entity);
        }

        pub fn deinit(self: *Self, _: *const nx.game.Context) void {
            if (job_state == @as(?*anyopaque, @ptrCast(self))) {
                job_state = null;
                job_call = null;
            }
        }

        pub fn saveState(self: *const Self, writer: *nx.game.StateWriter) !void {
            try writer.int(u64, self.counter);
            try writer.int(u64, self.scene);
            try writer.int(u64, self.entity);
        }

        pub fn loadState(self: *Self, reader: *nx.game.StateReader) !void {
            if (mode == .restore_fail) return error.InjectedRestore;
            const counter = try reader.int(u64);
            const scene = try reader.int(u64);
            const entity = try reader.int(u64);
            if (scene == 0 or entity == 0) return error.InvalidArgument;
            try reader.finish();
            self.counter = counter;
            self.scene = scene;
            self.entity = entity;
        }
    };
}
