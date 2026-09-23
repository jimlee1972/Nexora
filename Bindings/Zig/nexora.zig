//! Thin Zig declarations for the canonical `nexora/nexora.h` ABI.
//! Field order is checked against Engine/API/abi_baseline_v3.json in CTest.
pub const abi_version: u32 = 3;

pub const Result = enum(i32) {
    ok = 0,
    invalid_argument = -1,
    unsupported = -2,
    lifecycle = -3,
};

pub const scene_api_capability: u64 = 1 << 3;

pub const AllocationOwner = enum(u64) {
    unknown = 0,
    gameplay_state = 1,
    state_migration = 2,
};


pub const Vec3 = extern struct { x: f64 = 0, y: f64 = 0, z: f64 = 0 };
pub const AssetHandle = extern struct { value: u64 = 0 };
pub const EntitySpawnDescriptor = extern struct {
    struct_size: u32 = @sizeOf(EntitySpawnDescriptor), components: u32 = 0,
    position: Vec3 = .{}, camera_fov_degrees: f64 = 0, light_intensity: f32 = 0,
    reserved: u32 = 0, mesh: AssetHandle = .{}, material: AssetHandle = .{},
    bounds_minimum: Vec3 = .{}, bounds_maximum: Vec3 = .{},
};
pub const InputSnapshot = extern struct { sequence: u64 = 0, move_x: f64 = 0, move_y: f64 = 0, buttons: u32 = 0, reserved: u32 = 0 };
pub const RaycastRequest = extern struct { origin: Vec3 = .{}, direction: Vec3 = .{}, distance: f64 = 0 };
pub const RaycastHit = extern struct { entity: u64 = 0, distance: f64 = 0, point: Vec3 = .{} };
pub const DebugLine = extern struct { start: Vec3 = .{}, end: Vec3 = .{}, rgba: u32 = 0, duration_seconds: f32 = 0 };
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
};

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
