const nx = @import("nexora");
const Base = @import("game.zig").SceneGame(1, 2);
// Keep each camera visible long enough for an external pixel observer, independent of WSI FPS.
pub const Game = struct {
    inner: Base = .{},
    elapsed: f64 = 0,
    pub fn start(self: *@This(), context: *const nx.game.Context) !void {
        try self.inner.start(context);
    }
    pub fn update(self: *@This(), context: *const nx.game.Context, delta: f64) !void {
        self.elapsed += delta;
        if (self.elapsed < 3.0) return;
        self.elapsed = 0;
        try self.inner.update(context, delta);
    }
    pub fn stop(self: *@This(), context: *const nx.game.Context) void {
        self.inner.stop(context);
    }
};
