#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/TwoD/GameplayDisplay.h"
#include "Nexora/TwoD/GameplayUi.h"
#include "Nexora/TwoD/Scene.h"
#include "Nexora/Window/Geometry.h"
#include <cassert>
#include <limits>
#include <thread>
using namespace nexora;
int main() {
  int requests{};
  twod::GameplayDisplay display([&](const auto &) {
    ++requests;
    return -3;
  });
  auto api = display.Api();
  display.BeginFrame(1920, 1080, 1.5F, 0);
  NexoraDisplayStateV1 state{};
  state.struct_size = sizeof(state);
  state.abi_version = 1;
  assert(api.get_state(api.context, &state) == 0 && state.logical_width == 1280);
  const auto revision = state.revision;
  assert(api.set_aspect(api.context, 16.F / 9, 21.F / 9) == 0);
  assert(display.State().revision == revision); // No mutation of the active snapshot.
  for (const auto &[w, h] :
       {std::pair{1920U, 1080U}, {2520U, 1080U}, {1024U, 768U}, {3441U, 1081U}, {801U, 601U}}) {
    for (float dpi : {1.F, 1.25F, 1.5F, 1.75F}) {
      display.BeginFrame(w, h, dpi, 0);
      const auto &s = display.State();
      const auto v = s.viewport_pixels;
      assert(v.x >= 0 && v.y >= 0 && v.x + v.width <= w && v.y + v.height <= h);
      assert(std::abs(v.width / v.height - std::clamp(float(w) / h, 16.F / 9, 21.F / 9)) < .01F);
      assert(std::abs(s.viewport_logical.x * dpi - v.x) < .001F);
      const auto unchanged = s.revision;
      display.BeginFrame(w, h, dpi, 0);
      assert(display.State().revision == unchanged);
      for (float scale : {1.F, 1.5F}) {
        twod::GameplayUi ui;
        NexoraGameplayUiApiV1 u{};
        u.struct_size = sizeof(u);
        u.abi_version = 1;
        assert(ui.GetApi(&u) == 0);
        assert(u.begin_frame(u.context, s.logical_width, s.logical_height) == 0);
        assert(u.panel(u.context, {10, 20, 100 * scale, 40 * scale}, 0xffffffff) == 0);
        assert(u.end_frame(u.context) == 0);
        auto frame = ui.Frame();
        frame.ToPixels(dpi, w, h);
        assert(frame.vertices[0].position[0] == 10 * dpi);
        assert(frame.vertices[2].position[1] == (20 + 40 * scale) * dpi);
        for (const auto &c : frame.commands)
          assert(c.clipX >= 0 && c.clipY >= 0 && c.clipX + c.clipWidth <= w &&
                 c.clipY + c.clipHeight <= h);
        twod::GameplayUiFont font;
        font.texture_id = 42;
        font.width = font.height = 1;
        font.pixel_size = font.line_height = 16;
        font.rgba.assign(4, std::byte{255});
        font.glyphs = {{'A', 8, 0, 0, 8, 16, 0, 0, 1, 1}};
        assert(ui.RegisterFont(font));
        const auto rawX = 15 * dpi, rawY = 25 * dpi;
        for (int phase = 0; phase < 2; ++phase) {
          twod::GameplayUiInput input;
          input.pointer_x = rawX / dpi;
          input.pointer_y = rawY / dpi;
          input.pointer_down = input.pointer_pressed = phase == 0;
          input.pointer_released = phase == 1;
          ui.BeginInput(input);
          assert(u.begin_frame(u.context, s.logical_width, s.logical_height) == 0);
          uint32_t state{};
          assert(u.button(u.context, 1, {10, 20, 100 * scale, 40 * scale}, "A", 1, 1, 16 * scale, 1,
                          &state) == 0);
          assert((state & NEXORA_UI_HOVERED) != 0);
          assert(((state & NEXORA_UI_ACTIVATED) != 0) == (phase == 1));
          assert(u.end_frame(u.context) == 0);
        }
      }
    }
  }
  NexoraDisplayWindowV1 request{sizeof(request), 1, 1920, 1080, 0, 1};
  assert(api.request_window(api.context, &request) == -3 && requests == 1);
  request.width = 0;
  assert(api.request_window(api.context, &request) == -1 && requests == 1);
  assert(api.set_aspect(api.context, std::numeric_limits<float>::quiet_NaN(), 2) == -1);
  assert(api.set_aspect(api.context, 2, 1) == -1);
  int wrong{};
  std::thread worker([&] { wrong = api.get_state(api.context, &state); });
  worker.join();
  assert(wrong == -3);
  display.BeginFrame(0, 0, 1, 0);
  assert(display.State().viewport_pixels.width == 0);
  runtime::GameplayServices services;
  services.display = api;
  // The historical service prefix has exactly five getters. A canary catches over-copy.
  struct OldServices {
    uint32_t size, version;
    void *context;
    void (*getters[5])();
    uint64_t canary;
  } old{uint32_t(NEXORA_GAMEPLAY_SERVICES_V1_BASE_SIZE), 1, nullptr, {}, 0xfeedface};
  assert(offsetof(OldServices, canary) == NEXORA_GAMEPLAY_SERVICES_V1_BASE_SIZE);
  assert(services.GetApi(reinterpret_cast<NexoraGameplayServicesApiV1 *>(&old)) == 0);
  assert(old.canary == 0xfeedface && old.size == NEXORA_GAMEPLAY_SERVICES_V1_BASE_SIZE);
  NexoraGameplayServicesApiV1 aggregate{};
  aggregate.struct_size = sizeof(aggregate);
  aggregate.abi_version = 1;
  assert(services.GetApi(&aggregate) == 0 && aggregate.get_display_api);
  NexoraGameplayDisplayApiV1 output{};
  output.struct_size = sizeof(output);
  output.abi_version = 1;
  assert(aggregate.get_display_api(aggregate.context, &output) == 0);
  services.display = {};
  const auto before = output;
  assert(aggregate.get_display_api(aggregate.context, &output) == -2);
  assert(std::memcmp(&before, &output, sizeof(output)) == 0);
  const auto fit = Nexora::Window::FitClientExtent(1920, 1080, 1366, 728, 16, 39);
  assert(fit && fit->first + 16 <= 1366 && fit->second + 39 <= 728);
  assert(std::abs(double(fit->first) / fit->second - 16. / 9) < .01);
  assert(!Nexora::Window::FitClientExtent(1920, 1080, 10, 10, 16, 39));
  const twod::Image image{1, 1, 1, twod::Bytes(4, std::byte{255})};
  const auto atlas =
      std::make_shared<twod::Atlas>(*twod::PackAtlas({1, 1}, std::span(&image, 1), 8, 8));
  twod::Sprite sprite;
  sprite.atlas = atlas->id;
  sprite.region = 1;
  const auto matrix = runtime::ToMatrix(runtime::Transform{});
  const twod::SpriteInstance instance{1, sprite, matrix};
  const twod::AtlasResolver resolver = [&](auto) -> std::optional<twod::AtlasBinding> {
    return twod::AtlasBinding{atlas, 21};
  };
  for (float span : {24.F, 28.F}) {
    const twod::Camera camera{{}, span, 0, 1920, 1080};
    auto extracted = twod::ExtractSprites(std::span(&instance, 1), camera, resolver);
    assert(extracted && extracted->statistics.visible == 1);
    extracted->OffsetViewport(0, 60);
    double x{}, y{};
    for (const auto &v : extracted->vertices) {
      x += v.position[0];
      y += v.position[1];
    }
    assert(std::abs(x / 4 - 960) < .001 && std::abs(y / 4 - 600) < .001);
    assert(extracted->batches[0].clipY == 60 && extracted->batches[0].clipHeight == 1080);
    const auto prepared = Nexora::Presentation::PrepareSpriteDraws(
        {extracted->vertices, std::as_bytes(std::span(extracted->indices)), extracted->batches,
         extracted->Uploads(), true},
        {}, 1920, 1200, true, 0xff000000);
    assert(prepared && prepared->vertices[0].color == 0xff000000);
  }
  twod::SpriteFrame projected;
  projected.vertices.push_back({{10, 20}, {0, 0}, 0xffffffff});
  projected.batches.push_back({0, 0, 1920, 1080, 1, 6, 0, 0});
  projected.OffsetViewport(0, 60);
  assert(projected.vertices[0].position[1] == 80 && projected.batches[0].clipY == 60);
}
