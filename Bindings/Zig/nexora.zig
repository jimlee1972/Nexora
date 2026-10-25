//! Thin Zig declarations for the canonical `nexora/nexora.h` ABI.
//! Field order is checked against Engine/API/abi_baseline_v3.json in CTest.
pub const abi_version: u32 = 3;
pub const game = @import("game.zig");
pub const input = @import("input.zig");
pub const ui = @import("ui.zig");
pub const dialogue = @import("dialogue.zig");
pub const lifecycle = @import("lifecycle.zig");
pub const scene = @import("scene.zig");
pub const audio_state = @import("audio_state.zig");
pub const audio = @import("audio.zig");
pub const storage = @import("storage.zig");
pub const content = @import("content.zig");
pub const assets = @import("assets.zig");
pub const display = @import("display.zig");
pub const color = @import("color.zig");
pub const ui_draw = @import("ui_draw.zig");
pub const services_api_capability: u64 = 1 << 6;
pub const GameplayServicesApiV1 = extern struct {
    struct_size: u32 = @sizeOf(GameplayServicesApiV1),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    get_input_api: ?*const fn (?*anyopaque, *input.Api) callconv(.c) i32 = null,
    get_ui_api: ?*const fn (?*anyopaque, *ui.Api) callconv(.c) i32 = null,
    get_audio_api: ?*const fn (?*anyopaque, *audio.Api) callconv(.c) i32 = null,
    get_storage_api: ?*const fn (?*anyopaque, *storage.Api) callconv(.c) i32 = null,
    get_content_api: ?*const fn (?*anyopaque, *content.Api) callconv(.c) i32 = null,
    get_display_api: ?*const fn (?*anyopaque, *display.Api) callconv(.c) i32 = null,
    get_ui_draw_api: ?*const fn (?*anyopaque, *ui_draw.Api) callconv(.c) i32 = null,
    get_assets_api: ?*const fn (?*anyopaque, *assets.Api) callconv(.c) i32 = null,
    get_audio_state_api: ?*const fn (?*anyopaque, *audio_state.Api) callconv(.c) i32 = null,
    get_lifecycle_api: ?*const fn (?*anyopaque, *lifecycle.Api) callconv(.c) i32 = null,
    get_scene_api: ?*const fn (?*anyopaque, *scene.Api) callconv(.c) i32 = null,
};
pub const services_v1_base_size: u32 = @offsetOf(GameplayServicesApiV1, "get_display_api");
pub fn getScene(services: GameplayServicesApiV1) ?scene.Api {
    if (services.struct_size < @offsetOf(GameplayServicesApiV1, "get_scene_api") + @sizeOf(@FieldType(GameplayServicesApiV1, "get_scene_api"))) return null;
    const get = services.get_scene_api orelse return null;
    var api = scene.Api{};
    if (get(services.context, &api) != 0 or !api.valid()) return null;
    return api;
}
pub fn getDisplay(services: GameplayServicesApiV1) ?display.Api {
    if (services.struct_size < @offsetOf(GameplayServicesApiV1, "get_display_api") + @sizeOf(@FieldType(GameplayServicesApiV1, "get_display_api"))) return null;
    const get = services.get_display_api orelse return null;
    var api = display.Api{};
    if (get(services.context, &api) != 0 or api.struct_size < @sizeOf(display.Api) or api.abi_version != 1) return null;
    return api;
}
pub fn getUiDraw(services: GameplayServicesApiV1) ?ui_draw.Api {
    if (services.struct_size < @offsetOf(GameplayServicesApiV1, "get_ui_draw_api") + @sizeOf(@FieldType(GameplayServicesApiV1, "get_ui_draw_api"))) return null;
    const get = services.get_ui_draw_api orelse return null;
    var api = ui_draw.Api{};
    if (get(services.context, &api) != 0 or !api.valid()) return null;
    return api;
}
pub fn getAssets(services: GameplayServicesApiV1) ?assets.Api {
    if (services.struct_size < @offsetOf(GameplayServicesApiV1, "get_assets_api") + @sizeOf(@FieldType(GameplayServicesApiV1, "get_assets_api"))) return null;
    const get = services.get_assets_api orelse return null;
    var api = assets.Api{};
    if (get(services.context, &api) != 0 or !api.valid()) return null;
    return api;
}
pub fn getAudioState(services: GameplayServicesApiV1) ?audio_state.Api {
    if (services.struct_size < @offsetOf(GameplayServicesApiV1, "get_audio_state_api") + @sizeOf(@FieldType(GameplayServicesApiV1, "get_audio_state_api"))) return null;
    const get = services.get_audio_state_api orelse return null;
    var api = audio_state.Api{};
    if (get(services.context, &api) != 0 or !api.valid()) return null;
    return api;
}
pub fn getLifecycle(services: GameplayServicesApiV1) ?lifecycle.Api {
    if (services.struct_size < @offsetOf(GameplayServicesApiV1, "get_lifecycle_api") + @sizeOf(@FieldType(GameplayServicesApiV1, "get_lifecycle_api"))) return null;
    const get = services.get_lifecycle_api orelse return null;
    var api = lifecycle.Api{};
    if (get(services.context, &api) != 0 or !api.valid()) return null;
    return api;
}
pub const host_v3_services_size: u32 = @offsetOf(GameplayHostV3, "get_services_api") + @sizeOf(@FieldType(GameplayHostV3, "get_services_api"));
pub fn getServices(host: *const GameplayHostV3) ?GameplayServicesApiV1 {
    if (host.struct_size < host_v3_services_size or host.abi_version != abi_version or
        (host.capabilities & services_api_capability) == 0) return null;
    const get = host.get_services_api orelse return null;
    var api = GameplayServicesApiV1{};
    if (get(host.context, &api) != 0 or api.struct_size < services_v1_base_size or api.abi_version != 1) return null;
    return api;
}

