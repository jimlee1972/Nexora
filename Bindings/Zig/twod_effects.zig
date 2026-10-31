//! Atomic owner-thread 2D effects snapshots; optional and independently negotiated.
pub const Point = extern struct { x: f32 = 0, y: f32 = 0 };
pub const Light = extern struct {
    id: u64 = 0,
    position: Point = .{},
    color: [3]f32 = .{ 1, 1, 1 },
    intensity: f32 = 1,
    radius: f32 = 1,
    softness: f32 = 0,
    bias: f32 = 0,
    groups: u32 = 0xffffffff,
    shadow: u32 = 0,
};
pub const Occluder = extern struct {
    id: u64 = 0,
    groups: u32 = 0xffffffff,
    vertex_count: u32 = 0,
    vertices: [16]Point = @splat(.{}),
};
pub const Sprite = extern struct {
    handle: u64 = 0,
    kind: u32 = 0,
    groups: u32 = 0xffffffff,
    receive_light: u32 = 1,
    receive_shadow: u32 = 1,
    emission_atlas_high: u64 = 0,
    emission_atlas_low: u64 = 0,
    emission_region: u32 = 0,
    reserved: u32 = 0,
    emission_color: [3]f32 = .{ 1, 1, 1 },
    emission_intensity: f32 = 0,
};
pub const Bloom = extern struct {
    intensity: f32 = 0,
    radius: f32 = 12,
    levels: u32 = 5,
    reserved: u32 = 0,
};
pub const Frame = extern struct {
    struct_size: u32 = @sizeOf(Frame),
    abi_version: u32 = 1,
    enabled: u32 = 0,
    light_count: u32 = 0,
    occluder_count: u32 = 0,
    sprite_count: u32 = 0,
    ambient: [3]f32 = .{ 1, 1, 1 },
    strength: f32 = 1,
    bloom: Bloom = .{},
    lights: ?[*]const Light = null,
    occluders: ?[*]const Occluder = null,
    sprites: ?[*]const Sprite = null,
};
pub const Capabilities = extern struct {
    struct_size: u32 = @sizeOf(Capabilities),
    abi_version: u32 = 1,
    features: u32 = 0,
    max_lights: u32 = 0,
    max_shadow_lights: u32 = 0,
    max_occluders: u32 = 0,
    max_vertices: u32 = 0,
    max_edges: u32 = 0,
    max_sprites: u32 = 0,
    shadow_samples: u32 = 0,
};
pub const State = extern struct {
    struct_size: u32 = @sizeOf(State),
    abi_version: u32 = 1,
    accepted_revision: u64 = 0,
    active_revision: u64 = 0,
    enabled: u32 = 0,
    light_count: u32 = 0,
    occluder_count: u32 = 0,
    sprite_count: u32 = 0,
};
pub const Error = error{ InvalidArgument, Unsupported, Lifecycle };
fn check(code: i32) Error!void {
    return switch (code) {
        0 => {},
        -1 => error.InvalidArgument,
        -2 => error.Unsupported,
        else => error.Lifecycle,
    };
}
/// Last completed native frame. timing_mask marks actual GPU measurements; absent values
/// are unavailable, never a zero-duration claim. Allocations/capacities are not resident VRAM.
pub const Diagnostics = extern struct {
    struct_size: u32 = @sizeOf(Diagnostics),
    abi_version: u32 = 1,
    timing_mask: u32 = 0,
    timing_source: u32 = 0,
    timing_status: u32 = 0,
    reserved: u32 = 0,
    completed_frame: u64 = 0,
    effects_revision: u64 = 0,
    shadow_dispatches: u64 = 0,
    world_draws: u64 = 0,
    bloom_passes: u64 = 0,
    composite_draws: u64 = 0,
    shadow_ms: f64 = 0,
    world_ms: f64 = 0,
    bloom_ms: f64 = 0,
    composite_ms: f64 = 0,
    scene_target_bytes: u64 = 0,
    shadow_buffer_bytes: u64 = 0,
    bloom_target_bytes: u64 = 0,
    upload_capacity_bytes: u64 = 0,
    peak_effect_bytes: u64 = 0,
    texture_count: u64 = 0,
    peak_texture_count: u64 = 0,
};
pub const Api = extern struct {
    struct_size: u32 = @sizeOf(Api),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    get_capabilities: ?*const fn (?*anyopaque, *Capabilities) callconv(.c) i32 = null,
    get_state: ?*const fn (?*anyopaque, *State) callconv(.c) i32 = null,
    submit_frame: ?*const fn (?*anyopaque, *const Frame) callconv(.c) i32 = null,
    pool_identity: ?*const fn (?*anyopaque, u64, u32, u32, *u64) callconv(.c) i32 = null,
    get_diagnostics: ?*const fn (?*anyopaque, *Diagnostics) callconv(.c) i32 = null,
    set_debug_view: ?*const fn (?*anyopaque, u32, u32) callconv(.c) i32 = null,
    pub fn valid(self: Api) bool {
        return self.struct_size >= @offsetOf(Api, "pool_identity") and self.abi_version == 1 and
            self.get_capabilities != null and self.get_state != null and self.submit_frame != null;
    }
    pub fn poolIdentity(self: Api, handle: anytype) Error!u64 {
        if (!self.valid() or self.struct_size < @offsetOf(Api, "pool_identity") + @sizeOf(@FieldType(Api, "pool_identity"))) return error.Unsupported;
        const resolve = self.pool_identity orelse return error.Unsupported;
        var identity: u64 = 0;
        try check(resolve(self.context, handle.owner, handle.index, handle.generation, &identity));
        if (identity == 0) return error.Lifecycle;
        return identity;
    }
    pub fn diagnostics(self: Api) Error!Diagnostics {
        if (!self.valid() or self.struct_size < @offsetOf(Api, "get_diagnostics") + @sizeOf(@FieldType(Api, "get_diagnostics"))) return error.Unsupported;
        const query = self.get_diagnostics orelse return error.Unsupported;
        var result = Diagnostics{};
        try check(query(self.context, &result));
        if (result.struct_size < @sizeOf(Diagnostics) or result.abi_version != 1) return error.Unsupported;
        return result;
    }
    pub fn debugView(self: Api, mode: u32, shadow_slot: u32) Error!void {
        if (!self.valid() or self.struct_size < @offsetOf(Api, "set_debug_view") + @sizeOf(@FieldType(Api, "set_debug_view"))) return error.Unsupported;
        const request = self.set_debug_view orelse return error.Unsupported;
        try check(request(self.context, mode, shadow_slot));
    }
    pub fn poolSprite(self: Api, handle: anytype) Error!Sprite {
        return .{ .handle = try self.poolIdentity(handle), .kind = 1 };
    }
    pub fn capabilities(self: Api) Error!Capabilities {
        if (!self.valid()) return error.Unsupported;
        var result = Capabilities{};
        try check(self.get_capabilities.?(self.context, &result));
        if (result.struct_size < @sizeOf(Capabilities) or result.abi_version != 1) return error.Unsupported;
        return result;
    }
    pub fn state(self: Api) Error!State {
        if (!self.valid()) return error.Unsupported;
        var result = State{};
        try check(self.get_state.?(self.context, &result));
        if (result.struct_size < @sizeOf(State) or result.abi_version != 1) return error.Unsupported;
        return result;
    }
    pub fn submit(self: Api, options: Frame, lights: []const Light, occluders: []const Occluder, sprites: []const Sprite) Error!void {
        if (!self.valid()) return error.Unsupported;
        if (lights.len > 32 or occluders.len > 256 or sprites.len > 100000) return error.InvalidArgument;
        var frame = options;
        frame.light_count = @intCast(lights.len);
        frame.occluder_count = @intCast(occluders.len);
        frame.sprite_count = @intCast(sprites.len);
        frame.lights = if (lights.len == 0) null else lights.ptr;
        frame.occluders = if (occluders.len == 0) null else occluders.ptr;
        frame.sprites = if (sprites.len == 0) null else sprites.ptr;
        try check(self.submit_frame.?(self.context, &frame));
    }
};

