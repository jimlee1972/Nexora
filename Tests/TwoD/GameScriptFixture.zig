//! Ordinary authored script: no build file, exported symbols or C callbacks.
const std = @import("std");
const nx = @import("nexora");

pub const Game = struct {
    ticks: u64 = 0,
    pub fn fixedUpdate(self: *Game, _: *const nx.game.Context, _: f64) !void {
        self.ticks += 1;
    }
};

test "authored script tests run separately from the generated entry" {
    var game = Game{};
    const context: nx.game.Context = undefined;
    try game.fixedUpdate(&context, 1.0 / 60.0);
    try std.testing.expectEqual(@as(u64, 1), game.ticks);
}
