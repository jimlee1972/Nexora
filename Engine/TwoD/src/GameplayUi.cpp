#include "Nexora/TwoD/GameplayUi.h"
#include "nexora/nexora.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <thread>

namespace nexora::twod {
namespace {
constexpr int Ok = NEXORA_GAMEPLAY_OK, Invalid = NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
              Missing = NEXORA_GAMEPLAY_ERROR_UNSUPPORTED,
              Lifecycle = NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
bool Finite(float f) { return std::isfinite(f) && std::abs(f) <= 1e6F; }
bool Rect(NexoraUiRect r) {
  return Finite(r.x) && Finite(r.y) && Finite(r.width) && Finite(r.height) && r.width >= 0 &&
         r.height >= 0;
}
bool Inside(NexoraUiRect r, float x, float y) {
  return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}
bool Decode(const char *bytes, std::uint32_t length, std::vector<std::uint32_t> &out) {
  if ((!bytes && length) || length > 65536)
    return false;
  for (std::uint32_t i = 0; i < length;) {
    const auto first = static_cast<unsigned char>(bytes[i++]);
    unsigned count = 0;
    std::uint32_t cp = first, minimum = 0;
    if (first >= 0xC2 && first <= 0xDF) {
      count = 1;
      cp &= 31;
      minimum = 0x80;
    } else if (first >= 0xE0 && first <= 0xEF) {
      count = 2;
      cp &= 15;
      minimum = 0x800;
    } else if (first >= 0xF0 && first <= 0xF4) {
      count = 3;
      cp &= 7;
      minimum = 0x10000;
    } else if (first >= 0x80 || first == 0)
      return false;
    if (count > length - i)
      return false;
    for (unsigned n = 0; n < count; ++n) {
      const auto b = static_cast<unsigned char>(bytes[i++]);
      if ((b & 0xC0) != 0x80)
        return false;
      cp = (cp << 6) | (b & 63);
    }
    if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
      return false;
    out.push_back(cp);
  }
  return true;
}
const GameplayUiGlyph *Glyph(const GameplayUiFont &font, std::uint32_t cp) {
  const auto found =
      std::lower_bound(font.glyphs.begin(), font.glyphs.end(), cp,
                       [](const auto &g, auto value) { return g.codepoint < value; });
  return found != font.glyphs.end() && found->codepoint == cp ? &*found : nullptr;
}
} // namespace
Nexora::Presentation::UiDrawData GameplayUiFrame::DrawData() const noexcept {
  return {vertices, std::as_bytes(std::span(indices)), commands, uploads, true, {}};
}
struct GameplayUi::State final {
  std::thread::id thread{std::this_thread::get_id()};
  GameplayUiInput input{};
  bool open{}, ready{};
  float width{}, height{};
  std::uint64_t focus{}, pressed{};
  std::vector<std::uint64_t> previous, current, focusable;
  std::vector<std::shared_ptr<const GameplayUiFont>> fonts;
  std::shared_ptr<const GameplayUiFont> white;
  GameplayUiFrame frame;
  template <class Fn> static int Call(void *context, Fn &&fn) noexcept {
    if (!context)
      return Invalid;
    auto &s = *static_cast<State *>(context);
    if (s.thread != std::this_thread::get_id())
      return Lifecycle;
    try {
      return fn(s);
    } catch (...) {
      return Lifecycle;
    }
  }
  bool Budget(std::size_t quads) const {
    return quads <= 65536 && frame.vertices.size() / 4 <= 65536 - quads;
  }
  void Quad(NexoraUiRect r, NexoraUiRect clip, std::uint64_t texture, std::uint32_t color,
            float u0 = 0.5F, float v0 = 0.5F, float u1 = 0.5F, float v1 = 0.5F) {
    const float left = std::max(0.F, clip.x), top = std::max(0.F, clip.y);
    const float right = std::min(width, clip.x + clip.width),
                bottom = std::min(height, clip.y + clip.height);
    if (right <= left || bottom <= top || r.width == 0 || r.height == 0)
      return;
    const auto base = static_cast<std::uint32_t>(frame.vertices.size());
    const auto offset = static_cast<std::uint32_t>(frame.indices.size());
    frame.vertices.push_back({{r.x, r.y}, {u0, v0}, color});
    frame.vertices.push_back({{r.x + r.width, r.y}, {u1, v0}, color});
    frame.vertices.push_back({{r.x + r.width, r.y + r.height}, {u1, v1}, color});
    frame.vertices.push_back({{r.x, r.y + r.height}, {u0, v1}, color});
    for (auto index : {0u, 1u, 2u, 0u, 2u, 3u})
      frame.indices.push_back(base + index);
    const auto x = static_cast<std::int32_t>(std::floor(left)),
               y = static_cast<std::int32_t>(std::floor(top));
    frame.commands.push_back({x, y, static_cast<std::uint32_t>(std::ceil(right) - x),
                              static_cast<std::uint32_t>(std::ceil(bottom) - y), texture, 6, offset,
                              0, 0, 0});
  }
  int ValidateText(std::uint64_t handle, float size, NexoraUiRect rect, const char *text,
                   std::uint32_t length, std::vector<std::uint32_t> &cp) const {
    if (!Rect(rect) || !Finite(size) || size <= 0 || size > 1024 || !Decode(text, length, cp))
      return Invalid;
    if (!handle || handle > fonts.size())
      return Missing;
    if (!Budget(cp.size() + 1))
      return Missing;
    const auto &font = *fonts[static_cast<std::size_t>(handle - 1)];
    for (auto code : cp)
      if (code != '\n' && !Glyph(font, code))
        return Missing;
    return Ok;
  }
  void Text(std::uint64_t handle, float size, NexoraUiRect rect,
            const std::vector<std::uint32_t> &cp, std::uint32_t color) {
    const auto &font = *fonts[static_cast<std::size_t>(handle - 1)];
    const float scale = size / font.pixel_size;
    float x = rect.x, y = rect.y;
    for (auto code : cp) {
      if (code == '\n') {
        x = rect.x;
        y += font.line_height * scale;
        continue;
      }
      const auto &g = *Glyph(font, code);
      Quad({x + g.x * scale, y + g.y * scale, g.width * scale, g.height * scale}, rect,
           font.texture_id, color, g.u0, g.v0, g.u1, g.v1);
      x += g.advance * scale;
    }
  }
  static int Begin(void *ctx, float w, float h) noexcept {
    return Call(ctx, [&](State &s) {
      if (s.open)
        return Lifecycle;
      if (!Finite(w) || !Finite(h) || w <= 0 || h <= 0 || w > 16384 || h > 16384)
        return Invalid;
      s.frame = {};
      s.current.clear();
      s.focusable.clear();
      s.width = w;
      s.height = h;
      s.open = true;
      s.ready = false;
      if (s.input.focused && s.input.navigate && !s.previous.empty()) {
        const auto it = std::find(s.previous.begin(), s.previous.end(), s.focus);
        const auto count = static_cast<std::ptrdiff_t>(s.previous.size());
        const auto index =
            it == s.previous.end() ? (s.input.navigate > 0 ? -1 : 0) : it - s.previous.begin();
        s.focus = s.previous[static_cast<std::size_t>((index + s.input.navigate + count) % count)];
      }
      return Ok;
    });
  }
  static int Panel(void *ctx, NexoraUiRect rect, std::uint32_t color) noexcept {
    return Call(ctx, [&](State &s) {
      if (!s.open)
        return Lifecycle;
      if (!Rect(rect))
        return Invalid;
      if (!s.Budget(1))
        return Missing;
      s.Quad(rect, rect, s.white->texture_id, color);
      return Ok;
    });
  }
  static int Label(void *ctx, std::uint64_t font, float size, NexoraUiRect rect, const char *text,
                   std::uint32_t length, std::uint32_t color) noexcept {
    return Call(ctx, [&](State &s) {
      if (!s.open)
        return Lifecycle;
      std::vector<std::uint32_t> cp;
      const int result = s.ValidateText(font, size, rect, text, length, cp);
      if (result)
        return result;
      s.Text(font, size, rect, cp, color);
      return Ok;
    });
  }
  static int Button(void *ctx, std::uint64_t id, NexoraUiRect rect, const char *text,
                    std::uint32_t length, std::uint64_t font, float size, std::uint32_t enabled,
                    std::uint32_t *out) noexcept {
    return Call(ctx, [&](State &s) {
      if (!s.open)
        return Lifecycle;
      if (!id || !out || enabled > 1 || s.current.size() >= 4096 ||
          std::find(s.current.begin(), s.current.end(), id) != s.current.end())
        return Invalid;
      std::vector<std::uint32_t> cp;
      const int result = s.ValidateText(font, size, rect, text, length, cp);
      if (result)
        return result;
      s.current.push_back(id);
      if (enabled)
        s.focusable.push_back(id);
      std::uint32_t flags = 0;
      if (enabled && s.input.focused) {
        const bool hovered = Inside(rect, s.input.pointer_x, s.input.pointer_y);
        if (hovered)
          flags |= NEXORA_UI_HOVERED;
        if (hovered && s.input.pointer_pressed) {
          s.focus = id;
          s.pressed = id;
        }
        if (!s.focus)
          s.focus = id;
        if (s.focus == id)
          flags |= NEXORA_UI_FOCUSED;
        if ((s.input.pointer_released && s.pressed == id && hovered) ||
            (s.input.activate && s.focus == id))
          flags |= NEXORA_UI_ACTIVATED;
        if (s.input.cancel && s.focus == id)
          flags |= NEXORA_UI_CANCELLED;
      }
      s.Quad(rect, rect, s.white->texture_id,
             enabled ? (flags & NEXORA_UI_FOCUSED ? 0xff806040u : 0xff302820u) : 0xff202020u);
      s.Text(font, size, rect, cp, enabled ? 0xffffffffu : 0xff808080u);
      *out = flags;
      return Ok;
    });
  }
  static int End(void *ctx) noexcept {
    return Call(ctx, [](State &s) {
      if (!s.open)
        return Lifecycle;
      s.frame.font_owners = s.fonts;
      s.frame.font_owners.push_back(s.white);
      for (const auto &font : s.frame.font_owners)
        s.frame.uploads.push_back({font->texture_id, font->width, font->height, font->width * 4,
                                   font->rgba, false, true});
      s.previous = s.focusable;
      if (std::find(s.focusable.begin(), s.focusable.end(), s.focus) == s.focusable.end())
        s.focus = 0;
      if (s.input.pointer_released || !s.input.focused)
        s.pressed = 0;
      s.open = false;
      s.ready = true;
      // Events belong to one UI frame even if a module begins another frame.
      s.input.pointer_pressed = s.input.pointer_released = s.input.activate = s.input.cancel =
          false;
      s.input.navigate = 0;
      return Ok;
    });
  }
  static int Font(void *ctx, std::uint32_t index, std::uint64_t *out) noexcept {
    return Call(ctx, [&](State &s) {
      if (!out)
        return Invalid;
      if (index >= s.fonts.size())
        return Missing;
      *out = index + 1;
      return Ok;
    });
  }
  static int HasGlyph(void *ctx, std::uint64_t font, std::uint32_t cp,
                      std::uint32_t *out) noexcept {
    return Call(ctx, [&](State &s) {
      if (!out || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return Invalid;
      if (!font || font > s.fonts.size())
        return Missing;
      *out = Glyph(*s.fonts[static_cast<std::size_t>(font - 1)], cp) ? 1u : 0u;
      return Ok;
    });
  }
  bool AddFont(GameplayUiFont font) {
    if (thread != std::this_thread::get_id() || open || fonts.size() >= 32 || !font.texture_id ||
        font.texture_id >= UINT64_MAX - 1 || font.texture_id == white->texture_id ||
        font.width == 0 || font.height == 0 || font.width > 4096 || font.height > 4096 ||
        font.rgba.size() != std::size_t(font.width) * font.height * 4 ||
        font.rgba.size() > 16 * 1024 * 1024 || !Finite(font.pixel_size) || font.pixel_size < 1 ||
        !Finite(font.line_height) || font.line_height <= 0 || font.glyphs.size() > 65536)
      return false;
    for (const auto &existing : fonts)
      if (existing->texture_id == font.texture_id)
        return false;
    std::sort(font.glyphs.begin(), font.glyphs.end(),
              [](const auto &a, const auto &b) { return a.codepoint < b.codepoint; });
    std::uint32_t previousCodepoint = UINT32_MAX;
    for (const auto &g : font.glyphs) {
      if (!g.codepoint || g.codepoint > 0x10FFFF ||
          (g.codepoint >= 0xD800 && g.codepoint <= 0xDFFF) || previousCodepoint == g.codepoint ||
          !Finite(g.advance) || g.advance < 0 || !Finite(g.x) || !Finite(g.y) || !Finite(g.width) ||
          !Finite(g.height) || g.width < 0 || g.height < 0 || !std::isfinite(g.u0) ||
          !std::isfinite(g.v0) || !std::isfinite(g.u1) || !std::isfinite(g.v1) || g.u0 < 0 ||
          g.v0 < 0 || g.u1 > 1 || g.v1 > 1 || g.u0 > g.u1 || g.v0 > g.v1)
        return false;
      previousCodepoint = g.codepoint;
    }
    fonts.push_back(std::make_shared<const GameplayUiFont>(std::move(font)));
    return true;
  }
  static int Register(void *ctx, const NexoraUiFontV1 *in, std::uint64_t *out) noexcept {
    return Call(ctx, [&](State &s) {
      if (!in || !out || in->struct_size < sizeof(*in) || in->reserved || in->reserved2 ||
          !in->width || !in->height || in->width > 4096 || in->height > 4096 ||
          in->rgba_length != std::uint64_t(in->width) * in->height * 4 ||
          in->rgba_length > 16 * 1024 * 1024 || !in->rgba || in->glyph_count > 65536 ||
          (!in->glyphs && in->glyph_count))
        return Invalid;
      if (s.open)
        return Lifecycle;
      if (s.fonts.size() >= 32)
        return Missing;
      GameplayUiFont font;
      font.texture_id = in->texture_id;
      font.width = in->width;
      font.height = in->height;
      font.pixel_size = in->pixel_size;
      font.line_height = in->line_height;
      const auto bytes = reinterpret_cast<const std::byte *>(in->rgba);
      font.rgba.assign(bytes, bytes + static_cast<std::size_t>(in->rgba_length));
      font.glyphs.reserve(in->glyph_count);
      for (std::uint32_t i = 0; i < in->glyph_count; ++i) {
        const auto &g = in->glyphs[i];
        font.glyphs.push_back(
            {g.codepoint, g.advance, g.x, g.y, g.width, g.height, g.u0, g.v0, g.u1, g.v1});
      }
      if (!s.AddFont(std::move(font)))
        return Invalid;
      *out = static_cast<std::uint64_t>(s.fonts.size());
      return Ok;
    });
  }
};
GameplayUi::GameplayUi(std::uint64_t white_texture_id) : state_(std::make_unique<State>()) {
  if (!white_texture_id || white_texture_id >= UINT64_MAX - 1)
    throw std::invalid_argument("UI texture ID");
  auto font = std::make_shared<GameplayUiFont>();
  font->texture_id = white_texture_id;
  font->width = font->height = 1;
  font->rgba.assign(4, std::byte{255});
  state_->white = std::move(font);
}
GameplayUi::~GameplayUi() = default;
int32_t GameplayUi::GetApi(NexoraGameplayUiApiV1 *out) noexcept {
  return State::Call(state_.get(), [&](State &) {
    if (!out)
      return Invalid;
    if (out->struct_size < sizeof(*out))
      return Missing;
    *out = {sizeof(*out),  1,          state_.get(), State::Begin,    State::Panel,   State::Label,
            State::Button, State::End, State::Font,  State::HasGlyph, State::Register};
    return Ok;
  });
}
bool GameplayUi::RegisterFont(GameplayUiFont font) { return state_->AddFont(std::move(font)); }
void GameplayUi::BeginInput(GameplayUiInput input) {
  auto &s = *state_;
  if (s.thread != std::this_thread::get_id() || s.open)
    throw std::logic_error("UI input lifecycle");
  if (!Finite(input.pointer_x) || !Finite(input.pointer_y) || input.navigate < -1 ||
      input.navigate > 1)
    throw std::invalid_argument("UI input");
  s.input = input;
  s.ready = false;
  if (!input.focused) {
    s.focus = 0;
    s.pressed = 0;
  }
}
void GameplayUi::CancelFrame() {
  auto &s = *state_;
  if (s.thread != std::this_thread::get_id())
    throw std::logic_error("UI owner thread");
  s.open = s.ready = false;
  s.focus = s.pressed = 0;
  s.frame = {};
  s.previous.clear();
  s.current.clear();
  s.focusable.clear();
  s.input = {};
}
GameplayUiFrame GameplayUi::Frame() const {
  if (state_->thread != std::this_thread::get_id())
    throw std::logic_error("UI owner thread");
  return state_->ready ? state_->frame : GameplayUiFrame{};
}
} // namespace nexora::twod