/// Borrowed companion links. Load through the existing blob asset service or @embedFile;
/// register/load the referenced mask Atlas before submitting styles. No allocator or IO here.
pub const EmissionAsset = struct {
    id: [2]u64,
    base_atlas: [2]u64,
    mask_atlas: [2]u64,
    regions: []const u8,

    fn number(comptime T: type, bytes: []const u8) T {
        return @import("std").mem.readInt(T, bytes[0..@sizeOf(T)], .little);
    }
    pub fn parse(bytes: []const u8) Error!EmissionAsset {
        if (bytes.len < 60 or bytes.len > 60 + 1024 * 8) return error.InvalidArgument;
        if (number(u32, bytes[0..]) != 0x4532444e or number(u32, bytes[4..]) != 1) return error.InvalidArgument;
        const count = number(u32, bytes[56..]);
        if (count == 0 or count > 1024 or bytes.len != 60 + @as(usize, count) * 8) return error.InvalidArgument;
        const result = EmissionAsset{
            .id = .{ number(u64, bytes[8..]), number(u64, bytes[16..]) },
            .base_atlas = .{ number(u64, bytes[24..]), number(u64, bytes[32..]) },
            .mask_atlas = .{ number(u64, bytes[40..]), number(u64, bytes[48..]) },
            .regions = bytes[60..],
        };
        const std = @import("std");
        for ([_][2]u64{ result.id, result.base_atlas, result.mask_atlas }) |identity|
            if (identity[0] == 0 and identity[1] == 0) return error.InvalidArgument;
        if (std.meta.eql(result.id, result.base_atlas) or std.meta.eql(result.id, result.mask_atlas) or
            std.meta.eql(result.base_atlas, result.mask_atlas)) return error.InvalidArgument;
        var i: usize = 0;
        while (i < result.regions.len) : (i += 8) {
            const base = number(u32, result.regions[i..]);
            if (base == 0 or number(u32, result.regions[i + 4 ..]) == 0) return error.InvalidArgument;
            var j: usize = 0;
            while (j < i) : (j += 8)
                if (number(u32, result.regions[j..]) == base) return error.InvalidArgument;
        }
        return result;
    }
    pub fn maskFor(self: EmissionAsset, base_region: u32) ?u32 {
        var offset: usize = 0;
        while (offset < self.regions.len) : (offset += 8)
            if (number(u32, self.regions[offset..]) == base_region)
                return number(u32, self.regions[offset + 4 ..]);
        return null;
    }
    pub fn apply(self: EmissionAsset, sprite: Sprite, base_region: u32, color: [3]f32, intensity: f32) Error!Sprite {
        var result = sprite;
        result.emission_region = self.maskFor(base_region) orelse return error.InvalidArgument;
        result.emission_atlas_high = self.mask_atlas[0];
        result.emission_atlas_low = self.mask_atlas[1];
        result.emission_color = color;
        result.emission_intensity = intensity;
        return result;
    }
};
