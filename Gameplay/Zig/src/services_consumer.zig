const nx = @import("nexora");

const types = .{ nx.GameplayServicesApiV1, nx.input.Api, nx.input.Binding, nx.input.ActionState, nx.input.DeviceState, nx.ui.Api, nx.ui.Rect, nx.audio.Api, nx.audio.PlayDesc, nx.storage.Api, nx.storage.Info, nx.ui.Font, nx.ui.Glyph, nx.input.PointerState, nx.content.Api, nx.content.Asset };
export fn NexoraServicesLayout(kind: u32, field: u32) u64 {
    inline for (types, 0..) |T, i| {
        if (kind == i) {
            if (field == 0) return @sizeOf(T);
            if (field == 1) return @alignOf(T);
            inline for (@typeInfo(T).@"struct".fields, 2..) |f, j| {
                if (field == j) return @offsetOf(T, f.name);
            }
        }
    }
    return 0xffffffffffffffff;
}
export fn NexoraServicesProbe(host: *const nx.GameplayHostV3) i32 {
    return if (nx.getServices(host) != null) 0 else -2;
}
// A game defines its own action IDs, save schema and content handles; the host owns IO/devices.
export fn NexoraServicesExercise(host: *const nx.GameplayHostV3, phase: u32) i32 {
    const services = nx.getServices(host) orelse return -2;
    var input = nx.input.Api{};
    var ui = nx.ui.Api{};
    var audio = nx.audio.Api{};
    var storage = nx.storage.Api{};
    if (services.get_input_api.?(services.context, &input) != 0 or
        services.get_ui_api.?(services.context, &ui) != 0 or
        services.get_audio_api.?(services.context, &audio) != 0 or
        services.get_storage_api.?(services.context, &storage) != 0) return -10;
    if (phase == 0) {
        const binding = nx.input.Binding{ .device = .keyboard, .control = nx.input.Key.space };
        if (input.rebind(0, 1, &binding) != 0) return -11;
        if (storage.saveSlot("resume", 7, "zig durable state") != 0) return -12;
        // The one-pixel atlas is a wire fixture; production modules supply rasterized glyphs.
        const pixels = [_]u8{ 255, 255, 255, 255 };
        const glyphs = [_]nx.ui.Glyph{.{ .codepoint = 0x4e2d, .advance = 16, .width = 16, .height = 16, .u1 = 1, .v1 = 1 }};
        var registered: u64 = 0;
        if (ui.registerFont(91, 1, 1, 16, 16, &pixels, &glyphs, &registered) != 0 or registered != 1) return -27;
    } else {
        var action = nx.input.ActionState{};
        if (input.action(0, 1, &action) != 0 or action.pressed != 1 or action.held != 1) return -13;
        var pointer = nx.input.PointerState{};
        if (input.pointer(0, &pointer) != 0 or pointer.x != 12 or pointer.delta_x != 2 or pointer.wheel_y != 120) return -28;
        var info = nx.storage.Info{};
        if (storage.probe("resume", &info) != 0 or info.schema_version != 7 or info.payload_size != 17) return -14;
        var payload: [17]u8 = undefined;
        if (storage.loadSlot("resume", &payload, &info) != 0) return -15;
        const expected = "zig durable state";
        for (payload, expected) |a, b| {
            if (a != b) return -16;
        }
    }
    var font: u64 = 0;
    if (ui.get_font.?(ui.context, 0, &font) != 0) return -17;
    if (ui.begin_frame.?(ui.context, 640, 480) != 0) return -18;
    if (ui.panel.?(ui.context, .{ .width = 180, .height = 60 }, 0xff302010) != 0) return -19;
    if (ui.label(font, 16, .{ .x = 4, .y = 4, .width = 100, .height = 20 }, "中", 0xffffffff) != 0) return -20;
    var button: u32 = 0;
    if (ui.menuButton(1, .{ .x = 4, .y = 28, .width = 100, .height = 28 }, "中", font, 16, true, &button) != 0) return -21;
    if (ui.end_frame.?(ui.context) != 0) return -22;
    if (phase == 1 and button & nx.ui.activated == 0) return -23;
    var available: u32 = 0;
    if (audio.outputAvailable(&available) != 0 or available != 1) return -24;
    var voice: u64 = 0;
    if (audio.playSound(1, .music, true, 0.5, &voice) != 0) return -25;
    if (audio.pauseVoice(voice, true) != 0 or audio.setVoiceGain(voice, 0.25) != 0 or audio.pauseVoice(voice, false) != 0) return -26;
    return 0;
}

export fn NexoraContentRegister(host: *const nx.GameplayHostV3, kind: u32, bytes: [*]const u8, size: u64, high: *u64, low: *u64) i32 {
    const services = nx.getServices(host) orelse return -2;
    var content = nx.content.Api{};
    if (services.get_content_api.?(services.context, &content) != 0) return -2;
    if (kind == 1) return content.register_atlas.?(content.context, bytes, size, high, low);
    if (kind == 2) return content.register_clip.?(content.context, bytes, size, high, low);
    return -1;
}