pub const Result = enum(i32) {
    ok = 0,
    invalid_argument = -1,
    unsupported = -2,
    lifecycle = -3,
};

pub const scene_api_capability: u64 = 1 << 3;
pub const twod_api_capability: u64 = 1 << 4;
pub const pointer_input_capability: u64 = 1 << 5;

pub const AllocationOwner = enum(u64) {
    unknown = 0,
    gameplay_state = 1,
    state_migration = 2,
};

pub const Vec3 = extern struct { x: f64 = 0, y: f64 = 0, z: f64 = 0 };
pub const AssetHandle = extern struct { value: u64 = 0 };
pub const EntitySpawnDescriptor = extern struct {
    struct_size: u32 = @sizeOf(EntitySpawnDescriptor),
    components: u32 = 0,
    position: Vec3 = .{},
    camera_fov_degrees: f64 = 0,
    light_intensity: f32 = 0,
    reserved: u32 = 0,
    mesh: AssetHandle = .{},
    material: AssetHandle = .{},
    bounds_minimum: Vec3 = .{},
    bounds_maximum: Vec3 = .{},
};
pub const InputSnapshot = extern struct { sequence: u64 = 0, move_x: f64 = 0, move_y: f64 = 0, buttons: u32 = 0, reserved: u32 = 0 };
pub const PointerInputSnapshotV1 = extern struct {
    struct_size: u32 = @sizeOf(PointerInputSnapshotV1),
    abi_version: u32 = 1,
    sequence: u64 = 0,
    delta_x: f64 = 0,
    delta_y: f64 = 0,
    wheel_x: f64 = 0,
    wheel_y: f64 = 0,
    buttons: u32 = 0,
    reserved: u32 = 0,
};
pub const RaycastRequest = extern struct { origin: Vec3 = .{}, direction: Vec3 = .{}, distance: f64 = 0 };
pub const RaycastHit = extern struct { entity: u64 = 0, distance: f64 = 0, point: Vec3 = .{} };
pub const DebugLine = extern struct { start: Vec3 = .{}, end: Vec3 = .{}, rgba: u32 = 0, duration_seconds: f32 = 0 };
pub const Quat = extern struct {
    x: f64 = 0,
    y: f64 = 0,
    z: f64 = 0,
    w: f64 = 1,
};
/// "Nexora.TransformV2" (local) and "Nexora.WorldTransform" (read only).
pub const TransformV2 = extern struct {
    position: Vec3 = .{},
    rotation: Quat = .{},
    scale: Vec3 = .{ .x = 1, .y = 1, .z = 1 },
};
/// "Nexora.Parent": 0 for a root; keep_local == 0 keeps the world pose when written.
pub const Parent = extern struct {
    parent: u64 = 0,
    keep_local: u32 = 0,
    reserved: u32 = 0,
};
// Component wires must match nexora/nexora.h on every target (checked by Tests/API too).
comptime {
    if (@sizeOf(Quat) != 32 or @sizeOf(TransformV2) != 80 or @offsetOf(TransformV2, "scale") != 56 or
        @sizeOf(Parent) != 16 or @offsetOf(Parent, "keep_local") != 8)
        @compileError("Nexora component wire layout changed");
}
pub const FrameDiagnostics = extern struct { frame: u64 = 0, scene_entities: u64 = 0, debug_lines: u64 = 0, api_errors: u64 = 0 };

