#include "Nexora/TwoD/GameplayUi.h"
#include "nexora/nexora.h"
#include <cassert>
#include <limits>
#include <thread>
using namespace nexora;
namespace {
// Deterministic raster fixture verifies atlas ownership and UTF-8 lookup, not font appearance.
twod::GameplayUiFont Font() {
  twod::GameplayUiFont font;
  font.texture_id = 42;
  font.width = font.height = 2;
  font.pixel_size = font.line_height = 16;
  font.rgba.assign(16, std::byte{255});
  font.glyphs = {{0x4E2D, 16, 0, 0, 16, 16, 0, 0, 1, 1}, {'A', 8, 0, 0, 8, 16, 0, 0, 0.5F, 1}};
  return font;
}
NexoraGameplayUiApiV1 Api(twod::GameplayUi &ui) {
  NexoraGameplayUiApiV1 api{};
  api.struct_size = sizeof(api);
  assert(ui.GetApi(&api) == 0);
  assert(api.abi_version == 1);
  return api;
}
constexpr NexoraUiRect first{0, 0, 40, 30}, second{0, 40, 40, 30};
} // namespace
void RegistrationWire() {
  twod::GameplayUi ui;
  auto api = Api(ui);
  auto source = Font();
  std::vector<NexoraUiGlyph> glyphs;
  for (const auto &g : source.glyphs)
    glyphs.push_back({g.codepoint, g.advance, g.x, g.y, g.width, g.height, g.u0, g.v0, g.u1, g.v1});
  NexoraUiFontV1 font{sizeof(NexoraUiFontV1),
                      source.width,
                      source.height,
                      0,
                      source.texture_id,
                      source.pixel_size,
                      source.line_height,
                      reinterpret_cast<const std::uint8_t *>(source.rgba.data()),
                      source.rgba.size(),
                      glyphs.data(),
                      static_cast<std::uint32_t>(glyphs.size()),
                      0};
  std::uint64_t out = 99;
  auto bad = font;
  bad.glyphs = nullptr;
  assert(api.register_font(api.context, &bad, &out) == -1 && out == 99);
  bad = font;
  bad.glyph_count = 65537;
  assert(api.register_font(api.context, &bad, &out) == -1 && out == 99);
  bad = font;
  bad.rgba_length = UINT64_MAX;
  assert(api.register_font(api.context, &bad, &out) == -1 && out == 99);
  bad = font;
  bad.rgba = nullptr;
  assert(api.register_font(api.context, &bad, &out) == -1 && out == 99);
  bad = font;
  bad.struct_size = 4;
  assert(api.register_font(api.context, &bad, &out) == -1 && out == 99);
  assert(api.register_font(api.context, nullptr, &out) == -1 && out == 99);
  assert(api.register_font(api.context, &font, nullptr) == -1);
  assert(api.register_font(api.context, &font, &out) == 0 && out == 1);
  assert(api.register_font(api.context, &font, &out) == -1 && out == 1);
  source.rgba.assign(source.rgba.size(), std::byte{0});
  glyphs.clear();
  ui.BeginInput({});
  assert(api.begin_frame(api.context, 100, 100) == 0);
  assert(api.register_font(api.context, &font, &out) == -3 && out == 1);
  assert(api.text(api.context, out, 16, first, "A", 1, 0xffffffff) == 0);
  assert(api.end_frame(api.context) == 0);
  auto frame = ui.Frame();
  assert(frame.uploads[0].pixels[0] == std::byte{255});
}
int main() {
  RegistrationWire();
  twod::GameplayUiFrame retained;
  {
    twod::GameplayUi ui;
    auto api = Api(ui);
    auto bad = Font();
    bad.glyphs.push_back(bad.glyphs[0]);
    assert(!ui.RegisterFont(bad));
    bad = Font();
    bad.glyphs[0].u1 = 2;
    assert(!ui.RegisterFont(bad));
    bad = Font();
    bad.rgba.pop_back();
    assert(!ui.RegisterFont(bad));
    assert(ui.RegisterFont(Font()));
    assert(!ui.RegisterFont(Font()));
    std::uint64_t font = 99;
    assert(api.get_font(api.context, 0, &font) == 0 && font == 1);
    assert(api.get_font(api.context, 1, &font) == -2 && font == 1);
    std::uint32_t present = 9;
    assert(api.has_glyph(api.context, font, 0x4E2D, &present) == 0 && present == 1);
    assert(api.has_glyph(api.context, font, 0x4E00, &present) == 0 && present == 0);
    assert(api.has_glyph(api.context, font, 0xD800, &present) == -1 && present == 0);
    assert(api.panel(api.context, first, 0xffffffff) == -3);
    assert(api.begin_frame(api.context, std::numeric_limits<float>::quiet_NaN(), 100) == -1);
    ui.BeginInput({});
    assert(api.begin_frame(api.context, 100, 100) == 0);
    assert(api.begin_frame(api.context, 100, 100) == -3);
    assert(api.panel(api.context, {-10, -10, 30, 30}, 0xffffffff) == 0);
    assert(api.text(api.context, font, 16, first, "A\xE4\xB8\xAD", 4, 0xffffffff) == 0);
    const auto before = ui.Frame();
    assert(before.vertices.empty());
    assert(api.text(api.context, font, 16, first, "\xE0\x80\x80", 3, 0xffffffff) == -1);
    assert(api.text(api.context, font, 16, first, "\xED\xA0\x80", 3, 0xffffffff) == -1);
    assert(api.text(api.context, font, 16, first, "\xF4\x90\x80\x80", 4, 0xffffffff) == -1);
    assert(api.text(api.context, font, 16, first, "B", 1, 0xffffffff) == -2);
    std::uint32_t a = 99, b = 99;
    assert(api.button(api.context, 1, first, "A", 1, font, 16, 1, &a) == 0 && (a & 2));
    assert(api.button(api.context, 1, first, "A", 1, font, 16, 1, &a) == -1);
    assert(api.button(api.context, 2, second, "A", 1, font, 16, 1, &b) == 0 && !(b & 2));
    assert(api.end_frame(api.context) == 0);
    auto frame = ui.Frame();
    assert(frame.vertices.size() == 28 && frame.indices.size() == 42);
    assert(frame.commands[0].clipX == 0 && frame.commands[0].clipWidth == 20);
    assert(frame.commands[1].textureId == 42 && frame.vertices[8].uv[0] == 0);
    assert(frame.uploads.size() == 2 && frame.DrawData().indices32Bit);
    const auto native =
        Nexora::Presentation::PrepareSpriteDraws({}, frame.DrawData(), 100, 100, true);
    assert(native && native->commands.size() == frame.commands.size() + 1);
    assert(native->textureUploads.size() == 3);
    // Previous-frame visual order drives keyboard and controller menu navigation.
    twod::GameplayUiInput input{};
    input.navigate = 1;
    input.activate = true;
    ui.BeginInput(input);
    assert(api.begin_frame(api.context, 100, 100) == 0);
    assert(api.button(api.context, 1, first, "A", 1, font, 16, 1, &a) == 0 && !(a & 4));
    assert(api.button(api.context, 2, second, "A", 1, font, 16, 1, &b) == 0 && (b & 2) && (b & 4));
    assert(api.end_frame(api.context) == 0);
    // Press outside/release inside does not click. Press inside then release inside does.
    input = {};
    input.pointer_x = 5;
    input.pointer_y = 5;
    input.pointer_released = true;
    ui.BeginInput(input);
    assert(api.begin_frame(api.context, 100, 100) == 0);
    assert(api.button(api.context, 1, first, "A", 1, font, 16, 1, &a) == 0 && !(a & 4));
    assert(api.end_frame(api.context) == 0);
    input.pointer_released = false;
    input.pointer_pressed = true;
    input.pointer_down = true;
    ui.BeginInput(input);
    assert(api.begin_frame(api.context, 100, 100) == 0);
    assert(api.button(api.context, 1, first, "A", 1, font, 16, 1, &a) == 0 && (a & 2));
    assert(api.end_frame(api.context) == 0);
    input.pointer_pressed = false;
    input.pointer_down = false;
    input.pointer_released = true;
    ui.BeginInput(input);
    assert(api.begin_frame(api.context, 100, 100) == 0);
    assert(api.button(api.context, 1, first, "A", 1, font, 16, 1, &a) == 0 && (a & 4));
    assert(api.end_frame(api.context) == 0);
    input.focused = false;
    input.activate = true;
    ui.BeginInput(input);
    assert(api.begin_frame(api.context, 100, 100) == 0);
    assert(api.button(api.context, 1, first, "A", 1, font, 16, 1, &a) == 0 && a == 0);
    assert(api.end_frame(api.context) == 0);
    int wrongThread = 0;
    std::thread thread([&] { wrongThread = api.begin_frame(api.context, 100, 100); });
    thread.join();
    assert(wrongThread == -3);
    retained = ui.Frame();
    ui.BeginInput({});
    assert(ui.Frame().vertices.empty());
    assert(api.begin_frame(api.context, 100, 100) == 0);
    ui.CancelFrame();
    assert(api.end_frame(api.context) == -3);
    assert(api.begin_frame(api.context, 100, 100) == 0);
    assert(api.end_frame(api.context) == 0);
  }
  assert(retained.uploads.size() == 2 && retained.uploads[0].pixels[0] == std::byte{255});
  assert(retained.DrawData().commands.size() == 2);
}
