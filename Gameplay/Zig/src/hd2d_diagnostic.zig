const nx = @import("nexora");
const std = @import("std");
const builtin = @import("builtin");
// Keep the raw diagnostic unloadable under the Player's strict macOS admission.
pub const panic = if (builtin.os.tag == .macos and !builtin.is_test)
    std.debug.simple_panic
else
    std.debug.FullPanic(std.debug.defaultPanic);
const Diagnostic = @import("hd2d_module.zig").Module(false);
export fn NexoraGameModuleLoad(abi: u32, module: ?*nx.GameModuleV3) i32 {
    return Diagnostic.load(abi, module);
}
