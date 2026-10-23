//! Explicit sRGB authoring -> existing straight-alpha linear RGBA8 runtime contract.
//! No allocation, ICC conversion, premultiplication or implicit texture tagging.
const std = @import("std");
pub fn srgbByteToLinear(value: u8) f32 {
    const c = @as(f64, @floatFromInt(value)) / 255.0;
    return @floatCast(if (c <= 0.04045) c / 12.92 else std.math.pow(f64, (c + 0.055) / 1.055, 2.4));
}
fn linearByte(value: u8) u8 {
    return @intFromFloat(@floor(@as(f64, srgbByteToLinear(value)) * 255.0 + 0.5));
}
/// In-place RGB decode; alpha (including hidden RGB at alpha zero) is preserved independently.
/// Rejects incomplete pixels before modifying the caller's buffer. Do not decode cooked Atlases.
pub fn decodeRgba8(pixels: []u8) !void {
    if (pixels.len % 4 != 0) return error.InvalidRgba;
    var offset: usize = 0;
    while (offset < pixels.len) : (offset += 4) {
        for (0..3) |channel| pixels[offset + channel] = linearByte(pixels[offset + channel]);
    }
}
/// Sprite tint keeps float precision; opacity is linear coverage, never gamma decoded.
pub fn spriteTint(rgba: [4]u8) [4]f32 {
    return .{ srgbByteToLinear(rgba[0]), srgbByteToLinear(rgba[1]), srgbByteToLinear(rgba[2]), @as(f32, @floatFromInt(rgba[3])) / 255.0 };
}
/// UI V1 packs linear UNORM8 with R in the low byte. Separate UI scale/DPI from color.
pub fn uiTint(rgba: [4]u8) u32 {
    return @as(u32, linearByte(rgba[0])) | (@as(u32, linearByte(rgba[1])) << 8) |
        (@as(u32, linearByte(rgba[2])) << 16) | (@as(u32, rgba[3]) << 24);
}
test "IEC transfer, all alpha values, legacy buffer ownership and invalid lengths" {
    try std.testing.expectApproxEqAbs(@as(f32, 0.2158605), srgbByteToLinear(128), 0.000001);
    try std.testing.expectEqual(@as(u32, 0x80373737), uiTint(.{ 128, 128, 128, 128 }));
    var pixels: [256 * 4]u8 = undefined;
    for (0..256) |i| @memcpy(pixels[i * 4 ..][0..4], &[_]u8{ @intCast(i), 128, 255, @intCast(i) });
    try decodeRgba8(&pixels);
    var previous: u8 = 0;
    var hash: u32 = 2166136261;
    for (0..256) |i| {
        try std.testing.expect(pixels[i * 4] >= previous);
        previous = pixels[i * 4];
        hash = (hash ^ pixels[i * 4]) *% 16777619;
        try std.testing.expectEqual(@as(u8, @intCast(i)), pixels[i * 4 + 3]);
        try std.testing.expectEqual(@as(u8, 55), pixels[i * 4 + 1]);
        try std.testing.expectEqual(@as(u8, 255), pixels[i * 4 + 2]);
    }
    // Golden hash of all 256 IEC transfer values, shared with the Python importer.
    try std.testing.expectEqual(@as(u32, 0x32d9e413), hash);
    var invalid = [_]u8{ 128, 128, 128 };
    try std.testing.expectError(error.InvalidRgba, decodeRgba8(&invalid));
    try std.testing.expectEqualSlices(u8, &.{ 128, 128, 128 }, &invalid);
    try std.testing.expectEqual(@as(f32, 128.0 / 255.0), spriteTint(.{ 0, 0, 0, 128 })[3]);
}