pub const GameplayHostV3 = extern struct {
    struct_size: u32,
    abi_version: u32,
    capabilities: u64,
    context: ?*anyopaque,
    log: ?*const fn (?*anyopaque, u32, [*]const u8, u32) callconv(.c) void,
    read_component: ?*const fn (?*anyopaque, u64, u64, ?*anyopaque, u32) callconv(.c) i32,
    write_component: ?*const fn (?*anyopaque, u64, u64, ?*const anyopaque, u32) callconv(.c) i32,
    allocate: ?*const fn (?*anyopaque, u64, u64, u64) callconv(.c) ?*anyopaque,
    deallocate: ?*const fn (?*anyopaque, u64, ?*anyopaque, u64, u64) callconv(.c) void,
    load_scene: ?*const fn (?*anyopaque, [*]const u8, u32, u32, *u64) callconv(.c) i32,
    activate_scene: ?*const fn (?*anyopaque, u64) callconv(.c) i32,
    spawn_entity: ?*const fn (?*anyopaque, u64, *const EntitySpawnDescriptor, *u64) callconv(.c) i32,
    despawn_entity: ?*const fn (?*anyopaque, u64) callconv(.c) i32,
    capture_input: ?*const fn (?*anyopaque, u32, *InputSnapshot) callconv(.c) i32,
    resolve_asset: ?*const fn (?*anyopaque, u64, u64, *AssetHandle) callconv(.c) i32,
    raycast: ?*const fn (?*anyopaque, *const RaycastRequest, *RaycastHit) callconv(.c) i32,
    debug_draw_line: ?*const fn (?*anyopaque, *const DebugLine) callconv(.c) i32,
    get_diagnostics: ?*const fn (?*anyopaque, *FrameDiagnostics) callconv(.c) i32,
    get_twod_api: ?*const fn (?*anyopaque, *TwoDApiV1) callconv(.c) i32 = null,
    capture_pointer_input: ?*const fn (?*anyopaque, u32, *PointerInputSnapshotV1) callconv(.c) i32 = null,
    get_services_api: ?*const fn (?*anyopaque, *GameplayServicesApiV1) callconv(.c) i32 = null,
};

pub const host_v3_base_size: u32 = @offsetOf(GameplayHostV3, "get_twod_api");
pub const host_v3_twod_size: u32 = @offsetOf(GameplayHostV3, "get_twod_api") + @sizeOf(@FieldType(GameplayHostV3, "get_twod_api"));
pub const host_v3_pointer_size: u32 = @offsetOf(GameplayHostV3, "capture_pointer_input") + @sizeOf(@FieldType(GameplayHostV3, "capture_pointer_input"));

pub fn capturePointerInput(host: *const GameplayHostV3, user: u32, output: *PointerInputSnapshotV1) i32 {
    if (host.struct_size < host_v3_pointer_size or host.abi_version != abi_version or
        (host.capabilities & pointer_input_capability) == 0) return -2;
    const capture = host.capture_pointer_input orelse return -2;
    return capture(host.context, user, output);
}

