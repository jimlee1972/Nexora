#include "EditorImGuiTestAccess.h"
#include "PlayHostMemoryPresenter.h"
#include "TemporaryDirectoryCleanup.h"
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using namespace editor;
using Access = imgui::EditorImGuiTestAccess;
NexoraGameplayHostV3 api{};
bool retain{}, fail_create{}, fail_start{};
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
int32_t Create(void **state, const NexoraGameplayHostV3 *host) {
  api = *host;
  *state = api.allocate(api.context, 7, 64, 16);
  return *state && !fail_create ? NEXORA_GAMEPLAY_OK : NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
}
int32_t Start(void *) { return fail_start ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : NEXORA_GAMEPLAY_OK; }
int32_t Update(void *, double) { return NEXORA_GAMEPLAY_OK; }
void Stop(void *) {}
void Destroy(void *state) {
  if (!retain)
    api.deallocate(api.context, 7, state, 64, 16);
}
int32_t Loader(std::uint32_t version, NexoraGameModuleV3 *module) {
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = version;
  module->create = Create;
  module->on_start = Start;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}
void Check(float dpi) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-play-memory-ui-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  ProjectWorkspace first, second, observer, duplicate;
  Require(first.Create(root / "first", "First") && second.Create(root / "second", "Second") &&
              observer.Open(first.Root(), ProjectAccess::ReadOnly),
          "project fixture failed");
  std::filesystem::create_directories(root / "duplicate" / "Content");
  std::filesystem::copy_file(first.Root() / "project.nexora",
                             root / "duplicate" / "project.nexora");
  Require(duplicate.Open(root / "duplicate", ProjectAccess::ReadOnly) &&
              duplicate.Project().id == first.Project().id,
          "same-UUID different-root fixture failed");
  ProjectWorkspace *active = &first;
  ProjectContentSession content;
  Require(content.Browser().Reset({}, 7), "content generation fixture failed");
  runtime::World source;
  const auto scene = source.LoadScene("Memory inspection");
  Require(source.Activate(scene), "scene activation failed");
  SceneDocument document(source, scene);
  document.Create("Source");
  const auto baseline = source.SaveScene(scene);
  const auto dirty = document.Dirty();
  runtime::PlaySession play(source);
  preview::PlayGameplayModule module;
  preview::PlayHostMemoryPresenter presenter;
  ProductShell shell;
  imgui::EditorImGuiHost ui;
  ui.SetDisplay(1280 * dpi, 900 * dpi, dpi);
  Access::ConfigureSyntheticInput(ui);
  bool publish = true;
  const auto draw = [&] {
    if (publish)
      Require(ui.SetPlayHostMemory(
                  presenter.Observe(module, active, content.Browser().ProjectGeneration(), play)),
              "application publication failed");
    ui.BeginFrame();
    ui.DrawProductShell(shell, &document, active, &content, nullptr, nullptr, nullptr, &play);
    Require(ui.EndFrame().vertices > 0, "UI did not draw");
  };
  for (int i = 0; i < 4; ++i)
    draw();
  const auto open = [&] {
    if (Access::PlayMemoryOpen(ui))
      return;
    Access::FocusGame(ui);
    draw();
    draw();
    const auto point = Access::PlayMemoryButton(ui);
    Require(point.has_value(), "memory button absent");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * dpi);
    pointer.value1 = static_cast<int>((*point)[1] * dpi);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    ui.ProcessEvents(std::array{pointer});
    draw();
    button.value1 = 1;
    ui.ProcessEvents(std::array{button});
    draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    draw();
    draw();
    Require(Access::PlayMemoryOpen(ui) && !Access::PlayMemoryText(ui).empty() &&
                ui.TakePlayCommand() == imgui::PlayCommand::None,
            "real memory button failed or emitted Play command");
  };
  const auto contains = [&](std::string_view text) {
    Require(Access::PlayMemoryText(ui).find(text) != std::string_view::npos,
            ("display text missing: " + std::string(text)).c_str());
  };
  open();
  contains("Current: no loaded gameplay module.");
  contains("Last teardown: unavailable for this project.");
  retain = fail_create = fail_start = false;
  Require(play.Start(.25, [](runtime::World &, double) { return true; }) &&
              module.Load(*play.PlayWorld(), Loader),
          "initial module load failed");
  draw();
  const auto copied = Access::PlayMemory(ui);
  auto *extra = api.allocate(api.context, 8, 96, 16);
  Require(extra, "extra allocation failed");
  api.deallocate(api.context, 9, extra, 96, 16);
  api.deallocate(api.context, 8, extra, 96, 16);
  Require(!api.allocate(api.context, 7, 16, 3) && play.Pause(), "rejection/Pause failed");
  draw();
  contains("Paused");
  contains("Live: 1 allocations | 64 payload bytes");
  contains("Peak: 2 allocations | 160 payload bytes");
  contains("Rejected: 1 allocations | 1 deallocations");
  Require(copied && copied->current && copied->current->peak_bytes == 64,
          "UI observation borrowed mutable provider state");
  publish = false;
  auto value = *Access::PlayMemory(ui);
  value.current->rejected_allocations = UINT64_MAX;
  Require(ui.SetPlayHostMemory(value), "full-width counters rejected");
  draw();
  contains("Rejected: 18446744073709551615 allocations");
  auto invalid = value;
  invalid.current->live_bytes = invalid.current->peak_bytes + 1;
  Require(!ui.SetPlayHostMemory(invalid) &&
              Access::PlayMemory(ui)->current->rejected_allocations == UINT64_MAX,
          "invalid observation replaced previous owning data");
  for (int field = 0; field < 4; ++field) {
    auto stale = value;
    if (field == 0)
      stale.project = second.Project().id;
    if (field == 1)
      stale.root = second.Root();
    if (field == 2)
      ++stale.project_generation;
    if (field == 3)
      ++stale.play_generation;
    Require(ui.SetPlayHostMemory(stale), "stale fixture malformed");
    draw();
    Require(!Access::PlayMemory(ui), "foreign scope reached widgets");
    contains("Observation unavailable");
  }
  publish = true;
  module.Unload();
  Require(play.Stop(), "initial Stop failed");
  draw();
  contains("Reclaimed: 0 allocations | 0 payload bytes");
  const auto serial = module.AllocationTeardownSequence();
  module.Unload();
  Require(module.AllocationTeardownSequence() == serial, "repeated Unload invented teardown");
  for (unsigned cycle = 0; cycle < 32; ++cycle) {
    fail_create = cycle % 4 == 2;
    fail_start = cycle % 4 == 3;
    retain = cycle % 4 != 0;
    Require(play.Start(.25, [](runtime::World &, double) { return true; }), "repeat Play failed");
    const bool loaded = module.Load(*play.PlayWorld(), Loader);
    Require(loaded == (!fail_create && !fail_start), "failed-load result changed");
    draw();
    if (loaded)
      contains("Live: 1 allocations | 64 payload bytes");
    module.Unload();
    Require(play.Stop(), "repeat Stop failed");
    draw();
    contains("Stopped");
    contains(retain ? "Reclaimed: 1 allocations | 64 payload bytes"
                    : "Reclaimed: 0 allocations | 0 payload bytes");
    Require(source.SaveScene(scene) == baseline && document.Dirty() == dirty,
            "inspection modified source/history");
  }
  active = &observer;
  draw();
  Require(Access::PlayMemory(ui)->last_teardown.has_value(), "read-only alias lost same scope");
  active = &duplicate;
  draw();
  Require(!Access::PlayMemory(ui)->last_teardown, "same UUID relabelled foreign-root teardown");
  open();
  Access::FocusGame(ui);
  draw();
  draw();
  const auto held_point = Access::PlayMemoryButton(ui);
  Require(held_point.has_value(), "held memory control absent");
  Nexora::Window::WindowEvent pointer, button;
  pointer.type = Nexora::Window::WindowEventType::Pointer;
  pointer.value0 = static_cast<int>((*held_point)[0] * dpi);
  pointer.value1 = static_cast<int>((*held_point)[1] * dpi);
  button.type = Nexora::Window::WindowEventType::PointerButton;
  button.value0 = 0;
  button.value1 = 1;
  ui.ProcessEvents(std::array{pointer});
  draw();
  ui.ProcessEvents(std::array{button});
  draw();
  active = &second;
  draw();
  button.value1 = 0;
  ui.ProcessEvents(std::array{button});
  draw();
  Require(!Access::PlayMemoryOpen(ui) && !Access::PlayMemory(ui)->last_teardown,
          "project switch exposed foreign teardown or retained window");
  open();
  contains("Last teardown: unavailable for this project.");
  Require(content.Browser().Reset({}, 8), "generation replacement failed");
  draw();
  Require(!Access::PlayMemoryOpen(ui) && !Access::PlayMemory(ui)->last_teardown,
          "generation replacement retained inspection");
  active = nullptr;
  draw();
  Require(!Access::PlayMemory(ui), "project detach retained data");
  active = &first;
  draw();
  Require(!Access::PlayMemory(ui)->last_teardown, "reattach relabelled old provider teardown");
  // The owner binds a freshly activated scope before accepting its first Play command.
  fail_create = true;
  Require(play.Start(.25, [](runtime::World &, double) { return true; }) &&
              !module.Load(*play.PlayWorld(), Loader) && play.Stop(),
          "first-load failure fixture failed");
  draw();
  Require(Access::PlayMemory(ui)->last_teardown &&
              Access::PlayMemory(ui)->last_teardown->reclaimed_bytes == 64,
          "first failure after activation was hidden as a prior-scope teardown");
  Require(ui.SetPlayHostMemory({}), "explicit clear failed");
}
} // namespace
int main() {
  try {
    Check(1);
    Check(2);
    std::cout
        << "Play host memory UI: real 1x/2x controls, exact uint64 text, 32-cycle provider "
           "publication, failed loads, Pause/Stop and project/root/generation isolation passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
