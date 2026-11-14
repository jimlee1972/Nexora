const nx = @import("nexora");
const builtin = @import("builtin");

pub const Mode = enum { normal, prepare_fail, restore_fail, reloaded_fail, schema_change };
fn check(result: i32) !void {
    if (result != 0) return error.HostFailure;
}

// Real SDK fixture for the isolated TwoD generation transaction. Production hosts own admission.
var job_state: ?*anyopaque = null;
var job_call: ?*const fn (?*anyopaque) void = null;
var final_storage: nx.storage.Api = .{};
fn finalize() callconv(.c) void {
    if (final_storage.remove) |remove| _ = remove(final_storage.context, "generation-guard", 16);
}
var linux_finalizer: *const fn () callconv(.c) void linksection(".fini_array") = finalize;
comptime {
    if (builtin.os.tag == .linux) @export(&linux_finalizer, .{ .name = "NexoraSourceReloadFinalizer" });
}
extern var __dso_handle: u8;
extern fn __cxa_atexit(callback: *const fn (?*anyopaque) callconv(.c) void, argument: ?*anyopaque, image: ?*anyopaque) c_int;
var mac_finalizer_registered = false;
fn finalizeMac(_: ?*anyopaque) callconv(.c) void {
    finalize();
}
fn registerFinalizer() !void {
    if (builtin.os.tag == .macos and !builtin.is_test and !mac_finalizer_registered) {
        // Use the same image-scoped CRT registration as native C++ destructors. A raw
        // __mod_term_func section was present but never called by the hosted dyld path.
        if (__cxa_atexit(finalizeMac, null, @ptrCast(&__dso_handle)) != 0) return error.FinalizerRegistration;
        mac_finalizer_registered = true;
    }
}

// A host work lease retains this exact library until the call returns.
export fn NexoraSourceReloadFixtureJob() callconv(.c) void {
    if (job_call) |call| call(job_state);
}

