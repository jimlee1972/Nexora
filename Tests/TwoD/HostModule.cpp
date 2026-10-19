#include "nexora/nexora.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

namespace {
struct State {
  NexoraGameplayHostV3 host;
  NexoraGameplayInputApiV1 input{};
  NexoraGameplayAudioApiV1 audio{};
  NexoraGameplayUiApiV1 ui{};
  NexoraStorageApiV1 storage{};
  uint64_t font{}, music{}, sfx{};
  double position{}, saved{};
  unsigned rebound{}, saves{}, opens{}, pauses{}, plays{}, stops{}, cancels{};
  unsigned keyboardMovement{}, gamepadMovement{}, buttonsSeen{}, busChanges{};
  std::array<float, 6> axesMin{}, axesMax{};
  std::array<float, 6> axes{};
  std::array<unsigned, 3> gains{};
  bool paused{};
};
int32_t BindMovement(State &s) {
  if (s.input.clear_bindings(s.input.context, 0, 250))
    return -1;
  for (const auto binding : std::array<NexoraGameplayInputBindingV1, 3>{
           {{sizeof(NexoraGameplayInputBindingV1), 1, NEXORA_INPUT_DEVICE_KEYBOARD, 0,
             static_cast<uint32_t>(s.rebound ? NEXORA_INPUT_KEY_F : NEXORA_INPUT_KEY_D), 1, 0, 0},
            {sizeof(NexoraGameplayInputBindingV1), 1, NEXORA_INPUT_DEVICE_KEYBOARD, 0,
             NEXORA_INPUT_KEY_A, -1, 0, 0},
            {sizeof(NexoraGameplayInputBindingV1), 1, NEXORA_INPUT_DEVICE_GAMEPAD, 0, 32, 1, .2F,
             0}}})
    if (s.input.bind(s.input.context, 0, 250, &binding))
      return -1;
  return 0;
}
std::array<unsigned char, 12> Encode(const State &s) {
  std::array<unsigned char, 12> bytes{};
  const auto value = std::bit_cast<uint64_t>(s.position);
  for (unsigned i = 0; i < 8; ++i)
    bytes[i] = static_cast<unsigned char>(value >> (i * 8));
  bytes[8] = static_cast<unsigned char>(s.rebound);
  return bytes;
}
int32_t Save(State &s) {
  const auto bytes = Encode(s);
  const auto result = s.storage.save(s.storage.context, "host", 4, 1, bytes.data(),
                                     static_cast<uint32_t>(bytes.size()));
  if (!result)
    s.saved = s.position;
  return result;
}
int32_t Load(State &s) {
  std::array<unsigned char, 12> bytes{};
  NexoraStorageInfoV1 info{sizeof(info), 1, 0, 0};
  const auto result = s.storage.load(s.storage.context, "host", 4, bytes.data(),
                                     static_cast<uint32_t>(bytes.size()), &info);
  if (result == NEXORA_STORAGE_NOT_FOUND)
    return 0;
  if (result || info.schema_version != 1 || info.payload_size != bytes.size() || bytes[8] > 1 ||
      bytes[9] || bytes[10] || bytes[11])
    return -1;
  uint64_t value{};
  for (unsigned i = 0; i < 8; ++i)
    value |= uint64_t(bytes[i]) << (i * 8);
  const auto position = std::bit_cast<double>(value);
  if (!std::isfinite(position) || std::abs(position) > 10000)
    return -1;
  s.position = s.saved = position;
  s.rebound = bytes[8];
  return BindMovement(s);
}
int32_t Create(void **out, const NexoraGameplayHostV3 *host) {
  if (!host || !out || !host->allocate || !host->deallocate)
    return -1;
  auto *memory = host->allocate(host->context, 1, sizeof(State), alignof(State));
  if (!memory)
    return -3;
  *out = new (memory) State{*host};
  return 0;
}
int32_t Start(void *opaque) {
  auto &s = *static_cast<State *>(opaque);
  NexoraGameplayServicesApiV1 services{};
  services.struct_size = sizeof(services);
  services.abi_version = 1;
  s.input.struct_size = sizeof(s.input);
  s.input.abi_version = 1;
  s.audio.struct_size = sizeof(s.audio);
  s.audio.abi_version = 1;
  s.ui.struct_size = sizeof(s.ui);
  s.ui.abi_version = 1;
  s.storage.struct_size = sizeof(s.storage);
  s.storage.abi_version = 1;
  uint64_t scene{};
  if (s.host.load_scene(s.host.context, "Main", 4, 0, &scene) ||
      s.host.activate_scene(s.host.context, scene) ||
      NexoraGetGameplayServicesV1(&s.host, &services) ||
      services.get_input_api(services.context, &s.input) ||
      services.get_audio_api(services.context, &s.audio) ||
      services.get_ui_api(services.context, &s.ui) ||
      services.get_storage_api(services.context, &s.storage) ||
      s.ui.get_font(s.ui.context, 0, &s.font) || BindMovement(s) || Load(s))
    return -3;
  for (unsigned i = 0; i < 22; ++i) {
    NexoraGameplayInputBindingV1 binding{
        sizeof(binding), 1, NEXORA_INPUT_DEVICE_GAMEPAD, 0, i < 16 ? i : 32 + i - 16, 1, .01F, 0};
    if (s.input.bind(s.input.context, 0, 100 + i, &binding))
      return -3;
  }
  std::fprintf(stderr, "host fixture loaded position=%.6f rebound=%u\n", s.position, s.rebound);
  return 0;
}
bool Button(State &s, uint64_t id, float x, float y, const char *label) {
  uint32_t flags{};
  if (const auto result =
          s.ui.button(s.ui.context, id, {x, y, 184, 34}, label,
                      static_cast<uint32_t>(std::strlen(label)), s.font, 18, 1, &flags)) {
    std::fprintf(stderr, "host fixture button %llu failed %d\n",
                 static_cast<unsigned long long>(id), result);
    throw 1;
  }
  if (flags & NEXORA_UI_CANCELLED)
    ++s.cancels;
  return (flags & NEXORA_UI_ACTIVATED) != 0;
}
int32_t Update(void *opaque, double delta) {
  auto &s = *static_cast<State *>(opaque);
  try {
    NexoraGameplayActionStateV1 action{};
    action.struct_size = sizeof(action);
    action.abi_version = 1;
    NexoraGameplayDeviceStateV1 device{};
    device.struct_size = sizeof(device);
    device.abi_version = 1;
    if (s.input.read_action(s.input.context, 0, 250, &action) ||
        s.input.read_device(s.input.context, 0, &device)) {
      std::fprintf(stderr, "host fixture input failed\n");
      return -3;
    }
    if (device.focused && action.value) {
      s.position += double(action.value) * delta;
      s.position = std::fmax(-10000., std::fmin(10000., s.position));
      if (device.active_device == NEXORA_INPUT_DEVICE_KEYBOARD)
        ++s.keyboardMovement;
      if (device.active_device == NEXORA_INPUT_DEVICE_GAMEPAD)
        ++s.gamepadMovement;
    }
    for (unsigned i = 0; i < 22; ++i) {
      if (s.input.read_action(s.input.context, 0, 100 + i, &action)) {
        std::fprintf(stderr, "host fixture action %u failed\n", 100 + i);
        return -3;
      }
      if (i < 16 && action.held)
        s.buttonsSeen |= 1U << i;
      if (i >= 16) {
        s.axes[i - 16] = action.value;
        s.axesMin[i - 16] = std::fmin(s.axesMin[i - 16], action.value);
        s.axesMax[i - 16] = std::fmax(s.axesMax[i - 16], action.value);
      }
    }
    auto &ui = s.ui;
    if (ui.begin_frame(ui.context, 640, 480) || ui.panel(ui.context, {0, 0, 640, 480}, 0xff302820))
      return -3;
    char status[160]{};
    std::snprintf(status, sizeof(status), "POSITION %.3f SAVED %.3f KEY %s", s.position, s.saved,
                  s.rebound ? "F" : "D");
    static constexpr char title[] = "中文介面驗收，。！？0123456789";
    const auto titleResult =
        ui.text(ui.context, s.font, 22, {16, 12, 608, 36}, title, sizeof(title) - 1, 0xffffffff);
    const auto statusResult = ui.text(ui.context, s.font, 18, {16, 52, 608, 30}, status,
                                      static_cast<uint32_t>(std::strlen(status)), 0xffffffff);
    if (titleResult || statusResult) {
      std::fprintf(stderr, "host fixture labels failed %d %d\n", titleResult, statusResult);
      return -3;
    }
    if (Button(s, 1, 16, 100, "儲存 SAVE")) {
      if (Save(s))
        return -3;
      ++s.saves;
    }
    if (Button(s, 2, 216, 100, "讀取 OPEN")) {
      if (Load(s))
        return -3;
      ++s.opens;
    }
    if (Button(s, 3, 416, 100, "重綁 D / F")) {
      s.rebound = 1 - s.rebound;
      if (BindMovement(s))
        return -3;
    }
    for (unsigned i = 0; i < 3; ++i) {
      static constexpr const char *names[]{"主音量", "音樂音量", "音效音量"};
      static constexpr float gains[]{1, .25F, 0};
      char label[80]{};
      std::snprintf(label, sizeof(label), "%s %u%%", names[i],
                    static_cast<unsigned>(gains[s.gains[i]] * 100));
      if (Button(s, 4 + i, 16 + 200.F * float(i), 150, label)) {
        s.gains[i] = (s.gains[i] + 1) % 3;
        if (s.audio.set_bus_gain(s.audio.context, i, gains[s.gains[i]]))
          return -3;
        ++s.busChanges;
      }
    }
    if (Button(s, 7, 16, 200, "播放音樂")) {
      if (s.music)
        (void)s.audio.stop(s.audio.context, s.music);
      const NexoraAudioPlayDesc desc{2, NEXORA_AUDIO_BUS_MUSIC, 1, 1};
      const auto result = s.audio.play(s.audio.context, &desc, &s.music);
      if (result && result != -2)
        return -3;
      ++s.plays;
      s.paused = false;
    }
    if (Button(s, 8, 216, 200, "暫停繼續") && s.music) {
      s.paused = !s.paused;
      if (s.audio.set_paused(s.audio.context, s.music, s.paused))
        return -3;
      ++s.pauses;
    }
    if (Button(s, 9, 416, 200, "停止音樂") && s.music) {
      if (s.audio.stop(s.audio.context, s.music))
        return -3;
      s.music = 0;
      ++s.stops;
    }
    if (Button(s, 10, 16, 250, "播放音效")) {
      if (s.sfx)
        (void)s.audio.stop(s.audio.context, s.sfx);
      const NexoraAudioPlayDesc desc{1, NEXORA_AUDIO_BUS_SFX, 1, 1};
      const auto result = s.audio.play(s.audio.context, &desc, &s.sfx);
      if (result && result != -2)
        return -3;
      ++s.plays;
    }
    if (Button(s, 11, 216, 250, "停止音效") && s.sfx) {
      if (s.audio.stop(s.audio.context, s.sfx))
        return -3;
      s.sfx = 0;
      ++s.stops;
    }
    char pad[240]{};
    std::snprintf(pad, sizeof(pad), "PAD %08X LX %.2f LY %.2f LT %.2f RX %.2f RY %.2f RT %.2f",
                  s.buttonsSeen, double(s.axes[0]), double(s.axes[1]), double(s.axes[2]),
                  double(s.axes[3]), double(s.axes[4]), double(s.axes[5]));
    if (ui.text(ui.context, s.font, 14, {16, 310, 608, 28}, pad,
                static_cast<uint32_t>(std::strlen(pad)), 0xffffffff))
      return -3;
    static constexpr char help[] = "D-PAD: MENU  CROSS: OK  CIRCLE: CANCEL";
    if (ui.text(ui.context, s.font, 16, {16, 348, 608, 28}, help, sizeof(help) - 1, 0xffffffff))
      return -3;
    if (ui.end_frame(ui.context))
      return -3;
    return 0;
  } catch (...) {
    return -3;
  }
}
void Stop(void *opaque) {
  auto &s = *static_cast<State *>(opaque);
  const bool saved = Save(s) == 0;
  (void)s.audio.stop_all(s.audio.context);
  std::fprintf(stderr,
               "host_fixture {\"position\":%.6f,\"rebound\":%u,\"saves\":%u,\"opens\":%u,"
               "\"keyboard_movement_frames\":%u,\"gamepad_movement_frames\":%u,"
               "\"buttons_seen\":%u,\"bus_changes\":%u,\"plays\":%u,\"pauses\":%u,"
               "\"stops\":%u,\"cancels\":%u,\"final_save_ok\":%s,\"axes_min\":[",
               s.position, s.rebound, s.saves, s.opens, s.keyboardMovement, s.gamepadMovement,
               s.buttonsSeen, s.busChanges, s.plays, s.pauses, s.stops, s.cancels,
               saved ? "true" : "false");
  for (unsigned i = 0; i < 6; ++i)
    std::fprintf(stderr, "%s%.6f", i ? "," : "", double(s.axesMin[i]));
  std::fprintf(stderr, "],\"axes_max\":[");
  for (unsigned i = 0; i < 6; ++i)
    std::fprintf(stderr, "%s%.6f", i ? "," : "", double(s.axesMax[i]));
  std::fprintf(stderr, "]}\n");
}
void Destroy(void *opaque) {
  auto *s = static_cast<State *>(opaque);
  const auto host = s->host;
  s->~State();
  host.deallocate(host.context, 1, s, sizeof(State), alignof(State));
}
} // namespace
#if defined(_WIN32)
#define HOST_EXPORT __declspec(dllexport)
#else
#define HOST_EXPORT __attribute__((visibility("default")))
#endif
extern "C" HOST_EXPORT int32_t NexoraGameModuleLoad(uint32_t abi, NexoraGameModuleV3 *module) {
  if (abi != NEXORA_GAMEPLAY_ABI_VERSION || !module || module->struct_size < sizeof(*module))
    return -2;
  *module = {sizeof(*module), abi,    0,    nullptr, Create,  Start,
             nullptr,         Update, Stop, Destroy, nullptr, nullptr};
  return 0;
}
