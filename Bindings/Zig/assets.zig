//! Bounded catalog IO. Ready Atlas/Clip requests transfer content to the registry;
//! releaseRequest forgets the ticket, while remove explicitly unloads its UUID.
pub const CatalogEntry = extern struct {
    struct_size: u32 = @sizeOf(CatalogEntry),
    kind: u32 = 0,
    file_bytes: u64 = 0,
};
pub const LoadState = extern struct {
    struct_size: u32 = @sizeOf(LoadState),
    phase: u32 = 0,
    result: i32 = 0,
    catalog_index: u32 = 0,
    high: u64 = 0,
    low: u64 = 0,
    byte_length: u64 = 0,
};
pub const Usage = extern struct {
    struct_size: u32 = @sizeOf(Usage),
    active_assets: u32 = 0,
    retired_generations: u32 = 0,
    requests: u32 = 0,
    active_bytes: u64 = 0,
    retired_bytes: u64 = 0,
    texture_bytes: u64 = 0,
    staging_bytes: u64 = 0,
    peak_content_bytes: u64 = 0,
};
pub const queued: u32 = 1;
pub const loading: u32 = 2;
pub const ready: u32 = 3;
pub const failed: u32 = 4;
pub const Api = extern struct {
    struct_size: u32 = @sizeOf(Api),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    describe_catalog: ?*const fn (?*anyopaque, u32, *CatalogEntry) callconv(.c) i32 = null,
    request: ?*const fn (?*anyopaque, u32, *u64) callconv(.c) i32 = null,
    poll: ?*const fn (?*anyopaque, u64, *LoadState) callconv(.c) i32 = null,
    release_request: ?*const fn (?*anyopaque, u64) callconv(.c) i32 = null,
    remove_asset: ?*const fn (?*anyopaque, u64, u64) callconv(.c) i32 = null,
    get_usage: ?*const fn (?*anyopaque, *Usage) callconv(.c) i32 = null,
    copy_bytes: ?*const fn (?*anyopaque, u64, ?*anyopaque, u64, *u64) callconv(.c) i32 = null,
    pub fn valid(self: *const Api) bool {
        return self.struct_size >= @sizeOf(Api) and self.abi_version == 1 and self.describe_catalog != null and
            self.request != null and self.poll != null and self.release_request != null and
            self.remove_asset != null and self.get_usage != null and self.copy_bytes != null;
    }
    pub fn requestLoad(self: *const Api, index: u32, ticket: *u64) i32 {
        if (!self.valid()) return -2;
        return self.request.?(self.context, index, ticket);
    }
    pub fn readState(self: *const Api, ticket: u64, state: *LoadState) i32 {
        if (!self.valid()) return -2;
        return self.poll.?(self.context, ticket, state);
    }
    pub fn releaseRequest(self: *const Api, ticket: u64) i32 {
        if (!self.valid()) return -2;
        return self.release_request.?(self.context, ticket);
    }
    pub fn remove(self: *const Api, high: u64, low: u64) i32 {
        if (!self.valid()) return -2;
        return self.remove_asset.?(self.context, high, low);
    }
    pub fn usage(self: *const Api, value: *Usage) i32 {
        if (!self.valid()) return -2;
        return self.get_usage.?(self.context, value);
    }
    pub fn copyBytes(self: *const Api, ticket: u64, buffer: []u8, length: *u64) i32 {
        if (!self.valid()) return -2;
        return self.copy_bytes.?(self.context, ticket, if (buffer.len == 0) null else buffer.ptr, buffer.len, length);
    }
};
