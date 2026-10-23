#include "Nexora/Runtime/GameplayModuleHost.h"
#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/TwoD/GameplayBridge.h"
#include "Nexora/TwoD/GameplayDisplay.h"
#include "Nexora/TwoD/GameplayUi.h"
#include <cassert>
#include <iostream>
#include <new>
using namespace nexora;
struct Host {
  runtime::GameplayServices services;
  twod::GameplayBridge &bridge;
  runtime::Id scene;
  unsigned allocated{}, freed{};
};
int main(int argc, char **argv) {
  assert(argc == 3);
  for (int index = 1; index < argc; ++index) {
    runtime::World world(runtime::WorldKind::Play);
    const auto sceneId = world.LoadScene("Main");
    twod::SpriteScene scene(world, sceneId);
    twod::InstancePool pool(16);
    twod::GameplayBridge bridge(world, scene, {}, nullptr, &pool);
    twod::GameplayUi ui;
    twod::GameplayDisplay display;
    Host context{{}, bridge, sceneId};
    auto &services = context.services;
    services.display = display.Api();
    services.ui.struct_size = sizeof(services.ui);
    services.ui.abi_version = 1;
    services.ui_draw.struct_size = sizeof(services.ui_draw);
    services.ui_draw.abi_version = 1;
    assert(ui.GetApi(&services.ui) == 0 && ui.GetDrawApi(&services.ui_draw) == 0);
    NexoraGameplayHostV3 host{};
    host.struct_size = sizeof(host);
    host.abi_version = 3;
    host.context = &context;
    host.capabilities = NEXORA_GAMEPLAY_CAPABILITY_SERVICES_API | NEXORA_GAMEPLAY_CAPABILITY_2D_API;
    host.get_services_api = [](void *p, NexoraGameplayServicesApiV1 *out) {
      return static_cast<Host *>(p)->services.GetApi(out);
    };
    host.get_twod_api = [](void *p, NexoraTwoDApiV1 *out) {
      return static_cast<Host *>(p)->bridge.GetApi(out);
    };
    host.allocate = [](void *p, uint64_t, uint64_t n, uint64_t a) -> void * {
      ++static_cast<Host *>(p)->allocated;
      return ::operator new(n, std::align_val_t(a));
    };
    host.deallocate = [](void *p, uint64_t, void *v, uint64_t, uint64_t a) {
      ++static_cast<Host *>(p)->freed;
      ::operator delete(v, std::align_val_t(a));
    };
    host.load_scene = [](void *p, const char *, uint32_t, uint32_t, uint64_t *id) -> int32_t {
      *id = static_cast<Host *>(p)->scene;
      return 0;
    };
    host.activate_scene = [](void *, uint64_t) -> int32_t { return 0; };
    display.BeginFrame(1920, 1080, 1.5F, 0);
    runtime::GameplayModuleHost module(host);
    assert(module.Load(std::filesystem::path(argv[index])));
    for (unsigned round = 0; round < 3; ++round) {
      // Actual module callbacks see zero dimensions repeatedly, then recover to a valid frame.
      display.BeginFrame(0, 0, 1.5F, 0);
      ui.BeginInput({});
      for (unsigned n = 0; n < 4; ++n)
        assert(module.Update(1.0 / 60));
      assert(module.IsLoaded());
      display.BeginFrame(1920, 1080, 1.5F, 0);
      ui.BeginInput({});
      assert(module.Update(1.0 / 60));
      assert(!ui.Frame().commands.empty());
    }
    module.Unload();
    assert(context.allocated == context.freed && pool.Size() == 0);
  }
  std::cout << "Actual SP-01/SP-04 Zig modules survive repeated zero-extent updates and restore UI "
               "without callback failure\n";
}
