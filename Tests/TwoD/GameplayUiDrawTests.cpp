#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/TwoD/GameplayUi.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <thread>
using namespace nexora;
int main() {
  twod::GameplayUi owner;
  NexoraGameplayUiApiV1 ui{};
  ui.struct_size = sizeof(ui);
  assert(owner.GetApi(&ui) == 0);
  NexoraGameplayUiDrawApiV1 draw{};
  draw.struct_size = sizeof(draw);
  draw.abi_version = 1;
  assert(owner.GetDrawApi(&draw) == 0);
  runtime::GameplayServices services;
  services.ui = ui;
  services.ui_draw = draw;
  services.display.struct_size = sizeof(services.display);
  services.display.abi_version = 1;
  // SP-01-sized and original callers retain their suffix/prefix without canary writes.
  constexpr auto sp01Size = offsetof(NexoraGameplayServicesApiV1, get_ui_draw_api);
  alignas(NexoraGameplayServicesApiV1)
      std::array<std::byte, sizeof(NexoraGameplayServicesApiV1) + 16>
          storage;
  for (const auto prefix : {std::size_t(NEXORA_GAMEPLAY_SERVICES_V1_BASE_SIZE), sp01Size,
                            sizeof(NexoraGameplayServicesApiV1)}) {
    storage.fill(std::byte{0x5a});
    auto *api = reinterpret_cast<NexoraGameplayServicesApiV1 *>(storage.data());
    const std::uint32_t header[]{static_cast<std::uint32_t>(prefix), 1};
    std::memcpy(storage.data(), header, sizeof(header));
    assert(services.GetApi(api) == 0 && api->struct_size == prefix);
    for (auto i = prefix; i < storage.size(); ++i)
      assert(storage[i] == std::byte{0x5a});
    if (prefix == sp01Size) {
      NexoraGameplayDisplayApiV1 display{};
      display.struct_size = sizeof(display);
      display.abi_version = 1;
      assert(api->get_display_api(api->context, &display) == 0);
    }
  }
  std::array<std::uint8_t, 64> pixels;
  pixels.fill(255);
  NexoraUiImageV1 image{sizeof(image), 4, 4, 0, 100, pixels.data(), pixels.size()};
  std::uint64_t handle = 987;
  image.reserved = 1;
  assert(draw.register_image(draw.context, &image, &handle) == -1 && handle == 987);
  image.reserved = 0;
  assert(draw.register_image(draw.context, &image, &handle) == 0);
  const auto oldHandle = handle;
  pixels.fill(0); // copied data, not a borrowed source buffer
  owner.BeginInput({});
  assert(ui.begin_frame(ui.context, 200, 150) == 0);
  assert(draw.remove_image(draw.context, handle) == -3);
  NexoraUiImageDrawV1 command{sizeof(command), 0xffffffff,        handle,
                              {0, 0, 4, 4},    {10, 20, 100, 80}, {15, 25, 80, 60}};
  auto invalid = command;
  invalid.source.width = 5;
  assert(draw.image(draw.context, &invalid) == -1);
  assert(draw.nine_slice(draw.context, &command, {3, 0, 3, 0}, {}) == -1);
  assert(draw.nine_slice(draw.context, &command, {1, 1, 1, 1}, {12, 10, 12, 10}) == 0);
  assert(ui.end_frame(ui.context) == 0);
  auto snapshot = owner.Frame();
  assert(snapshot.vertices.size() == 36 && snapshot.indices.size() == 54 &&
         snapshot.image_owners.size() == 1);
  assert(snapshot.image_owners[0]->rgba[0] == std::byte{255});
  assert(snapshot.vertices[0].position[0] == 10 && snapshot.vertices[0].uv[0] == 0);
  assert(snapshot.vertices[1].position[0] == 22 && snapshot.vertices[1].uv[0] == .25F);
  for (const auto dpi : {1.F, 1.25F, 1.5F, 1.75F}) {
    auto scaled = snapshot;
    scaled.ToPixels(dpi, 1000, 1000);
    assert(scaled.vertices[0].position[0] == 10 * dpi);
    assert(scaled.commands[0].clipX == int(std::floor(15 * dpi)));
  }
  assert(draw.remove_image(draw.context, handle) == 0);
  owner.BeginInput({}); // releases the internal frame, but not our retained snapshot
  assert(owner.CollectRetiredTextures().empty());
  assert(ui.begin_frame(ui.context, 200, 150) == 0);
  assert(draw.image(draw.context, &command) == -2); // stale handles never alias a replacement
  assert(ui.end_frame(ui.context) == 0);
  snapshot = {};
  const auto retired = owner.CollectRetiredTextures();
  assert(retired.size() == 1 && retired[0] == 100);
  assert(owner.CollectRetiredTextures().empty());
  assert(draw.register_image(draw.context, &image, &handle) == -1); // immutable ID cannot be reused
  image.texture_id = 101;
  assert(draw.register_image(draw.context, &image, &handle) == 0 && handle != oldHandle);
  command.image = handle;
  command.destination.width = command.destination.height = 1;
  owner.BeginInput({});
  assert(ui.begin_frame(ui.context, 200, 150) == 0);
  assert(draw.nine_slice(draw.context, &command, {1, 1, 1, 1}, {12, 10, 12, 10}) == 0);
  assert(ui.end_frame(ui.context) == 0);
  for (const auto &v : owner.Frame().vertices)
    assert(std::isfinite(v.position[0]) && v.position[0] >= 10 && v.position[0] <= 11);
  auto interact = [&](twod::GameplayUiInput input, std::uint32_t &flags) {
    owner.BeginInput(input);
    assert(ui.begin_frame(ui.context, 200, 150) == 0);
    assert(draw.interact(draw.context, 1, {0, 0, 40, 40}, 1, &flags) == 0);
    std::uint32_t disabled = 99;
    assert(draw.interact(draw.context, 2, {50, 0, 40, 40}, 0, &disabled) == 0 && disabled == 0);
    std::uint32_t other = 99;
    assert(draw.interact(draw.context, 3, {100, 0, 40, 40}, 1, &other) == 0);
    assert(ui.end_frame(ui.context) == 0);
    assert(owner.Frame().commands.empty()); // styling belongs to the caller
    return other;
  };
  std::uint32_t flags = 0;
  twod::GameplayUiInput input;
  input.pointer_x = 10;
  input.pointer_y = 10;
  input.pointer_pressed = true;
  interact(input, flags);
  assert(flags & NEXORA_UI_FOCUSED);
  input.pointer_pressed = false;
  input.pointer_released = true;
  interact(input, flags);
  assert(flags & NEXORA_UI_ACTIVATED);
  input = {};
  input.navigate = 1;
  input.activate = true;
  assert(interact(input, flags) & NEXORA_UI_ACTIVATED); // skips disabled ID 2
  input = {};
  input.focused = false;
  input.activate = true;
  assert(interact(input, flags) == 0 && flags == 0);
  int wrong = 0;
  std::thread thread([&] { wrong = draw.remove_image(draw.context, handle); });
  thread.join();
  assert(wrong == -3);
  // Releasable real glyph metrics use monotonic handles and automatic immutable IDs.
  NexoraUiGlyph glyph{0x4e2d, 16, 0, 1, 16, 16, 0, 0, 1, 1};
  NexoraUiFontV1 font{sizeof(font), 1, 1, 0, 0, 16, 20, pixels.data(), 4, &glyph, 1, 0};
  std::uint64_t fontHandle = 77;
  assert(draw.register_font(draw.context, &font, &fontHandle) == 0);
  NexoraUiGlyph metric{};
  assert(draw.get_glyph(draw.context, fontHandle, 0x4e2d, &metric) == 0 && metric.advance == 16);
  metric.advance = 99;
  assert(draw.get_glyph(draw.context, fontHandle, 0x4e00, &metric) == -2 && metric.advance == 99);
  owner.BeginInput({});
  assert(ui.begin_frame(ui.context, 200, 150) == 0);
  assert(ui.text(ui.context, fontHandle, 16, {0, 0, 100, 100}, "中", 3, UINT32_MAX) == 0);
  assert(draw.remove_font(draw.context, fontHandle) == -3);
  assert(ui.end_frame(ui.context) == 0);
  auto retainedFont = owner.Frame();
  assert(draw.remove_font(draw.context, fontHandle) == 0);
  owner.BeginInput({});
  assert(owner.CollectRetiredTextures().empty());
  assert(draw.get_glyph(draw.context, fontHandle, 0x4e2d, &metric) == -2);
  retainedFont = {};
  assert(owner.CollectRetiredTextures().size() == 1);
  for (unsigned i = 0; i < 40; ++i) {
    std::uint64_t fresh{};
    assert(draw.register_font(draw.context, &font, &fresh) == 0 && fresh > fontHandle);
    assert(draw.remove_font(draw.context, fresh) == 0);
    assert(owner.CollectRetiredTextures().size() == 1);
  }
  // Repeated removals return CPU/GPU ownership without reusing live or stale handles.
  assert(draw.remove_image(draw.context, handle) == 0);
  owner.BeginInput({});
  for (unsigned i = 0; i < 300; ++i) {
    image.texture_id = 1000 + i;
    assert(draw.register_image(draw.context, &image, &handle) == 0);
    assert(draw.remove_image(draw.context, handle) == 0);
    assert(!owner.CollectRetiredTextures().empty());
  }
}
