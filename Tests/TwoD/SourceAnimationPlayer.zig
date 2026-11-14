const std = @import("std");
const nx = @import("nexora");
const half_pixel: f64 = 1.0 / 48.0;
fn check(result: i32) !void {
    if (result != 0) return error.HostFailure;
}
pub const Game = struct {
    twod: nx.TwoDApiV1 = .{},
    sources: nx.TwoDSourceApiV1 = .{},
    content: nx.content.Api = .{},
    assets: nx.assets.Api = .{},
    entity: u64 = 0,
    handle: nx.TwoDInstanceHandle = .{},
    frames: u32 = 0,

    pub fn start(self: *Game, ctx: *const nx.game.Context) !void {
        self.twod = nx.getTwoD(&ctx.host) orelse return error.Unsupported;
        self.sources = nx.getTwoDSources(self.twod) orelse return error.Unsupported;
        const services = try ctx.services();
        self.assets = nx.getAssets(services) orelse return error.Unsupported;
        try check(services.get_content_api.?(services.context, &self.content));
        var asset = nx.content.Asset{};
        try check(self.content.describe_asset.?(self.content.context, 15, 16, &asset));
        if (asset.kind != 2 or asset.atlas_high != 0 or asset.atlas_low != 0)
            return error.SourceClipAdmission;
        try check(self.twod.set_camera.?(self.twod.context, &.{ .vertical_size = 10 }));
        var scene: u64 = 0;
        try check(ctx.host.load_scene.?(ctx.host.context, "Main", 4, 0, &scene));
        try check(ctx.host.activate_scene.?(ctx.host.context, scene));
        const descriptor = nx.EntitySpawnDescriptor{ .position = .{ .x = -2 + half_pixel, .y = -half_pixel } };
        try check(ctx.host.spawn_entity.?(ctx.host.context, scene, &descriptor, &self.entity));
        const sprite = nx.TwoDSourceSpriteV1{ .source_high = 11, .source_low = 12 };
        try check(self.sources.set_sprite.?(self.sources.context, self.entity, &sprite));
        const instances = [_]nx.TwoDSourceInstanceV1{.{
            .transform = .{ .position = .{ .x = 3 + half_pixel, .y = -half_pixel }, .scale = .{ .x = -1, .y = 1, .z = 1 } },
            .sprite = sprite,
        }};
        var handles: [1]nx.TwoDInstanceHandle = undefined;
        try check(self.sources.create_instances.?(self.sources.context, &instances, 1, &handles));
        self.handle = handles[0];
        try check(self.twod.play_animation.?(self.twod.context, self.entity, 15, 16, 1));
        if (self.assets.remove_asset.?(self.assets.context, 11, 12) != -3)
            return error.SourceDependencyRemoval;
        ctx.log("SOURCE PLAYER FILE CATALOG source animation", .{});
    }
    pub fn update(self: *Game, ctx: *const nx.game.Context, _: f64) !void {
        self.frames += 1;
        var events: [2]nx.TwoDEvent = undefined;
        var result = nx.TwoDAnimationResult{};
        const delta: f64 = if (self.frames == 21) 0.25 else 0;
        try check(self.twod.advance_animation.?(self.twod.context, self.entity, delta, &events, events.len, &result));
        const expected: u64 = if (self.frames >= 21) 13 else 11;
        var sprite = nx.TwoDSourceSpriteV1{};
        try check(self.sources.get_sprite.?(self.sources.context, self.entity, &sprite));
        if (sprite.source_high != expected or sprite.source_low != expected + 1 or result.region != 0)
            return error.SourceFrameLost;
        if (delta != 0 and (result.event_count != 1 or events[0].length != (if (expected == 13) @as(u32, 4) else 3)))
            return error.SourceEventLost;
        sprite.sprite.flags = 8 | @as(u32, if (self.frames >= 16) 3 else if (self.frames >= 11) 2 else if (self.frames >= 6) 1 else 0);
        try check(self.sources.set_sprite.?(self.sources.context, self.entity, &sprite));
        const handles = [_]nx.TwoDInstanceHandle{self.handle};
        var instances: [1]nx.TwoDSourceInstanceV1 = undefined;
        try check(self.sources.read_instances.?(self.sources.context, &handles, 1, &instances));
        if (instances[0].transform.scale.x != -1 or instances[0].transform.position.x != 3 + half_pixel or
            instances[0].transform.position.y != -half_pixel)
            return error.AnchorOrHandleDrift;
        instances[0].sprite = sprite;
        try check(self.sources.write_instances.?(self.sources.context, &handles, 1, &instances));
        if (self.frames == 5 or self.frames == 10 or self.frames == 15 or self.frames == 20 or self.frames == 25) {
            const phase: []const u8 = switch (self.frames) {
                5 => "base",
                10 => "flip-x",
                15 => "flip-y",
                20 => "flip-xy",
                else => "repacked",
            };
            ctx.log("SOURCE PLAYER CAPTURE {s}", .{phase});
            std.Thread.sleep(2 * std.time.ns_per_s);
        }
    }
    pub fn stop(self: *Game, ctx: *const nx.game.Context) void {
        const handles = [_]nx.TwoDInstanceHandle{self.handle};
        if (self.frames < 20 or self.twod.destroy_instances.?(self.twod.context, &handles, 1) != 0 or
            ctx.host.despawn_entity.?(ctx.host.context, self.entity) != 0 or
            self.assets.remove_asset.?(self.assets.context, 15, 16) != 0 or
            self.assets.remove_asset.?(self.assets.context, 11, 12) != 0 or
            self.assets.remove_asset.?(self.assets.context, 13, 14) != 0 or
            self.assets.remove_asset.?(self.assets.context, 8, 9) != 0 or
            self.assets.remove_asset.?(self.assets.context, 18, 19) != 0)
        {
            ctx.log("SOURCE PLAYER FAIL source animation teardown", .{});
            return;
        }
        ctx.log("SOURCE PLAYER PASS source animation frames={d}", .{self.frames});
    }
};
