//! Optional owner-thread audio service. Host owns all assets, voices and output.
//! Return codes: 0 success, -1 argument, -2 output unavailable, -3 stale/missing.
pub const abi_version: u32 = 1;
pub const Result = enum(i32) {
    ok = 0,
    invalid_argument = -1,
    unavailable = -2,
    stale_or_missing = -3,
};
pub const Bus = enum(u32) { master = 0, music = 1, sfx = 2 };
pub const PlayDesc = extern struct {
    asset: u64,
    bus: u32 = @intFromEnum(Bus.sfx),
    loop: u32 = 0,
    gain: f32 = 1,
};
pub const Api = extern struct {
    struct_size: u32 = @sizeOf(Api),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    play: ?*const fn (?*anyopaque, *const PlayDesc, *u64) callconv(.c) i32 = null,
    stop: ?*const fn (?*anyopaque, u64) callconv(.c) i32 = null,
    set_paused: ?*const fn (?*anyopaque, u64, u32) callconv(.c) i32 = null,
    set_gain: ?*const fn (?*anyopaque, u64, f32) callconv(.c) i32 = null,
    set_bus_gain: ?*const fn (?*anyopaque, u32, f32) callconv(.c) i32 = null,
    is_playing: ?*const fn (?*anyopaque, u64, *u32) callconv(.c) i32 = null,
    output_available: ?*const fn (?*anyopaque, *u32) callconv(.c) i32 = null,
    stop_all: ?*const fn (?*anyopaque) callconv(.c) i32 = null,
    register_wave: ?*const fn (?*anyopaque, u64, ?*const anyopaque, u32) callconv(.c) i32 = null,
    remove_asset: ?*const fn (?*anyopaque, u64) callconv(.c) i32 = null,

    pub fn valid(self: *const Api) bool {
        return self.struct_size >= @sizeOf(Api) and self.abi_version == 1 and self.context != null and
            self.play != null and self.stop != null and self.set_paused != null and self.set_gain != null and
            self.set_bus_gain != null and self.is_playing != null and self.output_available != null and self.stop_all != null and self.register_wave != null and self.remove_asset != null;
    }
    /// Returns an opaque voice ID only on success. Music and SFX are playable buses.
    pub fn playSound(self: *const Api, asset: u64, bus: Bus, looping: bool, gain: f32, voice: *u64) i32 {
        if (!self.valid() or bus == .master) return @intFromEnum(Result.invalid_argument);
        const desc = PlayDesc{ .asset = asset, .bus = @intFromEnum(bus), .loop = @intFromBool(looping), .gain = gain };
        return self.play.?(self.context, &desc, voice);
    }
    pub fn stopVoice(self: *const Api, voice: u64) i32 {
        if (!self.valid()) return @intFromEnum(Result.invalid_argument);
        return self.stop.?(self.context, voice);
    }
    pub fn pauseVoice(self: *const Api, voice: u64, paused: bool) i32 {
        if (!self.valid()) return @intFromEnum(Result.invalid_argument);
        return self.set_paused.?(self.context, voice, @intFromBool(paused));
    }
    pub fn setVoiceGain(self: *const Api, voice: u64, gain: f32) i32 {
        if (!self.valid()) return @intFromEnum(Result.invalid_argument);
        return self.set_gain.?(self.context, voice, gain);
    }
    pub fn setBusGain(self: *const Api, bus: Bus, gain: f32) i32 {
        if (!self.valid()) return @intFromEnum(Result.invalid_argument);
        return self.set_bus_gain.?(self.context, @intFromEnum(bus), gain);
    }
    /// Includes paused voices; completed/stale IDs report zero without an error.
    pub fn isPlaying(self: *const Api, voice: u64, output: *u32) i32 {
        if (!self.valid()) return @intFromEnum(Result.invalid_argument);
        return self.is_playing.?(self.context, voice, output);
    }
    pub fn outputAvailable(self: *const Api, output: *u32) i32 {
        if (!self.valid()) return @intFromEnum(Result.invalid_argument);
        return self.output_available.?(self.context, output);
    }
    pub fn stopAll(self: *const Api) i32 {
        if (!self.valid()) return @intFromEnum(Result.invalid_argument);
        return self.stop_all.?(self.context);
    }
    /// Copies decoded samples into host storage. Suitable for @embedFile WAV bytes.
    /// Maximum encoded payload 64 MiB; existing voices retain their sample data.
    pub fn registerWave(self: *const Api, asset: u64, bytes: []const u8) i32 {
        if (!self.valid() or bytes.len == 0 or bytes.len > 64 * 1024 * 1024) return @intFromEnum(Result.invalid_argument);
        return self.register_wave.?(self.context, asset, bytes.ptr, @intCast(bytes.len));
    }
    pub fn removeAsset(self: *const Api, asset: u64) i32 {
        if (!self.valid()) return @intFromEnum(Result.invalid_argument);
        return self.remove_asset.?(self.context, asset);
    }
};
