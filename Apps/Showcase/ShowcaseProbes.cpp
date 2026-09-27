#include "ShowcaseProbes.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nexora::showcase {
namespace {

std::string Escape(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char character : value) {
    switch (character) {
    case '\\':
      escaped += "\\\\";
      break;
    case '"':
      escaped += "\\\"";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      escaped += character;
      break;
    }
  }
  return escaped;
}

ProbeResult Inject(ProbeResult result, ErrorInjection injection) {
  struct Injection final {
    ErrorInjection kind;
    std::string_view milestone;
    std::string_view code;
    std::string_view message;
  };
  constexpr std::array injections{
      Injection{ErrorInjection::InvalidAsset, "M5", "invalid_asset",
                "invalid asset rejected before publication"},
      Injection{ErrorInjection::DependencyCycle, "M0", "dependency_cycle",
                "module dependency cycle rejected"},
      Injection{ErrorInjection::PluginAbiMismatch, "M6", "plugin_abi_mismatch",
                "incompatible plugin ABI rejected"},
      Injection{ErrorInjection::Rollback, "M12", "rollback",
                "staged update rejected and previous package restored"}};
  const auto found =
      std::find_if(injections.begin(), injections.end(),
                   [injection](const Injection &item) { return item.kind == injection; });
  if (found != injections.end() && result.milestone == found->milestone) {
    result.status = ProbeStatus::Pass;
    result.summary = std::string(found->message);
    result.issues.push_back({std::string(found->code), "injected failure was contained"});
    result.metrics.push_back({"state_committed", "false"});
  }
  return result;
}

} // namespace

std::string_view ToString(ProbeStatus status) noexcept {
  switch (status) {
  case ProbeStatus::Pass:
    return "PASS";
  case ProbeStatus::Partial:
    return "PARTIAL";
  case ProbeStatus::ContractOnly:
    return "CONTRACT_ONLY";
  case ProbeStatus::Unavailable:
    return "UNAVAILABLE";
  case ProbeStatus::Fail:
    return "FAIL";
  }
  return "FAIL";
}

void ProbeRegistry::Register(ProbeDescriptor descriptor) {
  if (descriptor.id.empty() || descriptor.milestone.empty() || !descriptor.run)
    throw std::invalid_argument("showcase probe descriptor is incomplete");
  if (Find(descriptor.id) != nullptr)
    throw std::invalid_argument("duplicate showcase probe id");
  probes_.push_back(std::move(descriptor));
}

const ProbeDescriptor *ProbeRegistry::Find(std::string_view id) const noexcept {
  const auto found = std::find_if(probes_.begin(), probes_.end(),
                                  [id](const ProbeDescriptor &probe) { return probe.id == id; });
  return found == probes_.end() ? nullptr : &*found;
}

std::vector<ProbeResult> ProbeRegistry::RunAll(ErrorInjection injection) const {
  std::vector<ProbeResult> results;
  results.reserve(probes_.size());
  for (const auto &probe : probes_)
    results.push_back(Inject(probe.run(), injection));
  return results;
}

ProbeRegistry ProbeRegistry::CreateV1Registry() {
  ProbeRegistry registry;
  struct Contract final {
    const char *milestone;
    const char *card;
    const char *test;
    ProbeStatus status;
  };
  constexpr std::array contracts{
      Contract{"M0", "Build and module graph", "build.module_graph", ProbeStatus::ContractOnly},
      Contract{"M1", "Core runtime", "core.runtime", ProbeStatus::ContractOnly},
      Contract{"M2", "Renderer contracts", "renderer.contracts", ProbeStatus::ContractOnly},
      Contract{"M3", "Native RHI", "renderer.native_backend", ProbeStatus::Partial},
      Contract{"M4", "Scene lifecycle", "runtime.v1_m4_vertical_slice", ProbeStatus::ContractOnly},
      Contract{"M5", "Asset pipeline", "runtime.v1_m5_asset_pipeline", ProbeStatus::ContractOnly},
      Contract{"M6", "Editor SDK", "runtime.v1_m6_editor_sdk", ProbeStatus::ContractOnly},
      Contract{"M7", "Input, UI, localization", "runtime.v1_m7_input_ui_localization",
               ProbeStatus::ContractOnly},
      Contract{"M8", "Gameplay simulation", "runtime.v1_m8_gameplay_simulation",
               ProbeStatus::ContractOnly},
      Contract{"M9", "Presentation", "runtime.v1_m9_presentation", ProbeStatus::ContractOnly},
      Contract{"M10", "Large world", "runtime.v1_m10_large_world", ProbeStatus::ContractOnly},
      Contract{"M11", "Platform lifecycle", "runtime.v1_m11_platform", ProbeStatus::ContractOnly},
      Contract{"M12", "Shipping", "runtime.v1_m12_shipping", ProbeStatus::ContractOnly}};
  for (const auto &contract : contracts) {
    registry.Register({std::string("v1.") + contract.milestone, contract.milestone, contract.card,
                       contract.test, [contract] {
                         return ProbeResult{std::string("v1.") + contract.milestone,
                                            contract.milestone,
                                            contract.status,
                                            std::string("Mapped to CTest contract ") +
                                                contract.test,
                                            {{"contract_test", contract.test}},
                                            {}};
                       }});
  }
  return registry;
}

