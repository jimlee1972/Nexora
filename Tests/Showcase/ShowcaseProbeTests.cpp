#include "ShowcaseProbes.h"

#include <array>
#include <cassert>
#include <stdexcept>
#include <string>

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
    assert(room.status == ProbeStatus::ContractOnly);
    assert(!room.visual_complete);
  }
  const auto json = SerializeJson(results, rooms);
  const auto markdown = SerializeMarkdown(results, rooms);
  assert(json.find("nexora.showcase.validation.v1") != std::string::npos);
  assert(json.find("\"visual_complete\": false") != std::string::npos);
  assert(markdown.find("| `v1.M12` | M12 | CONTRACT_ONLY |") != std::string::npos);
}
