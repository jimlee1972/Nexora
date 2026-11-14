const std = @import("std");
const nx = @import("nexora");
const file_catalog = @import("catalog_mode.zig").enabled;
fn check(result: i32) !void {
    if (result != 0) return error.HostFailure;
}
pub const Game = struct {
    twod: nx.TwoDApiV1 = .{},
    sources: nx.TwoDSourceApiV1 = .{},
    content: nx.content.Api = .{},
    registration: nx.content.SpriteApi = .{},
    assets: nx.assets.Api = .{},
    entity: u64 = 0,
    handle: nx.TwoDInstanceHandle = .{},
    frames: u32 = 0,

    pub fn start(self: *Game, ctx: *const nx.game.Context) !void {
        self.twod = nx.getTwoD(&ctx.host) orelse return error.Unsupported;
        self.sources = nx.getTwoDSources(self.twod) orelse return error.Unsupported;
        const services = try ctx.services();
        self.registration = nx.getSpriteContent(services) orelse return error.Unsupported;
        self.assets = nx.getAssets(services) orelse return error.Unsupported;
        try check(services.get_content_api.?(services.context, &self.content));
        var high: u64 = 55;
        var low: u64 = 66;
        if (file_catalog) {
            var asset = nx.content.Asset{};
            try check(self.content.describe_asset.?(self.content.context, 11, 12, &asset));
            if (asset.kind != nx.content.sprite_source_kind or asset.atlas_high != 8 or asset.atlas_low != 9 or
                asset.width != 6 or asset.height != 6) return error.CatalogSourceAdmission;
            high = asset.high;
            low = asset.low;
            ctx.log("SOURCE PLAYER FILE CATALOG", .{});
        } else {
            if (self.registration.registerSpriteSource(@embedFile("source.nx2sprite"), &high, &low) != -2 or high != 55 or low != 66)
                return error.DependencyAdmission;
            try check(self.content.registerAtlas(@embedFile("red.nx2atlas"), &high, &low));
            try check(self.registration.registerSpriteSource(@embedFile("source.nx2sprite"), &high, &low));
        }
        if (high != 11 or low != 12) return error.SourceIdentity;
        const camera = nx.TwoDCamera{ .vertical_size = 10 };
        try check(self.twod.set_camera.?(self.twod.context, &camera));
        var scene: u64 = 0;
        try check(ctx.host.load_scene.?(ctx.host.context, "Main", 4, 0, &scene));
        try check(ctx.host.activate_scene.?(ctx.host.context, scene));
        const descriptor = nx.EntitySpawnDescriptor{ .position = .{ .x = -1, .y = -3 } };
        try check(ctx.host.spawn_entity.?(ctx.host.context, scene, &descriptor, &self.entity));
        const sprite = nx.TwoDSourceSpriteV1{ .source_high = high, .source_low = low };
        try check(self.sources.set_sprite.?(self.sources.context, self.entity, &sprite));
        const instances = [_]nx.TwoDSourceInstanceV1{.{
            .transform = .{ .position = .{ .x = 3, .y = -3 }, .scale = .{ .x = -1, .y = 1, .z = 1 } },
            .sprite = sprite,
        }};
        var handles: [1]nx.TwoDInstanceHandle = undefined;
        try check(self.sources.create_instances.?(self.sources.context, &instances, 1, &handles));
        self.handle = handles[0];
        var legacy = nx.TwoDSprite{ .order = 123 };
        if (self.twod.get_sprite.?(self.twod.context, self.entity, &legacy) != -2 or legacy.order != 123)
            return error.LegacyReadLostSource;
        ctx.log("SOURCE PLAYER START source={d}:{d}", .{ high, low });
    }
    pub fn update(self: *Game, ctx: *const nx.game.Context, _: f64) !void {
        self.frames += 1;
        var sprite = nx.TwoDSourceSpriteV1{};
        try check(self.sources.get_sprite.?(self.sources.context, self.entity, &sprite));
        if (sprite.source_high != 11 or sprite.source_low != 12) return error.LostSceneSource;
        var instances: [1]nx.TwoDSourceInstanceV1 = undefined;
        const handles = [_]nx.TwoDInstanceHandle{self.handle};
        try check(self.sources.read_instances.?(self.sources.context, &handles, 1, &instances));
        if (instances[0].sprite.source_high != 11 or instances[0].sprite.source_low != 12 or
            instances[0].transform.scale.x != -1) return error.LostPoolSource;
        if (self.frames == 10) {
            var high: u64 = 0;
            var low: u64 = 0;
            try check(self.content.registerAtlas(@embedFile("blue.nx2atlas"), &high, &low));
            try check(self.registration.registerSpriteSource(@embedFile("shifted.nx2sprite"), &high, &low));
            // The published sources move; authored instance transforms and handle remain intact.
            try check(self.sources.write_instances.?(self.sources.context, &handles, 1, &instances));
            ctx.log("SOURCE PLAYER REPLACED", .{});
        }
        if (self.frames == 5 or self.frames == 15) {
            ctx.log("SOURCE PLAYER CAPTURE {s}", .{if (self.frames == 5) "red" else "blue"});
            // Acceptance fixture only: retain the previously submitted native frame for readback.
            std.Thread.sleep(2 * std.time.ns_per_s);
        }
    }
    pub fn stop(self: *Game, ctx: *const nx.game.Context) void {
        const handles = [_]nx.TwoDInstanceHandle{self.handle};
        if (self.frames < 20 or self.twod.destroy_instances.?(self.twod.context, &handles, 1) != 0 or
            ctx.host.despawn_entity.?(ctx.host.context, self.entity) != 0 or
            self.assets.remove_asset.?(self.assets.context, 11, 12) != 0 or
            self.assets.remove_asset.?(self.assets.context, 8, 9) != 0)
        {
            ctx.log("SOURCE PLAYER FAIL teardown", .{});
            return;
        }
        ctx.log("SOURCE PLAYER PASS frames={d}", .{self.frames});
    }
};
