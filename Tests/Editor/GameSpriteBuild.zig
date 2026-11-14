const nx = @import("nexora");

pub const Game = struct {
    ticks: u64 = 0,
    pub fn start(_: *@This(), context: *const nx.game.Context) !void {
        const scenes = try context.scenes();
        _ = try scenes.info(try scenes.active());
        context.log("SPRITE BUILD START", .{});
    }
    pub fn update(self: *@This(), context: *const nx.game.Context, _: f64) !void {
        self.ticks += 1;
        if (self.ticks == 1) context.log("SPRITE BUILD UPDATE", .{});
    }
    pub fn stop(self: *@This(), context: *const nx.game.Context) void {
        context.log("SPRITE BUILD STOP ticks={d}", .{self.ticks});
    }
    pub fn deinit(self: *@This(), context: *const nx.game.Context) void {
        context.log("SPRITE BUILD DESTROY ticks={d}", .{self.ticks});
    }
};
