//! Optional independent Voice bus and decoded PCM residency; bytes exclude staging, device copies and allocator overhead.
pub const Usage = extern struct {
    struct_size: u32 = @sizeOf(Usage),
    active_assets: u32 = 0,
    active_voices: u32 = 0,
    retired_generations: u32 = 0,
    active_bytes: u64 = 0,
    retired_bytes: u64 = 0,
    peak_live_bytes: u64 = 0,
    active_limit: u64 = 0,
    live_limit: u64 = 0,
    asset_limit: u32 = 0,
    reserved: u32 = 0,
};
pub const Api = extern struct {
    struct_size: u32 = @sizeOf(Api),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    get_usage: ?*const fn (?*anyopaque, *Usage) callconv(.c) i32 = null,
    play_voice: ?*const fn (?*anyopaque, u64, f32, *u64) callconv(.c) i32 = null,
    set_voice_bus_gain: ?*const fn (?*anyopaque, f32) callconv(.c) i32 = null,
    pub fn playVoice(self: *const Api, asset: u64, gain: f32, voice: *u64) i32 {
        if (!self.valid()) return -2;
        return self.play_voice.?(self.context, asset, gain, voice);
    }
    pub fn setVoiceBusGain(self: *const Api, gain: f32) i32 {
        if (!self.valid()) return -2;
        return self.set_voice_bus_gain.?(self.context, gain);
    }
    pub fn valid(self: *const Api) bool {
        return self.struct_size >= @sizeOf(Api) and self.abi_version == 1 and self.context != null and self.get_usage != null and self.play_voice != null and self.set_voice_bus_gain != null;
    }
    pub fn usage(self: *const Api, out: *Usage) i32 {
        if (!self.valid()) return -2;
        return self.get_usage.?(self.context, out);
    }
};
