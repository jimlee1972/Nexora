#include "ShowcaseProbes.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

using namespace nexora::showcase;

int main() {
  auto registry = ProbeRegistry::CreateV1Registry();
  const auto results = registry.RunAll();
  assert(results.size() == 13);
  assert(registry.Find("v1.M0") != nullptr);
  assert(registry.Find("v1.M12") != nullptr);
  assert(registry.Find("missing") == nullptr);

  bool duplicateRejected = false;
  try {
    registry.Register({"v1.M0", "M0", "duplicate", "duplicate", [] { return ProbeResult{}; }});
  } catch (const std::invalid_argument &) {
    duplicateRejected = true;
  }
  assert(duplicateRejected);

  constexpr std::array injections{ErrorInjection::InvalidAsset, ErrorInjection::DependencyCycle,
                                  ErrorInjection::PluginAbiMismatch, ErrorInjection::Rollback};
  for (const auto injection : injections) {
    const auto injected = registry.RunAll(injection);
    bool observed = false;
    for (const auto &result : injected)
      observed = observed || !result.issues.empty();
    assert(observed);
  }

  const auto rooms = BuildRoomStates({true, true, true, true});
  assert(rooms.size() == 4);
  for (const auto &room : rooms) {
    assert(room.status == ProbeStatus::NotRun);
    assert(!room.visual_complete);
  }
  const auto json = SerializeJson(results, rooms);
  const auto markdown = SerializeMarkdown(results, rooms);
  assert(json.find("nexora.showcase.validation.v1") != std::string::npos);
  assert(json.find("\"visual_complete\": false") != std::string::npos);
  assert(markdown.find("| `v1.M12` | M12 | NOT_RUN |") != std::string::npos);

  auto view = BuildValidationLab(results);
  assert(view.cards.size() == 13);
  assert(view.failure_states.size() == 4);
  assert(view.status_legend[4] == ProbeStatus::Blocked);
  assert(view.cards[5].room_id == "scene");
  assert(view.cards[5].world_object != 0);
  assert(view.cards[5].contract_test == "runtime.v1_m5_asset_pipeline");
  const auto presentation = SerializeValidationLabView(view);
  assert(presentation.find("\"room\": \"scene\"") != std::string::npos);
  assert(presentation.find("\"code\": \"rollback\"") != std::string::npos);
  assert(presentation.find("\"BLOCKED\"") != std::string::npos);

  std::vector<std::byte> first(320U * 180U * 4U);
  std::vector<std::byte> second(first.size());
  DrawValidationLab(first, 320, 180, view);
  DrawValidationLab(second, 320, 180, view);
  assert(first == second);
  assert(first.front() == std::byte{});

  bool invalidTargetRejected = false;
  try {
    DrawValidationLab(std::span<std::byte>{}, 1, 1, view);
  } catch (const std::invalid_argument &) {
    invalidTargetRejected = true;
  }
  assert(invalidTargetRejected);

  auto lifetimeCopy = view;
  view = {};
  DrawValidationLab(first, 320, 180, lifetimeCopy);
  lifetimeCopy = {};
}
