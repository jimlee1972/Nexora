const nx = @import("nexora");
pub const Game = SceneGame(30, 2);
pub fn SceneGame(comptime interval: u32, comptime transitions: u32) type {
    return struct {
        const Self = @This();
        ticks: u32 = 0,
        stage: u32 = 0,
        request: u64 = 0,
        initial: u64 = 0,
        initial_camera: u64 = 0,
        pub fn start(self: *Self, context: *const nx.game.Context) !void {
            const scenes = try context.scenes();
            const info = try scenes.info(try scenes.active());
            if (info.asset_high != 1 or info.asset_low != 2) return error.InvalidArgument;
            self.initial = info.scene;
            self.initial_camera = info.camera;
            context.log("scene fixture ready", .{});
        }
        pub fn update(self: *Self, context: *const nx.game.Context, _: f64) !void {
            const scenes = try context.scenes();
            if (self.request != 0) {
                const status = try scenes.status(self.request);
                if (status.state == @intFromEnum(nx.scene.State.failed)) {
                    const retained = try scenes.info(try scenes.active());
                    if (self.stage != 0 or retained.scene != self.initial or retained.camera != self.initial_camera or retained.asset_low != 2)
                        return error.FailureChangedLiveScene;
                    self.stage = 1;
                    self.request = 0;
                    self.ticks = 0;
                    context.log("scene fixture rejected with startup/camera retained", .{});
                } else {
                    if (status.state != @intFromEnum(nx.scene.State.applied)) return;
                    self.request = 0;
                    self.stage += 1;
                    self.ticks = 0;
                    context.log("scene fixture applied", .{});
                }
            }
            if (self.stage >= transitions) return;
            self.ticks += 1;
            if (self.ticks == interval) self.request = try scenes.loadPath(if (self.stage % 2 == 0) "Title.scene" else "Level01.scene", .replace, false);
        }
        pub fn stop(_: *Self, context: *const nx.game.Context) void {
            const scenes = context.scenes() catch return;
            _ = scenes.active() catch return;
            context.log("scene fixture stopped", .{});
        }
    };
}
