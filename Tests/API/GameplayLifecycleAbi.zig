const std = @import("std");
const life = @import("lifecycle");
const c = @cImport({
    @cInclude("nexora/gameplay_lifecycle.h");
});
test "lifecycle C/Zig wire and unsupported preservation" {
    inline for (.{ .{ life.State, c.NexoraLifecycleStateV1 }, .{ life.Api, c.NexoraGameplayLifecycleApiV1 } }) |pair| {
        try std.testing.expectEqual(@sizeOf(pair[1]), @sizeOf(pair[0]));
        try std.testing.expectEqual(@alignOf(pair[1]), @alignOf(pair[0]));
        inline for (@typeInfo(pair[0]).@"struct".fields) |f| try std.testing.expectEqual(@offsetOf(pair[1], f.name), @offsetOf(pair[0], f.name));
    }
    const api = life.Api{};
    var state = life.State{ .sequence = 123 };
    try std.testing.expectEqual(@as(i32, -2), api.state(&state));
    try std.testing.expectEqual(@as(u64, 123), state.sequence);
    try std.testing.expectEqual(@as(i32, -2), api.guardClose(true));
    try std.testing.expectEqual(@as(i32, -2), api.resolveClose(1, true));
    try std.testing.expectEqual(@as(i32, -2), api.vibrate(0, 1, 1, 10));
}
test "pause gate retains drafts outside itself and rejects unsafe catchup" {
    var gate = life.PauseGate{};
    var state = life.State{};
    gate.observe(state);
    try std.testing.expect(gate.advance(0.016));
    state.interruption_sequence = 1; // Loss and recovery in the same pump.
    gate.observe(state);
    try std.testing.expect(gate.paused and gate.snapshot_due);
    gate.saved(false);
    try std.testing.expect(!gate.confirmResume());
    gate.saved(true);
    try std.testing.expect(gate.confirmResume());
    try std.testing.expect(!gate.advance(std.math.nan(f64)));
    try std.testing.expect(!gate.advance(-1));
    try std.testing.expect(!gate.advance(1000000));
    try std.testing.expect(gate.remaining >= 2.9);
    state.flags = 0;
    gate.observe(state);
    try std.testing.expect(gate.remaining == 0 and gate.paused);
    state.flags = 1;
    gate.observe(state);
    try std.testing.expect(!gate.advance(0.1));
    try std.testing.expect(gate.paused);
    try std.testing.expect(gate.confirmResume());
    for (0..31) |_| _ = gate.advance(0.1);
    try std.testing.expect(!gate.paused);
    gate.controlled_device = 99;
    gate.observe(state);
    try std.testing.expect(gate.paused and !gate.confirmResume());
}
