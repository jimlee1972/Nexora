const nx = @import("nexora");

fn check(code: i32) !void {
    if (code != 0) return error.HostFailure;
}

pub const Game = struct {
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
        self.twod = nx.getTwoD(&ctx.host) orelse return error.Unsupported;
        self.sources = nx.getTwoDSources(self.twod) orelse return error.Unsupported;
        self.scenes = try ctx.scenes();
        const ids = try self.collect(2);
        self.original = .{ ids[0], ids[1] };
        for (self.original) |id| {
            try check(self.twod.play_animation.?(self.twod.context, id, 105, 1, 1));
            try check(self.twod.pause_animation.?(self.twod.context, id, 1));
        }
        const values = [_]nx.TwoDSourceInstanceV1{.{
            .transform = .{ .position = .{ .y = -5 }, .scale = .{ .x = -1, .y = 1, .z = 1 } },
            .sprite = .{ .source_high = 102, .source_low = 1, .sprite = .{ .flags = 9 } },
        }};
        var handles: [1]nx.TwoDInstanceHandle = undefined;
        try check(self.sources.create_instances.?(self.sources.context, &values, 1, &handles));
        self.handle = handles[0];
        try self.verify();
        ctx.log("MAIN SOURCE PHASE 0 ids={d},{d} pool={d}:{d}:{d}", .{ ids[0], ids[1], self.handle.owner, self.handle.index, self.handle.generation });
    }
    pub fn update(self: *Game, ctx: *const nx.game.Context, _: f64) !void {
        self.ticks += 1;
        if (self.pending != 0) {
            const state = try self.scenes.status(self.pending);
            if (state.state == @intFromEnum(nx.scene.State.failed) or
                state.state == @intFromEnum(nx.scene.State.cancelled)) return error.SceneFailed;
            if (state.state == @intFromEnum(nx.scene.State.applied)) {
                self.pending = 0;
                self.phase = self.next_phase;
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
                    try check(self.twod.advance_animation.?(self.twod.context, id, 0.25, &events, 4, &result));
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
        const handles = [_]nx.TwoDInstanceHandle{self.handle};
        const result = self.twod.destroy_instances.?(self.twod.context, &handles, 1);
        ctx.log("MAIN SOURCE STOP ticks={d} phase={d} pool_destroy={d}", .{ self.ticks, self.phase, result });
    }
    pub fn deinit(self: *Game, ctx: *const nx.game.Context) void {
        ctx.log("MAIN SOURCE DESTROY ticks={d} phase={d}", .{ self.ticks, self.phase });
    }
};
