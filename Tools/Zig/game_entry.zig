//! Engine-owned entry; never copied into an authored game project.
const std = @import("std");
const builtin = @import("builtin");
// dyld retains images with TLS. The default panic recursion counter would pin every
// otherwise unloadable gameplay library. Keep safety checks and fatal panic behavior,
// but use the standard message-and-trap handler without TLS for macOS libraries.
pub const panic = if (builtin.os.tag == .macos and !builtin.is_test)
    std.debug.simple_panic
else
    std.debug.FullPanic(std.debug.defaultPanic);
const nx = @import("nexora");
const script = @import("game");
const Adapter = nx.game.Module(script.Game);

export fn NexoraGameModuleLoad(abi: u32, module: ?*nx.GameModuleV3) callconv(.c) i32 {
    return Adapter.load(abi, module);
}

test "SDK entry negotiates ABI and advertises only implemented fixed updates" {
    var module: nx.GameModuleV3 = undefined;
    module.struct_size = @sizeOf(nx.GameModuleV3);
    try std.testing.expectEqual(@as(i32, 0), NexoraGameModuleLoad(nx.abi_version, &module));
    try std.testing.expectEqual(@hasDecl(script.Game, "fixedUpdate"), module.fixed_update != null);
    try std.testing.expectEqual(@hasDecl(script.Game, "fixedUpdate"), module.capabilities & (1 << 1) != 0);
    try std.testing.expectEqual(@as(i32, -2), NexoraGameModuleLoad(nx.abi_version - 1, &module));
    try std.testing.expectEqual(@as(i32, -1), NexoraGameModuleLoad(nx.abi_version, null));
    module.struct_size = 0;
    try std.testing.expectEqual(@as(i32, -2), NexoraGameModuleLoad(nx.abi_version, &module));
}
