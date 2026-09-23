//! Thin Zig declarations for the canonical `nexora/nexora.h` ABI.
//! Field order is checked against Engine/API/abi_baseline_v3.json in CTest.
pub const abi_version: u32 = 3;

pub const Result = enum(i32) {
    ok = 0,
    invalid_argument = -1,
    unsupported = -2,
    lifecycle = -3,
};

pub const GameplayHostV3 = extern struct {
    struct_size: u32,
    abi_version: u32,
    capabilities: u64,
    context: ?*anyopaque,
    log: ?*const fn (?*anyopaque, u32, [*]const u8, u32) callconv(.c) void,
    read_component: ?*const fn (?*anyopaque, u64, u64, ?*anyopaque, u32) callconv(.c) i32,
    write_component: ?*const fn (?*anyopaque, u64, u64, ?*const anyopaque, u32) callconv(.c) i32,
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
