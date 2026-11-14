const nx = @import("nexora");
pub const Game = struct {
    stage: u32 = 0,
    seconds: f64 = 0,
    request: u64 = 0,
    pub fn start(_: *@This(), context: *const nx.game.Context) !void {
        const scenes = try context.scenes();
        const info = try scenes.info(try scenes.active());
        if (info.asset_high != 1 or info.asset_low != 2) return error.WrongStartup;
        context.log("packaged sprite fixture ready", .{});
    }
    pub fn update(self: *@This(), context: *const nx.game.Context, delta: f64) !void {
        const scenes = try context.scenes();
        if (self.request != 0) {
            const result = try scenes.status(self.request);
            if (result.state == @intFromEnum(nx.scene.State.failed)) return error.SceneRejected;
            if (result.state != @intFromEnum(nx.scene.State.applied)) return;
            self.request = 0;
            self.seconds = 0;
            self.stage += 1;
            context.log("packaged sprite fixture applied", .{});
        }
        if (self.stage >= 3) return;
        self.seconds += delta;
        if (self.seconds >= 4) {
            self.request = try scenes.loadPath(
                if (self.stage < 2) "Title.scene" else "Level.scene",
                if (self.stage == 0) .additive else .replace,
                false,
            );
        }
    }
    pub fn stop(_: *@This(), context: *const nx.game.Context) void {
        context.log("packaged sprite fixture stopped", .{});
    }
};
