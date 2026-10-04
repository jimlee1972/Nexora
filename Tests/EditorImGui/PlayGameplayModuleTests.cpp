#include "Nexora/Runtime/EditorSdk.h"
#include "PlayGameplayModule.h"
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
                host.capture_input(host.context, 1, &snapshot) ==
                    NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
            "gameplay input callback did not return the owned user-zero snapshot");
    module.SetInputFocus(false);
    Require(host.capture_input(host.context, 0, &snapshot) == NEXORA_GAMEPLAY_OK &&
                snapshot.move_x == 0,
            "input callback retained a pressed key after focus loss");
    fail_fixed = true;
    Require(!play.Step() && play.LastPauseReason() == runtime::PauseReason::RuntimeFailure &&
                play.Stats().crashes == 1,
            "fixed failure did not enter contained Play recovery");
    fail_update = true;
    Require(!module.Update(0.1), "update failure was silently accepted");
    module.Unload();
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
