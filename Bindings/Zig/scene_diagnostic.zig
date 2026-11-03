//! Engine-owned packaged-scene acceptance fixture. No product gameplay or assets.
const nx = @import("nexora.zig");
pub const State = struct {
    api: nx.scene.Api = .{},
    request: u64 = 0,
    previous: u64 = 0,
    overlay: u64 = 0,
    phase: u32 = 0,
    complete: bool = false,

    pub fn start(self: *State, context: *const nx.game.Context) !void {
        self.api = try context.scenes();
        self.previous = try self.api.active();
        const info = try self.api.info(self.previous);
        if (info.asset_high != 501 or info.asset_low != 1) return error.WrongStartupScene;
        self.request = try self.api.loadPath("Start.scene", .replace, false);
    }
    pub fn update(self: *State, context: *const nx.game.Context) !void {
        if (self.complete) return;
        const status = try self.api.status(self.request);
        if (status.state == @intFromEnum(nx.scene.State.queued)) return;
        if (status.state != @intFromEnum(nx.scene.State.applied)) return error.SceneRequestFailed;
        switch (self.phase) {
            0 => {
                if (status.scene <= self.previous) return error.RecycledSceneId;
                self.previous = status.scene;
                self.request = try self.api.loadPath("Title.scene", .replace, false);
            },
            1 => {
                if (status.scene <= self.previous) return error.RecycledSceneId;
                self.previous = status.scene;
                self.request = try self.api.loadPath("Main.scene", .additive, false);
            },
            2 => {
                if (try self.api.active() != self.previous) return error.AdditiveChangedPrimary;
                self.overlay = status.scene;
                const info = try self.api.info(self.overlay);
                if (info.asset_low != 3) return error.WrongLoadedScene;
                self.request = try self.api.unload(self.overlay);
            },
            3 => {
                if (self.api.info(self.overlay)) |_| return error.UnloadRetainedScene else |err| {
                    if (err != error.NotFound) return err;
                }
                self.request = try self.api.loadPath("Main.scene", .replace, false);
            },
            4 => {
                const info = try self.api.info(try self.api.active());
                if (info.asset_low != 3 or status.scene <= self.previous) return error.WrongMainScene;
                self.complete = true;
                context.log("SCENE DIAGNOSTIC PASS replace/additive/unload/fresh-id", .{});
            },
            else => return error.InvalidScenePhase,
        }
        self.phase += 1;
    }
    pub fn stop(self: *State, context: *const nx.game.Context) void {
        if (!self.complete) context.log("SCENE DIAGNOSTIC FAIL incomplete phase={d}", .{self.phase});
    }
};
