#include "Nexora/Runtime/EditorSdk.h"
#include "PlayGameplayModule.h"
#include "PlayInputForwarding.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
NexoraGameplayHostV3 host;
std::uint64_t target;
bool fail_fixed{}, fail_update{}, destroyed_with_world{}, optional_fixed{};
int32_t Create(void **state, const NexoraGameplayHostV3 *api) {
  host = *api;
  *state = host.allocate(host.context, 7, 64, 16);
  return *state ? NEXORA_GAMEPLAY_OK : NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
}
int32_t Start(void *) {
  const char message[] = "Play gameplay started";
  host.log(host.context, 1, message, sizeof(message) - 1);
  return NEXORA_GAMEPLAY_OK;
}
int32_t Fixed(void *, double seconds) {
  if (fail_fixed)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  game::GameplayTransformWire wire;
  if (host.read_component(host.context, target, game::TransformComponentType(), &wire,
                          sizeof(wire)) != NEXORA_GAMEPLAY_OK)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  wire.x += seconds;
  return host.write_component(host.context, target, game::TransformComponentType(), &wire,
                              sizeof(wire));
}
int32_t Update(void *, double) {
  return fail_update ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : NEXORA_GAMEPLAY_OK;
}
void Stop(void *) {}
void Destroy(void *state) {
  game::GameplayTransformWire wire;
  destroyed_with_world = host.read_component(host.context, target, game::TransformComponentType(),
                                             &wire, sizeof(wire)) == NEXORA_GAMEPLAY_OK;
  host.deallocate(host.context, 7, state, 64, 16);
}
int32_t Load(uint32_t requested, NexoraGameModuleV3 *module) {
  if (requested != NEXORA_GAMEPLAY_ABI_VERSION)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = requested;
  module->capabilities = optional_fixed ? 0 : NEXORA_GAMEPLAY_CAPABILITY_FIXED_UPDATE;
  module->create = Create;
  module->on_start = Start;
  module->fixed_update = optional_fixed ? nullptr : Fixed;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}
} // namespace
int main() {
  try {
    runtime::World world;
    const auto scene = world.LoadScene("Play");
    Require(world.Activate(scene), "activate failed");
    target = world.CreateEntity(scene).id;
    runtime::PlaySession play(world);
    std::string logged;
    editor::preview::PlayGameplayModule module(
        [&](uint32_t, std::string text) { logged = std::move(text); });
    Require(play.Start(0.25, [&](runtime::World &,
                                 double seconds) { return module.FixedUpdate(seconds); }),
            "start failed");
    Require(module.Load(*play.PlayWorld(), Load) && logged == "Play gameplay started",
            "module create/start/log failed");
    Require(play.Tick() && play.Pause() && play.Step() &&
                play.PlayWorld()->FindEntity(target)->transform.x == 0.5 &&
                world.FindEntity(target)->transform.x == 0,
            "fixed callbacks did not mutate only the Play clone");
    Require(!host.allocate(host.context, 1, 32, 3) && !host.allocate(host.context, 1, 16777217, 16),
            "invalid allocation admitted");
    // Wrong-owner frees are ignored, and the correct free remains safe.
    void *extra = host.allocate(host.context, 8, 32, 16);
    Require(extra, "allocation failed");
    host.deallocate(host.context, 9, extra, 32, 16);
    host.deallocate(host.context, 8, extra, 32, 16);
    module.SetInputFocus(true);
    module.ProcessInput({});
    Nexora::Window::WindowEvent right;
    right.type = Nexora::Window::WindowEventType::Key;
    right.value0 = static_cast<int>(Nexora::Window::Key::D);
    right.value1 = 1;
    module.ProcessInput(std::array{right});
    NexoraInputSnapshot snapshot{};
    Require(host.capture_input &&
                host.capture_input(host.context, 0, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.move_x == 1 &&
                host.capture_input(host.context, 4, &snapshot) ==
                    NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
            "gameplay input callback did not return the owned user-zero snapshot");
    module.SetInputFocus(false);
    Require(host.capture_input(host.context, 0, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.move_x == 0,
            "input callback retained a pressed key after focus loss");
    Require(play.Resume(), "deferred input fixture resume failed");
    editor::preview::ForwardPlayInput(play, module, true, {});
    editor::preview::ForwardPlayInput(play, module, true, std::array{right});
    Require(host.capture_input(host.context, 0, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.move_x == 1,
            "native input forwarding did not capture the movement key");
    right.value1 = 0;
    editor::preview::ForwardPlayInput(play, module, true, std::array{right});
    Require(host.capture_input(host.context, 0, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.move_x == 0,
            "keyup without a rendered GUI frame retained movement");
    right.value1 = 1;
    editor::preview::ForwardPlayInput(play, module, true, std::array{right});
    editor::preview::ForwardPlayInput(play, module, false, {});
    Require(!play.AcceptsInput() &&
                host.capture_input(host.context, 0, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.move_x == 0 && play.Pause(),
            "deferred focus loss retained a held key");
    editor::preview::ForwardPlayInput(play, module, true, std::array{right});
    Require(!play.AcceptsInput() &&
                host.capture_input(host.context, 0, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.move_x == 0,
            "paused forwarding admitted gameplay movement");
    editor::PlayInputBindings rebound;
    rebound.controls[1][0] = editor::PlayInputControl::B;
    Require(play.Resume(), "rebound callback fixture resume failed");
    editor::preview::ForwardPlayInput(play, module, true, {}, rebound);
    auto custom = right;
    custom.value0 = static_cast<int>(Nexora::Window::Key::B);
    editor::preview::ForwardPlayInput(play, module, true, std::array{custom}, rebound);
    Require(host.capture_input(host.context, 0, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.move_x == 1 && snapshot.reserved == 0,
            "custom binding did not reach gameplay callback without a rendered frame");
    const auto user_zero_copy = snapshot;
    NexoraInputSnapshot neutral{};
    Require(host.capture_input(host.context, 3, &neutral) == NEXORA_GAMEPLAY_OK &&
                neutral.sequence == snapshot.sequence && neutral.move_x == 0 &&
                neutral.move_y == 0 && neutral.buttons == 0 && neutral.reserved == 0,
            "Unselected logical user borrowed held state");
    Require(!module.SetInputUser(4) &&
                host.capture_input(host.context, 0, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.move_x == 1,
            "Invalid user changed selected input or callback output");
    editor::preview::ForwardPlayInput(play, module, true, std::array{custom}, rebound, 3);
    Require(host.capture_input(host.context, 3, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.move_x == 0,
            "User switch replayed the acquisition batch");
    editor::preview::ForwardPlayInput(play, module, true, std::array{custom}, rebound, 3);
    Require(host.capture_input(host.context, 3, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.move_x == 1 && snapshot.reserved == 0 && user_zero_copy.move_x == 1,
            "Selected user callback or owning previous snapshot failed");
    for (std::uint32_t user = 0; user < 3; ++user)
      Require(host.capture_input(host.context, user, &neutral) == NEXORA_GAMEPLAY_OK &&
                  neutral.sequence == snapshot.sequence && neutral.move_x == 0 &&
                  neutral.buttons == 0,
              "Selected user input leaked to another callback lane");
    const auto preserved = snapshot;
    Require(
        host.capture_input(host.context, 4, &snapshot) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
            snapshot.sequence == preserved.sequence && snapshot.move_x == preserved.move_x &&
            host.capture_input(host.context, 0, nullptr) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
        "Invalid capture mutated output or admitted a null pointer");
    editor::preview::ForwardPlayInput(play, module, true, std::array{custom}, rebound, 4);
    Require(!play.AcceptsInput() &&
                host.capture_input(host.context, 3, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.move_x == 0,
            "Invalid forwarding user retained focus or held controls");
    editor::preview::ForwardPlayInput(play, module, true, {}, rebound);
    editor::preview::ForwardPlayInput(play, module, true, std::array{custom}, rebound);
    Require(host.capture_input(host.context, 0, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.move_x == 1,
            "Default user-zero compatibility did not reacquire after switch");
    const auto captured = snapshot;
    Require(host.capture_input(host.context, 0, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.sequence == captured.sequence && snapshot.move_x == captured.move_x,
            "repeated capture changed copied rebound frame");
    custom.value1 = 0;
    editor::preview::ForwardPlayInput(play, module, true, std::array{custom}, rebound);
    Require(host.capture_input(host.context, 0, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.move_x == 0,
            "rebound keyup without rendering retained movement");
    editor::preview::ForwardPlayInput(play, module, false, {}, rebound);
    Require(play.Pause(), "rebound callback fixture pause failed");
    fail_fixed = true;
    Require(!play.Step() && play.LastPauseReason() == runtime::PauseReason::RuntimeFailure &&
                play.Stats().crashes == 1,
            "fixed failure did not enter contained Play recovery");
    fail_update = true;
    Require(!module.Update(0.1), "update failure was silently accepted");
    Require(module.SetInputUser(3) && module.InputUser() == 3, "Unload user fixture failed");
    module.Unload();
    Require(module.InputUser() == 0, "Module unload retained selected input identity");
    Require(destroyed_with_world && !module.IsLoaded() && play.Stop(), "module outlived clone");
    fail_fixed = fail_update = false;
    optional_fixed = true;
    Require(play.Start(0.25, [&](runtime::World &,
                                 double seconds) { return module.FixedUpdate(seconds); }) &&
                module.Load(*play.PlayWorld(), Load) && play.Tick() && module.Update(0.1),
            "optional fixed callback prevented Play");
    module.Unload();
    Require(play.Stop(), "second stop failed");
    std::string error;
    Require(!module.LoadRelative(world, ".", "../outside.so", error) && !error.empty() &&
                !module.LoadRelative(world, ".", "/outside.so", error),
            "outside-project library admitted");
    std::cout << "Play gameplay module isolation/lifecycle contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
