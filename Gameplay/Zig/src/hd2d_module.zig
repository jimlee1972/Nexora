//! Generic Player wrappers for the reusable Zig diagnostic; no game-specific C++ callbacks.
const nx = @import("nexora");
pub fn Module(comptime automated: bool) type {
    return struct {
        const Owner = struct { host: nx.GameplayHostV3, diagnostic: nx.hd2d_diagnostic.State = .{} };
        fn owner(p: ?*anyopaque) *Owner {
            return @ptrCast(@alignCast(p.?));
        }
        fn create(output: *?*anyopaque, host: *const nx.GameplayHostV3) callconv(.c) i32 {
            const allocate = host.allocate orelse return -2;
            if (host.deallocate == null or host.load_scene == null or host.activate_scene == null) return -2;
            const memory = allocate(host.context, 1, @sizeOf(Owner), @alignOf(Owner)) orelse return -3;
            const o: *Owner = @ptrCast(@alignCast(memory));
            o.* = .{ .host = host.* };
            output.* = o;
            return 0;
        }
        fn start(p: ?*anyopaque) callconv(.c) i32 {
            const o = owner(p);
            var scene: u64 = 0;
            if (o.host.load_scene.?(o.host.context, "Main".ptr, 4, 0, &scene) != 0 or
                o.host.activate_scene.?(o.host.context, scene) != 0) return -3;
            o.diagnostic.start(&o.host, .{ .automated = automated, .capture_holds = automated }) catch {
                if (o.host.log) |log| log(o.host.context, 3, "HD2D START FAIL".ptr, 15);
                return -3;
            };
            return 0;
        }
        fn update(p: ?*anyopaque, delta: f64) callconv(.c) i32 {
            const o = owner(p);
            o.diagnostic.update(delta) catch {
                if (o.host.log) |log| log(o.host.context, 3, "HD2D UPDATE FAIL".ptr, 16);
                return -3;
            };
            return 0;
        }
        fn stop(p: ?*anyopaque) callconv(.c) void {
            const o = owner(p);
            o.diagnostic.stop() catch {
                if (o.host.log) |log| log(o.host.context, 3, "HD2D STOP FAIL".ptr, 14);
            };
        }
        fn destroy(p: ?*anyopaque) callconv(.c) void {
            const o = owner(p);
            const host = o.host;
            stop(p);
            host.deallocate.?(host.context, 1, o, @sizeOf(Owner), @alignOf(Owner));
        }
        pub fn load(abi: u32, module: ?*nx.GameModuleV3) i32 {
            const output = module orelse return -1;
            if (abi != 3 or output.struct_size < @sizeOf(nx.GameModuleV3)) return -2;
            output.* = .{ .struct_size = @sizeOf(nx.GameModuleV3), .abi_version = 3, .capabilities = 0, .module_state = null, .create = create, .on_start = start, .fixed_update = null, .update = update, .on_stop = stop, .destroy = destroy, .save_state = null, .load_state = null };
            return 0;
        }
    };
}