std::vector<RoomState> BuildRoomStates(const CapabilitySet &capabilities) {
  const auto room = [](std::string id, bool available, std::string evidence) {
    return RoomState{std::move(id),
                     available ? ProbeStatus::ContractOnly : ProbeStatus::Unavailable,
                     std::move(evidence), false};
  };
  return {room("input-ui-localization", capabilities.input_ui_localization,
               "headless input routing, UI model, and locale fallback"),
          room("gameplay", capabilities.character_physics_navigation_ai,
               "headless character, physics, navigation, and AI counters"),
          room("presentation", capabilities.animation_particle_audio_video,
               "headless animation, particle, audio, and video capability counters"),
          room("world", capabilities.streaming_hlod_terrain_vegetation,
               "headless cell, HLOD, procedural terrain, and vegetation state")};
}

std::string SerializeJson(std::span<const ProbeResult> results, std::span<const RoomState> rooms) {
  std::ostringstream output;
  output << "{\n  \"schema\": \"nexora.showcase.validation.v1\",\n  \"probes\": [\n";
  for (std::size_t index = 0; index < results.size(); ++index) {
    const auto &result = results[index];
    output << "    {\"id\": \"" << Escape(result.id) << "\", \"milestone\": \""
           << Escape(result.milestone) << "\", \"status\": \"" << ToString(result.status)
           << "\", \"summary\": \"" << Escape(result.summary) << "\", \"metrics\": [";
    for (std::size_t metric = 0; metric < result.metrics.size(); ++metric) {
      output << (metric ? ", " : "") << "{\"name\": \"" << Escape(result.metrics[metric].name)
             << "\", \"value\": \"" << Escape(result.metrics[metric].value) << "\"}";
    }
    output << "], \"issues\": [";
    for (std::size_t issue = 0; issue < result.issues.size(); ++issue) {
      output << (issue ? ", " : "") << "{\"code\": \"" << Escape(result.issues[issue].code)
             << "\", \"message\": \"" << Escape(result.issues[issue].message) << "\"}";
    }
    output << "]}" << (index + 1 == results.size() ? "\n" : ",\n");
  }
  output << "  ],\n  \"rooms\": [\n";
  for (std::size_t index = 0; index < rooms.size(); ++index) {
    const auto &room = rooms[index];
    output << "    {\"id\": \"" << Escape(room.id) << "\", \"status\": \"" << ToString(room.status)
           << "\", \"headless_evidence\": \"" << Escape(room.evidence)
           << "\", \"visual_complete\": " << (room.visual_complete ? "true" : "false") << "}"
           << (index + 1 == rooms.size() ? "\n" : ",\n");
  }
  output << "  ]\n}\n";
  return output.str();
}

std::string SerializeMarkdown(std::span<const ProbeResult> results,
                              std::span<const RoomState> rooms) {
  std::ostringstream output;
  output << "# Nexora Showcase Validation\n\nSchema: `nexora.showcase.validation.v1`\n\n"
            "| Probe | Milestone | Status | Summary |\n| --- | --- | --- | --- |\n";
  for (const auto &result : results)
    output << "| `" << result.id << "` | " << result.milestone << " | " << ToString(result.status)
           << " | " << result.summary << " |\n";
  output
      << "\n## Capability-aware rooms\n\n| Room | Status | Headless evidence | Visual complete |\n"
         "| --- | --- | --- | --- |\n";
  for (const auto &room : rooms)
    output << "| `" << room.id << "` | " << ToString(room.status) << " | " << room.evidence << " | "
           << (room.visual_complete ? "yes" : "no") << " |\n";
  return output.str();
}

} // namespace nexora::showcase
