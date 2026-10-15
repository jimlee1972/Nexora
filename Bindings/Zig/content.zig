//! Register cooked .nx2atlas / .nx2clip bytes without native paths or gameplay C++.
pub const atlas_kind: u32 = 1;
pub const clip_kind: u32 = 2;
pub const Asset = extern struct {
    high: u64 = 0,
    low: u64 = 0,
    kind: u32 = 0,
    count: u32 = 0,
    width: u32 = 0,
    height: u32 = 0,
    atlas_high: u64 = 0,
    atlas_low: u64 = 0,
};
pub const Api = extern struct {
    struct_size: u32 = @sizeOf(Api),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    register_atlas: ?*const fn (?*anyopaque, ?*const anyopaque, u64, *u64, *u64) callconv(.c) i32 = null,
    register_clip: ?*const fn (?*anyopaque, ?*const anyopaque, u64, *u64, *u64) callconv(.c) i32 = null,
    list_assets: ?*const fn (?*anyopaque, ?[*]Asset, u32, *u32) callconv(.c) i32 = null,
    describe_asset: ?*const fn (?*anyopaque, u64, u64, *Asset) callconv(.c) i32 = null,
    pub fn valid(self: *const Api) bool {
        return self.struct_size >= @sizeOf(Api) and self.abi_version == 1 and
            self.register_atlas != null and self.register_clip != null and self.list_assets != null and self.describe_asset != null;
    }
    pub fn registerAtlas(self: *const Api, cooked: []const u8, high: *u64, low: *u64) i32 {
        if (!self.valid() or cooked.len == 0 or cooked.len > 64 * 1024 * 1024) return -1;
        return self.register_atlas.?(self.context, cooked.ptr, cooked.len, high, low);
    }
    pub fn registerClip(self: *const Api, cooked: []const u8, high: *u64, low: *u64) i32 {
        if (!self.valid() or cooked.len == 0 or cooked.len > 64 * 1024 * 1024) return -1;
        return self.register_clip.?(self.context, cooked.ptr, cooked.len, high, low);
    }
};
