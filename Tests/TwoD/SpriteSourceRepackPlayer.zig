const std = @import("std");
const nx = @import("nexora");
const file_catalog = @import("catalog_mode.zig").enabled;
const half_pixel: f32 = 1.0 / 48.0;

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
    flags: u32 = 8,
    frames: u32 = 0,

    pub fn start(self: *Game, ctx: *const nx.game.Context) !void {
        self.twod = nx.getTwoD(&ctx.host) orelse return error.Unsupported;
        self.sources = nx.getTwoDSources(self.twod) orelse return error.Unsupported;
        const services = try ctx.services();
        self.assets = nx.getAssets(services) orelse return error.Unsupported;
        try check(services.get_content_api.?(services.context, &self.content));
        if (!file_catalog) return error.CatalogRequired;
        var source = nx.content.Asset{};
        try check(self.content.describe_asset.?(self.content.context, 11, 12, &source));
        if (source.kind != nx.content.sprite_source_kind or source.atlas_high != 8 or source.atlas_low != 9 or
            source.width != 6 or source.height != 6) return error.SourceIdentity;
        const camera = nx.TwoDCamera{ .vertical_size = 10 };
        try check(self.twod.set_camera.?(self.twod.context, &camera));
        var scene: u64 = 0;
        try check(ctx.host.load_scene.?(ctx.host.context, "Main", 4, 0, &scene));
        try check(ctx.host.activate_scene.?(ctx.host.context, scene));
        // Align quadrant texel centers with native pixel centers, avoiding filter-edge ambiguity.
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
        ctx.log("SOURCE PLAYER FILE CATALOG", .{});
        ctx.log("SOURCE PLAYER START asymmetric flip/repack", .{});
    }

    pub fn update(self: *Game, ctx: *const nx.game.Context, _: f64) !void {
        self.frames += 1;
        var sprite = nx.TwoDSourceSpriteV1{};
        try check(self.sources.get_sprite.?(self.sources.context, self.entity, &sprite));
        if (sprite.source_high != 11 or sprite.source_low != 12 or sprite.sprite.flags != self.flags or
            sprite.sprite.size.x != 1 or sprite.sprite.pivot.x != 0.5) return error.LostSceneSource;
        var instances: [1]nx.TwoDSourceInstanceV1 = undefined;
        const handles = [_]nx.TwoDInstanceHandle{self.handle};
        try check(self.sources.read_instances.?(self.sources.context, &handles, 1, &instances));
        const instance = instances[0];
        if (instance.sprite.source_high != 11 or instance.sprite.source_low != 12 or
            instance.sprite.sprite.flags != self.flags or instance.transform.scale.x != -1 or
            instance.transform.position.x != 3 + half_pixel or instance.transform.position.y != -half_pixel)
            return error.LostPoolIdentity;
        if (self.frames == 6 or self.frames == 11 or self.frames == 16) {
            self.flags = if (self.frames == 6) 9 else if (self.frames == 11) 10 else 11;
            sprite.sprite.flags = self.flags;
            instances[0].sprite = sprite;
            try check(self.sources.set_sprite.?(self.sources.context, self.entity, &sprite));
            try check(self.sources.write_instances.?(self.sources.context, &handles, 1, &instances));
        }
        if (self.frames == 21) {
            var high: u64 = 0;
            var low: u64 = 0;
            try check(self.content.registerAtlas(@embedFile("blue.nx2atlas"), &high, &low));
            var atlas = nx.content.Asset{};
            try check(self.content.describe_asset.?(self.content.context, high, low, &atlas));
            if (high != 8 or low != 9 or atlas.width != 8 or atlas.height != 8) return error.AtlasRepack;
            // Only the Atlas generation changes; source metadata, handles and authored transforms stay intact.
            ctx.log("SOURCE PLAYER REPACK atlas=8:9 source=11:12 stable handles", .{});
        }
        if (self.frames <= 25 and self.frames % 5 == 0) {
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
        if (self.frames < 30 or self.twod.destroy_instances.?(self.twod.context, &handles, 1) != 0 or
            ctx.host.despawn_entity.?(ctx.host.context, self.entity) != 0 or
            self.assets.remove_asset.?(self.assets.context, 11, 12) != 0 or
            self.assets.remove_asset.?(self.assets.context, 8, 9) != 0)
        {
            ctx.log("SOURCE PLAYER FAIL teardown", .{});
            return;
        }
        ctx.log("SOURCE PLAYER PASS frames={d} flip/repack", .{self.frames});
    }
};
