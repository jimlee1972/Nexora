const nx = @import("nexora");
const builtin = @import("builtin");

// The native watcher fixture edits these saved constants; no host admission shortcut is used.
pub const fixture_generation: u32 = 0;
pub const fixture_fault: u32 = 0;
var final_context: ?nx.game.Context = null;
var final_sources: nx.TwoDSourceApiV1 = .{};
var final_entity: u64 = 0;
var final_handle: nx.TwoDInstanceHandle = .{};

fn probe(stage: []const u8) void {
    const ctx = final_context orelse return;
    var sprite = nx.TwoDSourceSpriteV1{};
    var entity_result: i32 = -2;
    if (final_sources.get_sprite.?(final_sources.context, final_entity, &sprite) == 0) {
        sprite.source_high = 999;
        entity_result = final_sources.set_sprite.?(final_sources.context, final_entity, &sprite);
    }
    const handles = [_]nx.TwoDInstanceHandle{final_handle};
    var values: [1]nx.TwoDSourceInstanceV1 = undefined;
    var pool_result: i32 = -2;
    if (final_sources.read_instances.?(final_sources.context, &handles, 1, &values) == 0) {
        values[0].sprite.source_high = 999;
        pool_result = final_sources.write_instances.?(final_sources.context, &handles, 1, &values);
    }
    const transform = nx.Vec3{ .x = 999 };
    const world_result = ctx.host.write_component.?(ctx.host.context, final_entity, nx.componentType("Nexora.Transform"), &transform, @sizeOf(nx.Vec3));
    ctx.log("MAIN SOURCE CALLBACK code={d} stage={s} entity={d} pool={d} world={d}", .{ fixture_generation, stage, entity_result, pool_result, world_result });
}
fn finalize() callconv(.c) void {
    probe("finalizer");
}
var linux_finalizer: *const fn () callconv(.c) void linksection(".fini_array") = finalize;
comptime {
    if (builtin.os.tag == .linux) @export(&linux_finalizer, .{ .name = "NexoraMainSourceFinalizer" });
}

fn check(code: i32) !void {
    if (code != 0) return error.HostFailure;
}

