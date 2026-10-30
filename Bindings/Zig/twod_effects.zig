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
pub const Api = extern struct {
    struct_size: u32 = @sizeOf(Api),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    get_capabilities: ?*const fn (?*anyopaque, *Capabilities) callconv(.c) i32 = null,
    get_state: ?*const fn (?*anyopaque, *State) callconv(.c) i32 = null,
    submit_frame: ?*const fn (?*anyopaque, *const Frame) callconv(.c) i32 = null,
    pub fn valid(self: Api) bool {
        return self.struct_size >= @sizeOf(Api) and self.abi_version == 1 and
            self.get_capabilities != null and self.get_state != null and self.submit_frame != null;
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
