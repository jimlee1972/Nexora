//! Display snapshots use native pixels and DPI-independent logical pixels. World Y points up.
const std = @import("std");
pub const Rect = extern struct { x: f32 = 0, y: f32 = 0, width: f32 = 0, height: f32 = 0 };
pub const State = extern struct {
    struct_size: u32 = @sizeOf(State),
    abi_version: u32 = 1,
    revision: u64 = 0,
    pixel_width: u32 = 0,
    pixel_height: u32 = 0,
    logical_width: f32 = 0,
    logical_height: f32 = 0,
    dpi_scale: f32 = 1,
    mode: u32 = 0,
    viewport_pixels: Rect = .{},
    viewport_logical: Rect = .{},
};
pub const Window = extern struct {
    struct_size: u32 = @sizeOf(Window),
    abi_version: u32 = 1,
    width: u32 = 1920,
    height: u32 = 1080,
    mode: u32 = 0,
    resizable: u32 = 1,
};
pub const Api = extern struct {
    struct_size: u32 = @sizeOf(Api),
    abi_version: u32 = 1,
    context: ?*anyopaque = null,
    get_state: ?*const fn (?*anyopaque, *State) callconv(.c) i32 = null,
    request_window: ?*const fn (?*anyopaque, *const Window) callconv(.c) i32 = null,
    set_aspect: ?*const fn (?*anyopaque, f32, f32) callconv(.c) i32 = null,
    pub fn state(self: Api) ?State {
        var result = State{};
        const get = self.get_state orelse return null;
        return if (get(self.context, &result) == 0) result else null;
    }
    pub fn request(self: Api, window: Window) i32 {
        const call = self.request_window orelse return -2;
        return call(self.context, &window);
    }
    pub fn aspect(self: Api, minimum: f32, maximum: f32) i32 {
        const call = self.set_aspect orelse return -2;
        return call(self.context, minimum, maximum);
    }
};
pub const Point = struct { x: f64, y: f64 };
pub const Camera = struct { x: f64 = 0, y: f64 = 0, vertical_size: f64 = 24, rotation: f64 = 0 };
fn valid(s: State) bool {
    return s.pixel_width > 0 and s.pixel_height > 0 and std.math.isFinite(s.dpi_scale) and s.dpi_scale > 0;
}
fn finite(p: Point) bool {
    return std.math.isFinite(p.x) and std.math.isFinite(p.y);
}
pub fn pixelToLogical(s: State, p: Point) ?Point {
    if (!valid(s) or !finite(p)) return null;
    return .{ .x = p.x / s.dpi_scale, .y = p.y / s.dpi_scale };
}
pub fn logicalToPixel(s: State, p: Point) ?Point {
    if (!valid(s) or !finite(p)) return null;
    return .{ .x = p.x * s.dpi_scale, .y = p.y * s.dpi_scale };
}
fn cameraValid(s: State, c: Camera) bool {
    return valid(s) and s.viewport_pixels.width > 0 and s.viewport_pixels.height > 0 and
        std.math.isFinite(c.vertical_size) and c.vertical_size > 0 and
        finite(.{ .x = c.x, .y = c.y }) and std.math.isFinite(c.rotation);
}
pub fn pixelToWorld(s: State, c: Camera, p: Point) ?Point {
    if (!cameraValid(s, c) or !finite(p)) return null;
    const v = s.viewport_pixels;
    if (p.x < v.x or p.y < v.y or p.x >= v.x + v.width or p.y >= v.y + v.height) return null;
    const units = c.vertical_size / v.height;
    const x = (p.x - v.x - v.width / 2) * units;
    const y = -(p.y - v.y - v.height / 2) * units;
    return .{ .x = c.x + @cos(c.rotation) * x - @sin(c.rotation) * y, .y = c.y + @sin(c.rotation) * x + @cos(c.rotation) * y };
}
pub fn worldToPixel(s: State, c: Camera, p: Point) ?Point {
    if (!cameraValid(s, c) or !finite(p)) return null;
    const v = s.viewport_pixels;
    const scale = v.height / c.vertical_size;
    const x = p.x - c.x;
    const y = p.y - c.y;
    return .{ .x = v.x + v.width / 2 + (@cos(c.rotation) * x + @sin(c.rotation) * y) * scale, .y = v.y + v.height / 2 - (-@sin(c.rotation) * x + @cos(c.rotation) * y) * scale };
}
test "DPI and rotated camera preserve coordinates; bars reject world input" {
    const s = State{ .pixel_width = 1920, .pixel_height = 1200, .dpi_scale = 1.5, .viewport_pixels = .{ .y = 60, .width = 1920, .height = 1080 } };
    const c = Camera{ .x = 7, .y = -3, .rotation = 0.6 };
    const point = Point{ .x = 9, .y = 1 };
    const pixel = worldToPixel(s, c, point).?;
    const restored = pixelToWorld(s, c, pixel).?;
    try std.testing.expectApproxEqAbs(point.x, restored.x, 0.00001);
    try std.testing.expectApproxEqAbs(point.y, restored.y, 0.00001);
    const logical = pixelToLogical(s, pixel).?;
    const again = logicalToPixel(s, logical).?;
    try std.testing.expectApproxEqAbs(pixel.x, again.x, 0.00001);
    try std.testing.expect(pixelToWorld(s, c, .{ .x = 100, .y = 20 }) == null);
    try std.testing.expect(worldToPixel(.{}, c, point) == null);
}