pub const Vec2 = extern struct { x: f32 = 0, y: f32 = 0 };
pub const TwoDSprite = extern struct {
    atlas_high: u64 = 0,
    atlas_low: u64 = 0,
    region: u32 = 0,
    flags: u32 = 8,
    size: Vec2 = .{ .x = 1, .y = 1 },
    pivot: Vec2 = .{ .x = 0.5, .y = 0.5 },
    color: [4]f32 = .{ 1, 1, 1, 1 },
    layer: i32 = 0,
    order: i32 = 0,
};
pub const TwoDCollider = extern struct {
    kind: u32 = 0,
    offset: Vec2 = .{},
    half_extent: Vec2 = .{ .x = 0.5, .y = 0.5 },
    radius: f32 = 0.5,
    group: u32 = 1,
    mask: u32 = 0xffffffff,
};
pub const TwoDCamera = extern struct {
    center: Vec2 = .{},
    vertical_size: f32 = 20,
    rotation_radians: f32 = 0,
};
pub const TwoDShape = extern struct {
    exclude_id: u64 = 0,
    geometry: TwoDCollider = .{},
};
pub const TwoDSweep = extern struct {
    from: Vec2 = .{},
    to: Vec2 = .{},
    radius: f32 = 0,
    group: u32 = 1,
    mask: u32 = 0xffffffff,
};
pub const TwoDHit = extern struct {
    id: u64 = 0,
    fraction: f64 = 0,
};
pub const TwoDEvent = extern struct {
    length: u32 = 0,
    name: [256]u8 = @splat(0),
};
pub const TwoDAnimationResult = extern struct {
    region: u32 = 0,
    finished: u32 = 0,
    event_count: u32 = 0,
    reserved: u32 = 0,
};
pub const TwoDTile = extern struct {
    x: i32 = 0,
    y: i32 = 0,
    layer: i32 = 0,
    region: u32 = 0,
    collision: u32 = 0,
};
pub const TwoDApiV1 = extern struct {
    struct_size: u32 = @sizeOf(TwoDApiV1),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    set_sprite: ?*const fn (?*anyopaque, u64, ?*const TwoDSprite) callconv(.c) i32 = null,
    get_sprite: ?*const fn (?*anyopaque, u64, *TwoDSprite) callconv(.c) i32 = null,
    set_collider: ?*const fn (?*anyopaque, u64, ?*const TwoDCollider) callconv(.c) i32 = null,
    get_collider: ?*const fn (?*anyopaque, u64, *TwoDCollider) callconv(.c) i32 = null,
    set_camera: ?*const fn (?*anyopaque, *const TwoDCamera) callconv(.c) i32 = null,
    get_camera: ?*const fn (?*anyopaque, *TwoDCamera) callconv(.c) i32 = null,
    play_animation: ?*const fn (?*anyopaque, u64, u64, u64, f64) callconv(.c) i32 = null,
    pause_animation: ?*const fn (?*anyopaque, u64, u32) callconv(.c) i32 = null,
    advance_animation: ?*const fn (?*anyopaque, u64, f64, ?[*]TwoDEvent, u32, *TwoDAnimationResult) callconv(.c) i32 = null,
    rebuild_queries: ?*const fn (?*anyopaque, u32) callconv(.c) i32 = null,
    overlap: ?*const fn (?*anyopaque, u32, *const TwoDShape, ?[*]u64, u32, *u32) callconv(.c) i32 = null,
    sweep: ?*const fn (?*anyopaque, u32, *const TwoDSweep, ?[*]TwoDHit, u32, *u32) callconv(.c) i32 = null,
    paint_tiles: ?*const fn (?*anyopaque, ?[*]const TwoDTile, u32) callconv(.c) i32 = null,
    set_chunk_loaded: ?*const fn (?*anyopaque, i32, i32, u32) callconv(.c) i32 = null,
    get_tile: ?*const fn (?*anyopaque, i32, i32, i32, *TwoDTile) callconv(.c) i32 = null,
    create_instances: ?*const fn (?*anyopaque, ?[*]const TwoDInstance, u32, ?[*]TwoDInstanceHandle) callconv(.c) i32 = null,
    destroy_instances: ?*const fn (?*anyopaque, ?[*]const TwoDInstanceHandle, u32) callconv(.c) i32 = null,
    read_instances: ?*const fn (?*anyopaque, ?[*]const TwoDInstanceHandle, u32, ?[*]TwoDInstance) callconv(.c) i32 = null,
    write_instances: ?*const fn (?*anyopaque, ?[*]const TwoDInstanceHandle, u32, ?[*]const TwoDInstance) callconv(.c) i32 = null,
};

pub const TwoDInstanceHandle = extern struct {
    owner: u64 = 0,
    index: u32 = 0,
    generation: u32 = 0,
};
pub const TwoDInstance = extern struct {
    transform: TransformV2 = .{},
    sprite: TwoDSprite = .{},
};

/// Always inspect the V3 prefix before reading the optional appended callback.
pub fn getTwoD(host: *const GameplayHostV3) ?TwoDApiV1 {
    if (host.abi_version != abi_version or host.struct_size < host_v3_twod_size or
        (host.capabilities & twod_api_capability) == 0) return null;
    const get = host.get_twod_api orelse return null;
    var api = TwoDApiV1{};
    if (get(host.context, &api) != 0 or api.abi_version != 1 or
        api.struct_size < @sizeOf(TwoDApiV1)) return null;
    inline for (@typeInfo(TwoDApiV1).@"struct".fields[3..]) |field| {
        if (@field(api, field.name) == null) return null;
    }
    return api;
}

pub fn componentType(comptime text: []const u8) u64 {
    var hash: u64 = 14695981039346656037;
    for (text) |byte| {
        hash ^= byte;
        hash *%= 1099511628211;
    }
    return hash;
}

pub const GameModuleV3 = extern struct {
    struct_size: u32,
    abi_version: u32,
    capabilities: u64,
    module_state: ?*anyopaque,
    create: ?*const fn (*?*anyopaque, *const GameplayHostV3) callconv(.c) i32,
    on_start: ?*const fn (?*anyopaque) callconv(.c) i32,
    fixed_update: ?*const fn (?*anyopaque, f64) callconv(.c) i32,
    update: ?*const fn (?*anyopaque, f64) callconv(.c) i32,
    on_stop: ?*const fn (?*anyopaque) callconv(.c) void,
    destroy: ?*const fn (?*anyopaque) callconv(.c) void,
    save_state: ?*const fn (?*anyopaque, ?*anyopaque, u32) callconv(.c) u32,
    load_state: ?*const fn (?*anyopaque, ?*const anyopaque, u32) callconv(.c) i32,
};

pub const GameModuleLoadV3 = *const fn (u32, ?*GameModuleV3) callconv(.c) i32;