pub const Game = struct {
    pub const state_version: u32 = 1;
    pub const reload_lifecycle = true;
    twod: nx.TwoDApiV1 = .{},
    sources: nx.TwoDSourceApiV1 = .{},
    scenes: nx.scene.Api = .{},
    original: [2]u64 = .{ 0, 0 },
    handle: nx.TwoDInstanceHandle = .{},
    pending: u64 = 0,
    additive: u64 = 0,
    phase: u32 = 0,
    next_phase: u32 = 0,
    last_action: u32 = 0,
    ticks: u64 = 0,
    primary: u64 = 0,
    reloads: u32 = 0,
    reported_live: bool = false,

    fn bind(self: *Game, ctx: *const nx.game.Context) !void {
        self.twod = nx.getTwoD(&ctx.host) orelse return error.Unsupported;
        self.sources = nx.getTwoDSources(self.twod) orelse return error.Unsupported;
        self.scenes = try ctx.scenes();
        final_context = ctx.*;
        final_sources = self.sources;
    }
    fn remember(self: *const Game) void {
        final_entity = self.original[0];
        final_handle = self.handle;
    }
    pub fn prepareReload(self: *Game, ctx: *const nx.game.Context) !void {
        try self.bind(ctx);
        if (fixture_fault == 1) return error.InjectedPrepare;
    }
    pub fn reloaded(self: *Game, ctx: *const nx.game.Context) !void {
        self.remember();
        if (try self.scenes.active() != self.primary) return error.SceneIdentity;
        try self.verify();
        if (fixture_fault == 3) return error.InjectedReloaded;
        if (fixture_fault == 6) {
            var value = nx.TwoDSourceSpriteV1{};
            try check(self.sources.get_sprite.?(self.sources.context, self.original[0], &value));
            value.source_high = 999;
            try check(self.sources.set_sprite.?(self.sources.context, self.original[0], &value));
        }
        if (fixture_fault == 7) {
            const handles = [_]nx.TwoDInstanceHandle{self.handle};
            var values: [1]nx.TwoDSourceInstanceV1 = undefined;
            try check(self.sources.read_instances.?(self.sources.context, &handles, 1, &values));
            values[0].sprite.source_high = 999;
            values[0].sprite.sprite.flags = 0;
            try check(self.sources.write_instances.?(self.sources.context, &handles, 1, &values));
        }
        self.reloads += 1;
        ctx.log("MAIN SOURCE CANDIDATE code={d} reloads={d} ticks={d}", .{ fixture_generation, self.reloads, self.ticks });
    }
    pub fn saveState(self: *const Game, writer: *nx.game.StateWriter) !void {
        // Only bounded scalar state is encoded, never service tables, pointers or raw Game bytes.
        if (fixture_fault == 4) probe("capture");
        for (self.original) |id| try writer.int(u64, id);
        try writer.int(u64, self.handle.owner);
        try writer.int(u32, self.handle.index);
        try writer.int(u32, self.handle.generation);
        try writer.int(u64, self.pending);
        try writer.int(u64, self.additive);
        try writer.int(u32, self.phase);
        try writer.int(u32, self.next_phase);
        try writer.int(u32, self.last_action);
        try writer.int(u64, self.ticks);
        try writer.int(u64, self.primary);
        try writer.int(u32, self.reloads);
    }
    pub fn loadState(self: *Game, reader: *nx.game.StateReader) !void {
        if (fixture_fault == 2) return error.InjectedRestore;
        for (&self.original) |*id| id.* = try reader.int(u64);
        self.handle.owner = try reader.int(u64);
        self.handle.index = try reader.int(u32);
        self.handle.generation = try reader.int(u32);
        self.pending = try reader.int(u64);
        self.additive = try reader.int(u64);
        self.phase = try reader.int(u32);
        self.next_phase = try reader.int(u32);
        self.last_action = try reader.int(u32);
        self.ticks = try reader.int(u64);
        self.primary = try reader.int(u64);
        self.reloads = try reader.int(u32);
        try reader.finish();
        if (self.original[0] == 0 or self.original[1] == 0 or self.original[0] == self.original[1] or
            self.handle.owner == 0 or self.handle.generation == 0 or self.primary == 0 or
            self.phase > 4 or self.next_phase > 4 or self.last_action > 4 or self.reloads >= 1024)
            return error.InvalidState;
    }

    // Fixture-only bounded probing observes actual live IDs, rather than assuming authored IDs
    // survived Runtime remapping. Production code must use its own live entity handles.
    fn collect(self: *Game, expected: usize) ![4]u64 {
        var result = [_]u64{0} ** 4;
        var count: usize = 0;
        for (1..129) |id| {
            var value = nx.TwoDSourceSpriteV1{};
            if (self.sources.get_sprite.?(self.sources.context, id, &value) == 0) {
                if (count == result.len or value.source_low != 1) return error.SourceScope;
                result[count] = id;
                count += 1;
            }
        }
        if (count != expected) return error.SourceCount;
        return result;
    }
    fn verify(self: *Game) !void {
        if (self.phase < 4) {
            for (self.original) |id| {
                var value = nx.TwoDSourceSpriteV1{};
                try check(self.sources.get_sprite.?(self.sources.context, id, &value));
                if (value.source_high != @as(u64, if (self.phase == 0) 102 else 103) or
                    value.source_low != 1 or value.sprite.flags != 8 or
                    value.sprite.color[0] != 1 or value.sprite.color[3] != 1) return error.LostSourceState;
                try check(self.twod.pause_animation.?(self.twod.context, id, 1));
            }
        }
        const handles = [_]nx.TwoDInstanceHandle{self.handle};
        var values: [1]nx.TwoDSourceInstanceV1 = undefined;
        try check(self.sources.read_instances.?(self.sources.context, &handles, 1, &values));
        if (values[0].sprite.source_high != @as(u64, if (self.phase == 0) 102 else 103) or
            values[0].sprite.source_low != 1 or values[0].sprite.sprite.flags != 9 or
            values[0].transform.scale.x != -1 or values[0].transform.position.y != -5)
            return error.LostPoolState;
    }
    pub fn start(self: *Game, ctx: *const nx.game.Context) !void {
        try self.bind(ctx);
        self.primary = try self.scenes.active();
        const ids = try self.collect(2);
        self.original = .{ ids[0], ids[1] };
        for (self.original) |id| {
            try check(self.twod.play_animation.?(self.twod.context, id, 105, 1, 1));
            var events: [4]nx.TwoDEvent = undefined;
            var result = nx.TwoDAnimationResult{};
            try check(self.twod.advance_animation.?(self.twod.context, id, 0.125, &events, 4, &result));
            try check(self.twod.pause_animation.?(self.twod.context, id, 1));
        }
        const values = [_]nx.TwoDSourceInstanceV1{.{
            .transform = .{ .position = .{ .y = -5 }, .scale = .{ .x = -1, .y = 1, .z = 1 } },
            .sprite = .{ .source_high = 102, .source_low = 1, .sprite = .{ .flags = 9 } },
        }};
        var handles: [1]nx.TwoDInstanceHandle = undefined;
        try check(self.sources.create_instances.?(self.sources.context, &values, 1, &handles));
        self.handle = handles[0];
        self.remember();
        try self.verify();
        ctx.log("MAIN SOURCE PHASE 0 ids={d},{d} pool={d}:{d}:{d}", .{ ids[0], ids[1], self.handle.owner, self.handle.index, self.handle.generation });
    }
    pub fn update(self: *Game, ctx: *const nx.game.Context, _: f64) !void {
        self.ticks += 1;
        if (!self.reported_live) {
            try self.verify();
            ctx.log("MAIN SOURCE LIVE code={d} reloads={d} ticks={d} scene={d} ids={d},{d} pool={d}:{d}:{d} phase={d}", .{ fixture_generation, self.reloads, self.ticks, self.primary, self.original[0], self.original[1], self.handle.owner, self.handle.index, self.handle.generation, self.phase });
            self.reported_live = true;
        }
        if (self.pending != 0) {
            const state = try self.scenes.status(self.pending);
            if (state.state == @intFromEnum(nx.scene.State.failed) or
                state.state == @intFromEnum(nx.scene.State.cancelled)) return error.SceneFailed;
            if (state.state == @intFromEnum(nx.scene.State.applied)) {
                self.pending = 0;
                self.phase = self.next_phase;
                self.primary = try self.scenes.active();
                if (self.phase == 2) self.additive = state.scene;
                _ = try self.collect(if (self.phase == 2) 4 else 2);
                if (self.phase == 4) {
                    for (self.original) |id| {
                        var old = nx.TwoDSourceSpriteV1{};
                        if (self.sources.get_sprite.?(self.sources.context, id, &old) != -1)
                            return error.RetiredEntity;
                    }
                }
                try self.verify();
                ctx.log("MAIN SOURCE PHASE {d} scene={d} pool={d}:{d}:{d}", .{ self.phase, state.scene, self.handle.owner, self.handle.index, self.handle.generation });
            }
        }
        try self.verify();
        var input = nx.InputSnapshot{};
        try check(ctx.host.capture_input.?(ctx.host.context, 0, &input));
        const action: u32 = if (input.move_x > 0.5) 1 else if (input.move_y > 0.5) 2 else if (input.move_y < -0.5) 3 else if (input.move_x < -0.5) 4 else 0;
        if (action != 0 and self.last_action == 0 and self.pending == 0 and action == self.phase + 1) {
            if (action == 1) {
                for (self.original) |id| {
                    var events: [4]nx.TwoDEvent = undefined;
                    var result = nx.TwoDAnimationResult{};
                    try check(self.twod.pause_animation.?(self.twod.context, id, 0));
                    try check(self.twod.advance_animation.?(self.twod.context, id, 0.125, &events, 4, &result));
                    if (result.event_count != 1) return error.SourceClock;
                    try check(self.twod.pause_animation.?(self.twod.context, id, 1));
                }
                const handles = [_]nx.TwoDInstanceHandle{self.handle};
                var values: [1]nx.TwoDSourceInstanceV1 = undefined;
                try check(self.sources.read_instances.?(self.sources.context, &handles, 1, &values));
                values[0].sprite.source_high = 103;
                try check(self.sources.write_instances.?(self.sources.context, &handles, 1, &values));
                self.phase = 1;
                try self.verify();
                ctx.log("MAIN SOURCE PHASE 1 animated pool={d}:{d}:{d}", .{ self.handle.owner, self.handle.index, self.handle.generation });
            } else {
                self.next_phase = action;
                self.pending = switch (action) {
                    2 => try self.scenes.loadPath("Level.scene", .additive, false),
                    3 => try self.scenes.unload(self.additive),
                    4 => try self.scenes.loadPath("Title.scene", .replace, false),
                    else => unreachable,
                };
            }
        }
        self.last_action = action;
    }
    pub fn stop(self: *Game, ctx: *const nx.game.Context) void {
        probe("stop");
        const handles = [_]nx.TwoDInstanceHandle{self.handle};
        const result = self.twod.destroy_instances.?(self.twod.context, &handles, 1);
        ctx.log("MAIN SOURCE STOP ticks={d} phase={d} pool_destroy={d}", .{ self.ticks, self.phase, result });
    }
    pub fn deinit(self: *Game, ctx: *const nx.game.Context) void {
        probe("destroy");
        ctx.log("MAIN SOURCE DESTROY ticks={d} phase={d}", .{ self.ticks, self.phase });
    }
};
