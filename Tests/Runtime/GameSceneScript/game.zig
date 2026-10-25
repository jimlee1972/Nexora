const nx = @import("nexora");
pub const Game = struct {
    ticks: u32 = 0,
    stage: u32 = 0,
    request: u64 = 0,
    pub fn start(_: *Game, context: *const nx.game.Context) !void {
        const scenes = try context.scenes();
        const info = try scenes.info(try scenes.active());
        if (info.asset_high != 1 or info.asset_low != 2) return error.InvalidArgument;
        context.log("scene fixture ready", .{});
    }
    pub fn update(self: *Game, context: *const nx.game.Context, _: f64) !void {
        const scenes = try context.scenes();
        if (self.request != 0) {
            const status = try scenes.status(self.request);
            if (status.state == @intFromEnum(nx.scene.State.failed)) return error.SceneFailed;
            if (status.state != @intFromEnum(nx.scene.State.applied)) return;
            self.request = 0;
            self.stage += 1;
            self.ticks = 0;
            context.log("scene fixture applied", .{});
        }
        if (self.stage >= 2) return;
        self.ticks += 1;
        if (self.ticks == 30) self.request = try scenes.loadPath(if (self.stage == 0) "Title.scene" else "Level01.scene", .replace, false);
    }
    pub fn stop(_: *Game, context: *const nx.game.Context) void {
        const scenes = context.scenes() catch return;
        _ = scenes.active() catch return;
        context.log("scene fixture stopped", .{});
    }
};
