const nx = @import("nexora");
const Diagnostic = @import("hd2d_module.zig").Module(false);
export fn NexoraGameModuleLoad(abi: u32, module: ?*nx.GameModuleV3) i32 {
    return Diagnostic.load(abi, module);
}
