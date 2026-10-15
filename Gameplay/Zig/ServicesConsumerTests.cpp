#include "Nexora/Runtime/GameplayAudio.h"
#include "Nexora/Runtime/GameplayInput.h"
#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/Runtime/GameplayStorage.h"
#include "Nexora/TwoD/GameplayContent.h"
#include "Nexora/TwoD/GameplayUi.h"
#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <thread>
extern "C" uint64_t NexoraServicesLayout(uint32_t, uint32_t);
extern "C" int32_t NexoraServicesProbe(const NexoraGameplayHostV3 *);
extern "C" int32_t NexoraServicesExercise(const NexoraGameplayHostV3 *, uint32_t);
extern "C" int32_t NexoraContentRegister(const NexoraGameplayHostV3 *, uint32_t, const void *,
                                         uint64_t, uint64_t *, uint64_t *);
int main() {
  {
    const std::array<std::size_t, 10> expected{
        sizeof(NexoraGameplayServicesApiV1),
        alignof(NexoraGameplayServicesApiV1),
        offsetof(NexoraGameplayServicesApiV1, struct_size),
        offsetof(NexoraGameplayServicesApiV1, abi_version),
        offsetof(NexoraGameplayServicesApiV1, context),
        offsetof(NexoraGameplayServicesApiV1, get_input_api),
        offsetof(NexoraGameplayServicesApiV1, get_ui_api),
        offsetof(NexoraGameplayServicesApiV1, get_audio_api),
        offsetof(NexoraGameplayServicesApiV1, get_storage_api),
        offsetof(NexoraGameplayServicesApiV1, get_content_api)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(0, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 10> expected{sizeof(NexoraGameplayInputApiV1),
                                               alignof(NexoraGameplayInputApiV1),
                                               offsetof(NexoraGameplayInputApiV1, struct_size),
                                               offsetof(NexoraGameplayInputApiV1, abi_version),
                                               offsetof(NexoraGameplayInputApiV1, context),
                                               offsetof(NexoraGameplayInputApiV1, read_action),
                                               offsetof(NexoraGameplayInputApiV1, bind),
                                               offsetof(NexoraGameplayInputApiV1, clear_bindings),
                                               offsetof(NexoraGameplayInputApiV1, read_device),
                                               offsetof(NexoraGameplayInputApiV1, read_pointer)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(1, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 10> expected{sizeof(NexoraGameplayInputBindingV1),
                                               alignof(NexoraGameplayInputBindingV1),
                                               offsetof(NexoraGameplayInputBindingV1, struct_size),
                                               offsetof(NexoraGameplayInputBindingV1, abi_version),
                                               offsetof(NexoraGameplayInputBindingV1, device),
                                               offsetof(NexoraGameplayInputBindingV1, device_id),
                                               offsetof(NexoraGameplayInputBindingV1, control),
                                               offsetof(NexoraGameplayInputBindingV1, scale),
                                               offsetof(NexoraGameplayInputBindingV1, deadzone),
                                               offsetof(NexoraGameplayInputBindingV1, reserved)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(2, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 9> expected{sizeof(NexoraGameplayActionStateV1),
                                              alignof(NexoraGameplayActionStateV1),
                                              offsetof(NexoraGameplayActionStateV1, struct_size),
                                              offsetof(NexoraGameplayActionStateV1, abi_version),
                                              offsetof(NexoraGameplayActionStateV1, sequence),
                                              offsetof(NexoraGameplayActionStateV1, value),
                                              offsetof(NexoraGameplayActionStateV1, held),
                                              offsetof(NexoraGameplayActionStateV1, pressed),
                                              offsetof(NexoraGameplayActionStateV1, released)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(3, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 9> expected{sizeof(NexoraGameplayDeviceStateV1),
                                              alignof(NexoraGameplayDeviceStateV1),
                                              offsetof(NexoraGameplayDeviceStateV1, struct_size),
                                              offsetof(NexoraGameplayDeviceStateV1, abi_version),
                                              offsetof(NexoraGameplayDeviceStateV1, sequence),
                                              offsetof(NexoraGameplayDeviceStateV1, active_device),
                                              offsetof(NexoraGameplayDeviceStateV1, device_id),
                                              offsetof(NexoraGameplayDeviceStateV1, focused),
                                              offsetof(NexoraGameplayDeviceStateV1, reserved)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(4, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 13> expected{sizeof(NexoraGameplayUiApiV1),
                                               alignof(NexoraGameplayUiApiV1),
                                               offsetof(NexoraGameplayUiApiV1, struct_size),
                                               offsetof(NexoraGameplayUiApiV1, abi_version),
                                               offsetof(NexoraGameplayUiApiV1, context),
                                               offsetof(NexoraGameplayUiApiV1, begin_frame),
                                               offsetof(NexoraGameplayUiApiV1, panel),
                                               offsetof(NexoraGameplayUiApiV1, text),
                                               offsetof(NexoraGameplayUiApiV1, button),
                                               offsetof(NexoraGameplayUiApiV1, end_frame),
                                               offsetof(NexoraGameplayUiApiV1, get_font),
                                               offsetof(NexoraGameplayUiApiV1, has_glyph),
                                               offsetof(NexoraGameplayUiApiV1, register_font)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(5, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 6> expected{
        sizeof(NexoraUiRect),      alignof(NexoraUiRect),         offsetof(NexoraUiRect, x),
        offsetof(NexoraUiRect, y), offsetof(NexoraUiRect, width), offsetof(NexoraUiRect, height)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(6, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 15> expected{sizeof(NexoraGameplayAudioApiV1),
                                               alignof(NexoraGameplayAudioApiV1),
                                               offsetof(NexoraGameplayAudioApiV1, struct_size),
                                               offsetof(NexoraGameplayAudioApiV1, abi_version),
                                               offsetof(NexoraGameplayAudioApiV1, context),
                                               offsetof(NexoraGameplayAudioApiV1, play),
                                               offsetof(NexoraGameplayAudioApiV1, stop),
                                               offsetof(NexoraGameplayAudioApiV1, set_paused),
                                               offsetof(NexoraGameplayAudioApiV1, set_gain),
                                               offsetof(NexoraGameplayAudioApiV1, set_bus_gain),
                                               offsetof(NexoraGameplayAudioApiV1, is_playing),
                                               offsetof(NexoraGameplayAudioApiV1, output_available),
                                               offsetof(NexoraGameplayAudioApiV1, stop_all),
                                               offsetof(NexoraGameplayAudioApiV1, register_wave),
                                               offsetof(NexoraGameplayAudioApiV1, remove_asset)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(7, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 6> expected{
        sizeof(NexoraAudioPlayDesc),          alignof(NexoraAudioPlayDesc),
        offsetof(NexoraAudioPlayDesc, asset), offsetof(NexoraAudioPlayDesc, bus),
        offsetof(NexoraAudioPlayDesc, loop),  offsetof(NexoraAudioPlayDesc, gain)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(8, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 9> expected{sizeof(NexoraStorageApiV1),
                                              alignof(NexoraStorageApiV1),
                                              offsetof(NexoraStorageApiV1, struct_size),
                                              offsetof(NexoraStorageApiV1, abi_version),
                                              offsetof(NexoraStorageApiV1, context),
                                              offsetof(NexoraStorageApiV1, save),
                                              offsetof(NexoraStorageApiV1, load),
                                              offsetof(NexoraStorageApiV1, remove),
                                              offsetof(NexoraStorageApiV1, exists)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(9, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 6> expected{sizeof(NexoraStorageInfoV1),
                                              alignof(NexoraStorageInfoV1),
                                              offsetof(NexoraStorageInfoV1, struct_size),
                                              offsetof(NexoraStorageInfoV1, abi_version),
                                              offsetof(NexoraStorageInfoV1, schema_version),
                                              offsetof(NexoraStorageInfoV1, payload_size)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(10, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 14> expected{sizeof(NexoraUiFontV1),
                                               alignof(NexoraUiFontV1),
                                               offsetof(NexoraUiFontV1, struct_size),
                                               offsetof(NexoraUiFontV1, width),
                                               offsetof(NexoraUiFontV1, height),
                                               offsetof(NexoraUiFontV1, reserved),
                                               offsetof(NexoraUiFontV1, texture_id),
                                               offsetof(NexoraUiFontV1, pixel_size),
                                               offsetof(NexoraUiFontV1, line_height),
                                               offsetof(NexoraUiFontV1, rgba),
                                               offsetof(NexoraUiFontV1, rgba_length),
                                               offsetof(NexoraUiFontV1, glyphs),
                                               offsetof(NexoraUiFontV1, glyph_count),
                                               offsetof(NexoraUiFontV1, reserved2)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(11, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 12> expected{sizeof(NexoraUiGlyph),
                                               alignof(NexoraUiGlyph),
                                               offsetof(NexoraUiGlyph, codepoint),
                                               offsetof(NexoraUiGlyph, advance),
                                               offsetof(NexoraUiGlyph, x),
                                               offsetof(NexoraUiGlyph, y),
                                               offsetof(NexoraUiGlyph, width),
                                               offsetof(NexoraUiGlyph, height),
                                               offsetof(NexoraUiGlyph, u0),
                                               offsetof(NexoraUiGlyph, v0),
                                               offsetof(NexoraUiGlyph, u1),
                                               offsetof(NexoraUiGlyph, v1)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(12, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 13> expected{sizeof(NexoraGameplayPointerStateV1),
                                               alignof(NexoraGameplayPointerStateV1),
                                               offsetof(NexoraGameplayPointerStateV1, struct_size),
                                               offsetof(NexoraGameplayPointerStateV1, abi_version),
                                               offsetof(NexoraGameplayPointerStateV1, sequence),
                                               offsetof(NexoraGameplayPointerStateV1, x),
                                               offsetof(NexoraGameplayPointerStateV1, y),
                                               offsetof(NexoraGameplayPointerStateV1, delta_x),
                                               offsetof(NexoraGameplayPointerStateV1, delta_y),
                                               offsetof(NexoraGameplayPointerStateV1, wheel_x),
                                               offsetof(NexoraGameplayPointerStateV1, wheel_y),
                                               offsetof(NexoraGameplayPointerStateV1, buttons),
                                               offsetof(NexoraGameplayPointerStateV1, focused)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(13, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 9> expected{sizeof(NexoraGameplayContentApiV1),
                                              alignof(NexoraGameplayContentApiV1),
                                              offsetof(NexoraGameplayContentApiV1, struct_size),
                                              offsetof(NexoraGameplayContentApiV1, abi_version),
                                              offsetof(NexoraGameplayContentApiV1, context),
                                              offsetof(NexoraGameplayContentApiV1, register_atlas),
                                              offsetof(NexoraGameplayContentApiV1, register_clip),
                                              offsetof(NexoraGameplayContentApiV1, list_assets),
                                              offsetof(NexoraGameplayContentApiV1, describe_asset)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(14, j) == expected[j]);
  }
  {
    const std::array<std::size_t, 10> expected{sizeof(NexoraContentAsset),
                                               alignof(NexoraContentAsset),
                                               offsetof(NexoraContentAsset, high),
                                               offsetof(NexoraContentAsset, low),
                                               offsetof(NexoraContentAsset, kind),
                                               offsetof(NexoraContentAsset, count),
                                               offsetof(NexoraContentAsset, width),
                                               offsetof(NexoraContentAsset, height),
                                               offsetof(NexoraContentAsset, atlas_high),
                                               offsetof(NexoraContentAsset, atlas_low)};
    for (uint32_t j = 0; j < expected.size(); ++j)
      assert(NexoraServicesLayout(15, j) == expected[j]);
  }
  using namespace nexora;
  runtime::GameplayServices services;
  NexoraGameplayHostV3 host{};
  host.struct_size = sizeof(host);
  host.abi_version = 3;
  host.capabilities = NEXORA_GAMEPLAY_CAPABILITY_SERVICES_API;
  host.context = &services;
  host.get_services_api = [](void *p, NexoraGameplayServicesApiV1 *api) {
    return static_cast<runtime::GameplayServices *>(p)->GetApi(api);
  };
  for (uint32_t size :
       {uint32_t(NEXORA_GAMEPLAY_HOST_V3_BASE_SIZE), uint32_t(NEXORA_GAMEPLAY_HOST_V3_2D_SIZE),
        uint32_t(NEXORA_GAMEPLAY_HOST_V3_POINTER_SIZE)}) {
    host.struct_size = size;
    alignas(NexoraGameplayHostV3) std::array<std::byte, sizeof(host)> bytes{};
    std::memcpy(bytes.data(), &host, size);
    assert(NexoraServicesProbe(reinterpret_cast<NexoraGameplayHostV3 *>(bytes.data())) == -2);
  }
  host.struct_size = sizeof(host);
  auto missing = host;
  missing.capabilities = 0;
  assert(NexoraServicesProbe(&missing) == -2);
  missing = host;
  missing.get_services_api = nullptr;
  assert(NexoraServicesProbe(&missing) == -2);
  assert(NexoraServicesProbe(&host) == 0);
  NexoraGameplayServicesApiV1 aggregate{};
  aggregate.struct_size = sizeof(aggregate);
  aggregate.abi_version = 1;
  assert(NexoraGetGameplayServicesV1(&host, &aggregate) == 0);
  NexoraGameplayInputApiV1 absent{};
  absent.struct_size = sizeof(absent);
  absent.abi_version = 1;
  const auto before = absent;
  assert(aggregate.get_input_api(aggregate.context, &absent) == -2);
  assert(std::memcmp(&absent, &before, sizeof(before)) == 0);
  twod::GameplayContent content;
  services.content.struct_size = sizeof(services.content);
  services.content.abi_version = 1;
  assert(content.GetApi(&services.content) == 0);
  twod::Image image{1, 1, 1, twod::Bytes(4, std::byte{255})};
  auto atlas = twod::PackAtlas({101, 102}, std::span(&image, 1), 8, 8, true);
  assert(atlas);
  const auto atlasBytes = twod::EncodeAtlas(*atlas);
  uint64_t assetHigh{}, assetLow{};
  assert(NexoraContentRegister(&host, 1, atlasBytes.data(), atlasBytes.size(), &assetHigh,
                               &assetLow) == 0);
  assert(assetHigh == 101 && assetLow == 102 && content.ResolveAtlas({101, 102}));
  twod::Clip clip{{201, 202}, {101, 102}, true, {{1, 0.1, "step"}}};
  const auto clipBytes = twod::EncodeClip(clip);
  assert(NexoraContentRegister(&host, 2, clipBytes.data(), clipBytes.size(), &assetHigh,
                               &assetLow) == 0);
  assert(assetHigh == 201 && assetLow == 202 && content.ResolveClip({201, 202}));
  runtime::GameplayInput input;
  services.input = input.Api();
  runtime::GameplayAudio audio;
  std::size_t submitted{};
  audio.SetOutput(
      [](void *p, const float *, uint32_t frames, uint32_t) {
        *static_cast<std::size_t *>(p) += frames;
        return true;
      },
      &submitted);
  const std::array<float, 4> samples{.25f, .25f, -.25f, -.25f};
  assert(audio.RegisterPcm(1, 48000, 1, samples));
  services.audio = audio.Api();
  twod::GameplayUi ui;
  services.ui.struct_size = sizeof(services.ui);
  services.ui.abi_version = 1;
  assert(ui.GetApi(&services.ui) == 0);
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("nexora-zig-services-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  {
    runtime::GameplayStorage storage(directory, "zig-acceptance");
    services.storage.struct_size = sizeof(services.storage);
    services.storage.abi_version = 1;
    assert(storage.GetApi(services.storage) == 0);
    assert(input.BeginFrame());
    ui.BeginInput({});
    assert(NexoraServicesExercise(&host, 0) == 0);
  }
  // A new storage owner proves data outlives both the service and its borrowed table.
  {
    runtime::GameplayStorage storage(directory, "zig-acceptance");
    assert(storage.GetApi(services.storage) == 0);
    assert(input.BeginFrame());
    assert(input.Submit(0, NEXORA_INPUT_DEVICE_KEYBOARD, 0, NEXORA_INPUT_KEY_SPACE, 1));
    assert(input.SubmitPointer(0, 10, 20));
    assert(input.SubmitPointer(0, 12, 20, 0, 120));
    twod::GameplayUiInput events;
    events.activate = true;
    ui.BeginInput(events);
    assert(NexoraServicesExercise(&host, 1) == 0);
    const auto frame = ui.Frame();
    assert(!frame.vertices.empty() && !frame.commands.empty());
    assert(audio.Pump(8));
    assert(submitted == 8);
  }
  std::filesystem::remove_all(directory);
  int32_t wrongThread = 0;
  std::thread worker([&] { wrongThread = NexoraGetGameplayServicesV1(&host, &aggregate); });
  worker.join();
  assert(wrongThread == -3);
}