pub fn Script(comptime mode: Mode) type {
    return struct {
        const Self = @This();
        pub const state_version: u32 = if (mode == .schema_change) 2 else 1;
        pub const reload_lifecycle = true;
        host: nx.GameplayHostV3 = undefined,
        twod: nx.TwoDApiV1 = .{},
        sources: nx.TwoDSourceApiV1 = .{},
        scene: u64 = 0,
        entity: u64 = 0,
        counter_entity: u64 = 0,
        handle: nx.TwoDInstanceHandle = .{},
        frames: u64 = 0,
        ticks: u64 = 0,
        jobs: u64 = 0,
        external_input: nx.input.Api = .{},
        external_storage: nx.storage.Api = .{},
        external_audio: nx.audio.Api = .{},
        external_display: nx.display.Api = .{},
        external_ui: nx.ui.Api = .{},

        fn bind(self: *Self, context: *const nx.game.Context) !void {
            job_state = @ptrCast(self);
            job_call = job;
            self.host = context.host;
            self.twod = nx.getTwoD(&context.host) orelse return error.Unsupported;
            self.sources = nx.getTwoDSources(self.twod) orelse return error.Unsupported;
            // Optional: the original pure-TwoD owner supplies no services. The production
            // Player test supplies real/counted services and checks cached-context quarantine.
            if (nx.getServices(&context.host)) |services| {
                if (services.get_input_api) |get| _ = get(services.context, &self.external_input);
                if (services.get_storage_api) |get| _ = get(services.context, &self.external_storage);
                if (services.get_audio_api) |get| _ = get(services.context, &self.external_audio);
                if (services.get_display_api) |get| _ = get(services.context, &self.external_display);
                if (services.get_ui_api) |get| _ = get(services.context, &self.external_ui);
                final_storage = self.external_storage;
            }
            try registerFinalizer();
        }
        fn probeExternal(self: *const Self, expected: ?i32) !void {
            var results: [5]i32 = @splat(-2);
            if (self.external_input.clear_bindings) |call| results[0] = call(self.external_input.context, 0, 250);
            if (self.external_storage.remove) |call| results[1] = call(self.external_storage.context, "generation-guard", 16);
            if (self.external_audio.stop_all) |call| results[2] = call(self.external_audio.context);
            if (self.external_display.request_window) |call| results[3] = call(self.external_display.context, &.{ .width = 640, .height = 480 });
            if (self.external_ui.begin_frame) |call| results[4] = call(self.external_ui.context, 640, 480);
            if (expected) |value| for (results) |result| {
                if (result != value) return error.ExternalServiceEscaped;
            };
        }
        fn instance() nx.TwoDSourceInstanceV1 {
            return .{
                .transform = .{ .position = .{ .x = 3, .y = 2 }, .scale = .{ .x = -1, .y = 1, .z = 1 } },
                .sprite = .{ .source_high = 11, .source_low = 12 },
            };
        }
        pub fn init(self: *Self, context: *const nx.game.Context) !void {
            try self.bind(context);
        }
        pub fn start(self: *Self, _: *const nx.game.Context) !void {
            try check(self.host.load_scene.?(self.host.context, "Main", 4, 0, &self.scene));
            try check(self.host.activate_scene.?(self.host.context, self.scene));
            const descriptor = nx.EntitySpawnDescriptor{ .position = .{ .x = -2, .y = 2 } };
            try check(self.host.spawn_entity.?(self.host.context, self.scene, &descriptor, &self.entity));
            try check(self.host.spawn_entity.?(self.host.context, self.scene, &.{}, &self.counter_entity));
            const sprite = nx.TwoDSourceSpriteV1{ .source_high = 11, .source_low = 12 };
            try check(self.sources.set_sprite.?(self.sources.context, self.entity, &sprite));
            const instances = [_]nx.TwoDSourceInstanceV1{instance()};
            var handles: [1]nx.TwoDInstanceHandle = undefined;
            try check(self.sources.create_instances.?(self.sources.context, &instances, 1, &handles));
            self.handle = handles[0];
            try check(self.twod.play_animation.?(self.twod.context, self.entity, 15, 16, 1));
            try check(self.twod.set_camera.?(self.twod.context, &.{ .vertical_size = 10 }));
        }
        pub fn prepareReload(self: *Self, context: *const nx.game.Context) !void {
            try self.bind(context);
            try self.probeExternal(-2);
            if (mode == .prepare_fail) {
                try check(self.twod.set_camera.?(self.twod.context, &.{ .vertical_size = 17 }));
                const instances = [_]nx.TwoDSourceInstanceV1{instance()};
                var handles: [1]nx.TwoDInstanceHandle = undefined;
                try check(self.sources.create_instances.?(self.sources.context, &instances, 1, &handles));
                job_state = null;
                job_call = null;
                return error.InjectedPreparation;
            }
        }
        fn mutateCandidate(self: *Self) !void {
            try check(self.twod.set_camera.?(self.twod.context, &.{ .vertical_size = 17 }));
            const sprite = nx.TwoDSourceSpriteV1{ .source_high = 13, .source_low = 14 };
            try check(self.sources.set_sprite.?(self.sources.context, self.entity, &sprite));
            const handles = [_]nx.TwoDInstanceHandle{self.handle};
            try check(self.twod.destroy_instances.?(self.twod.context, &handles, 1));
            const position = nx.Vec3{ .x = 999 };
            try check(self.host.write_component.?(self.host.context, self.counter_entity, nx.componentType("Nexora.Transform"), &position, @sizeOf(nx.Vec3)));
        }
        fn verify(self: *Self) !void {
            const expected: u64 = if (self.ticks % 6 < 2) 11 else 13;
            var sprite = nx.TwoDSourceSpriteV1{};
            try check(self.sources.get_sprite.?(self.sources.context, self.entity, &sprite));
            if (sprite.source_high != expected or sprite.source_low != expected + 1)
                return error.SourceFrameLost;
            const handles = [_]nx.TwoDInstanceHandle{self.handle};
            var instances: [1]nx.TwoDSourceInstanceV1 = undefined;
            try check(self.sources.read_instances.?(self.sources.context, &handles, 1, &instances));
            if (instances[0].transform.position.x != 3 or instances[0].transform.position.y != 2 or
                instances[0].transform.scale.x != -1)
                return error.PoolHandleOrAnchorLost;
            var position = nx.Vec3{};
            try check(self.host.read_component.?(self.host.context, self.counter_entity, nx.componentType("Nexora.Transform"), &position, @sizeOf(nx.Vec3)));
            if (position.x != @as(f64, @floatFromInt(self.frames))) return error.CounterLost;
        }
        pub fn reloaded(self: *Self, _: *const nx.game.Context) !void {
            if (mode == .reloaded_fail) {
                try self.mutateCandidate();
                return error.InjectedReloaded;
            }
            try self.verify();
        }
        pub fn update(self: *Self, _: *const nx.game.Context, _: f64) !void {
            self.frames += 1;
            const paused = self.frames % 9 == 0;
            try check(self.twod.pause_animation.?(self.twod.context, self.entity, @intFromBool(paused)));
            var events: [2]nx.TwoDEvent = undefined;
            var result = nx.TwoDAnimationResult{};
            try check(self.twod.advance_animation.?(self.twod.context, self.entity, 0.125, &events, events.len, &result));
            if (!paused) self.ticks += 1;
            if (result.region != 0) return error.RegionModeChanged;
            var sprite = nx.TwoDSourceSpriteV1{};
            try check(self.sources.get_sprite.?(self.sources.context, self.entity, &sprite));
            sprite.sprite.flags = 12 | @as(u32, @intCast(self.frames % 4));
            sprite.sprite.order = 7;
            try check(self.sources.set_sprite.?(self.sources.context, self.entity, &sprite));
            const handles = [_]nx.TwoDInstanceHandle{self.handle};
            var instances: [1]nx.TwoDSourceInstanceV1 = undefined;
            try check(self.sources.read_instances.?(self.sources.context, &handles, 1, &instances));
            instances[0].sprite = sprite;
            try check(self.sources.write_instances.?(self.sources.context, &handles, 1, &instances));
            const position = nx.Vec3{ .x = @floatFromInt(self.frames), .y = @floatFromInt(self.jobs) };
            try check(self.host.write_component.?(self.host.context, self.counter_entity, nx.componentType("Nexora.Transform"), &position, @sizeOf(nx.Vec3)));
            try self.verify();
        }
        pub fn saveState(self: *const Self, writer: *nx.game.StateWriter) !void {
            if (nx.getServices(&self.host) != null) {
                try self.probeExternal(-2);
                const position = nx.Vec3{ .x = 999 };
                if (self.host.write_component.?(self.host.context, self.counter_entity, nx.componentType("Nexora.Transform"), &position, @sizeOf(nx.Vec3)) != -2 or
                    self.twod.set_camera.?(self.twod.context, &.{ .vertical_size = 17 }) != -2) return error.SaveMutatedLiveWorld;
                const handles = [_]nx.TwoDInstanceHandle{self.handle};
                const instances = [_]nx.TwoDSourceInstanceV1{instance()};
                if (self.sources.write_instances.?(self.sources.context, &handles, 1, &instances) != -2) return error.SaveMutatedLivePool;
            }
            try writer.int(u64, self.frames);
            try writer.int(u64, self.ticks);
            try writer.int(u64, self.jobs);
            try writer.int(u64, self.scene);
            try writer.int(u64, self.entity);
            try writer.int(u64, self.counter_entity);
            try writer.int(u64, self.handle.owner);
            try writer.int(u32, self.handle.index);
            try writer.int(u32, self.handle.generation);
        }
        pub fn loadState(self: *Self, reader: *nx.game.StateReader) !void {
            self.frames = try reader.int(u64);
            self.ticks = try reader.int(u64);
            self.jobs = try reader.int(u64);
            self.scene = try reader.int(u64);
            self.entity = try reader.int(u64);
            self.counter_entity = try reader.int(u64);
            self.handle.owner = try reader.int(u64);
            self.handle.index = try reader.int(u32);
            self.handle.generation = try reader.int(u32);
            try reader.finish();
            if (self.scene == 0 or self.entity == 0 or self.counter_entity == 0 or
                self.handle.owner == 0 or self.handle.generation == 0 or self.ticks > self.frames)
                return error.InvalidRestoredValues;
            if (mode == .restore_fail) {
                try self.mutateCandidate();
                return error.InjectedRestore;
            }
        }
        fn job(pointer: ?*anyopaque) void {
            const self: *Self = @ptrCast(@alignCast(pointer.?));
            self.jobs += 100; // Module-only data; World/TwoD stay on their owning thread.
        }
        pub fn deinit(self: *Self, _: *const nx.game.Context) void {
            self.probeExternal(null) catch {};
            if (job_state == @as(?*anyopaque, @ptrCast(self))) {
                job_state = null;
                job_call = null;
            }
        }
        pub fn stop(self: *Self, _: *const nx.game.Context) void {
            self.probeExternal(null) catch {};
            const handles = [_]nx.TwoDInstanceHandle{self.handle};
            _ = self.twod.destroy_instances.?(self.twod.context, &handles, 1);
            _ = self.host.despawn_entity.?(self.host.context, self.entity);
            _ = self.host.despawn_entity.?(self.host.context, self.counter_entity);
        }
    };
}
