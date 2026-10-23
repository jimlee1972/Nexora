#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/TwoD/GameplayDisplay.h"
#include "Nexora/TwoD/GameplayUi.h"
#include <cassert>
#include <cmath>
#include <new>
#include <string>
extern "C" int32_t NexoraGameModuleLoad(uint32_t, NexoraGameModuleV3 *);
using namespace nexora;
struct Host {
  runtime::GameplayServices services;
  std::string log;
  unsigned allocated{}, freed{};
};
int main() {
  for (const auto dpi : {1.F, 1.25F, 1.5F, 1.75F}) {
    twod::GameplayUi ui;
    twod::GameplayDisplay display;
    Host context;
    context.services.ui.struct_size = sizeof(context.services.ui);
    assert(ui.GetApi(&context.services.ui) == 0);
    context.services.ui_draw.struct_size = sizeof(context.services.ui_draw);
    context.services.ui_draw.abi_version = 1;
    assert(ui.GetDrawApi(&context.services.ui_draw) == 0);
    context.services.display = display.Api();
    NexoraGameplayHostV3 host{};
    host.struct_size = sizeof(host);
    host.abi_version = 3;
    host.context = &context;
    host.capabilities = NEXORA_GAMEPLAY_CAPABILITY_SERVICES_API;
    host.get_services_api = [](void *p, NexoraGameplayServicesApiV1 *out) {
      return static_cast<Host *>(p)->services.GetApi(out);
    };
    host.allocate = [](void *p, uint64_t, uint64_t size, uint64_t align) -> void * {
      ++static_cast<Host *>(p)->allocated;
      return ::operator new(size, std::align_val_t(align));
    };
    host.deallocate = [](void *p, uint64_t, void *allocation, uint64_t, uint64_t align) {
      ++static_cast<Host *>(p)->freed;
      ::operator delete(allocation, std::align_val_t(align));
    };
    host.log = [](void *p, uint32_t, const char *text, uint32_t length) {
      static_cast<Host *>(p)->log.append(text, length).push_back('\n');
    };
    NexoraGameModuleV3 module{};
    module.struct_size = sizeof(module);
    assert(NexoraGameModuleLoad(3, &module) == 0);
    void *state{};
    assert(module.create(&state, &host) == 0);
    assert(module.on_start(state) == 0);
    auto step = [&](twod::GameplayUiInput input) {
      display.BeginFrame(1920, 1080, dpi, 0);
      ui.BeginInput(input);
      assert(module.update(state, 1.0 / 60) == 0);
      auto frame = ui.Frame();
      assert(!frame.commands.empty() && !frame.image_owners.empty());
      frame.ToPixels(dpi, 1920, 1080);
      for (const auto &vertex : frame.vertices)
        assert(std::isfinite(vertex.position[0]));
      return frame;
    };
    auto frame = step({});
    assert(frame.commands.size() > 20 && context.log.find("陰陽 生存") != std::string::npos);
    // Pointer coordinates are logical (the real Player divides native pixels by DPI once).
    const float unit = 2 / dpi;
    twod::GameplayUiInput pointer;
    pointer.pointer_x = 1920 / dpi - 80 * unit;
    pointer.pointer_y = 1080 / dpi - 25 * unit;
    pointer.pointer_pressed = true;
    step(pointer);
    pointer.pointer_pressed = false;
    pointer.pointer_released = true;
    step(pointer);
    step({});
    assert(context.log.find("選擇") != std::string::npos);
    // Keyboard/gamepad activation follows the same UI navigation contract.
    twod::GameplayUiInput keyboard;
    keyboard.activate = true;
    step(keyboard);
    step({});
    assert(context.log.find("整備") != std::string::npos);
    step(keyboard);
    step({});
    assert(context.log.find("字幕") != std::string::npos);
    step(keyboard);
    step({});
    assert(context.log.find("設定") != std::string::npos);
    twod::GameplayUiInput navigate;
    navigate.navigate = 1;
    navigate.activate = true;
    step(navigate);
    assert(context.log.find("SP-02 UI scale toggled") != std::string::npos);
    step({}); // 150% UI uses the same world-independent display snapshot
    twod::GameplayUiInput cancel;
    cancel.cancel = true;
    step(cancel);
    step({});
    assert(context.log.rfind("陰陽 生存") > context.log.find("設定"));
    // All own retained frames must expire before native IDs can be released.
    module.on_stop(state);
    ui.BeginInput({});
    frame = {};
    assert(ui.CollectRetiredTextures().size() == 2);
    module.destroy(state);
    assert(context.allocated == context.freed);
    // The same session can attach a fresh module without leaking font slots or aliasing image IDs.
    state = nullptr;
    assert(module.create(&state, &host) == 0 && module.on_start(state) == 0);
    frame = step({});
    module.on_stop(state);
    ui.BeginInput({});
    frame = {};
    assert(ui.CollectRetiredTextures().size() == 2);
    module.destroy(state);
    assert(context.allocated == context.freed);
    // Failed Start must clean an already registered font even when Stop is never called.
    context.services.ui_draw.register_image = [](void *, const NexoraUiImageV1 *, uint64_t *) {
      return -2;
    };
    state = nullptr;
    assert(module.create(&state, &host) == 0 && module.on_start(state) == -2);
    std::uint64_t missingFont = 99;
    assert(context.services.ui.get_font(context.services.ui.context, 0, &missingFont) == -2 &&
           missingFont == 99);
    assert(ui.CollectRetiredTextures().size() == 1);
    module.destroy(state);
    assert(context.allocated == context.freed);
  }
}
