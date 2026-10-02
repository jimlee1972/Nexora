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
  case ProbeStatus::Unsupported:
    return "UNSUPPORTED";
  case ProbeStatus::NotRun:
    return "NOT_RUN";
  case ProbeStatus::Blocked:
    return "BLOCKED";
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
      Contract{"M0", "Build and module graph", "build.module_graph", ProbeStatus::NotRun},
      Contract{"M1", "Core runtime", "core.runtime", ProbeStatus::NotRun},
      Contract{"M2", "Renderer contracts", "renderer.contracts", ProbeStatus::NotRun},
      Contract{"M3", "Native RHI", "renderer.native_backend", ProbeStatus::Unsupported},
      Contract{"M4", "Scene lifecycle", "runtime.v1_m4_vertical_slice", ProbeStatus::NotRun},
      Contract{"M5", "Asset pipeline", "runtime.v1_m5_asset_pipeline", ProbeStatus::NotRun},
      Contract{"M6", "Editor SDK", "runtime.v1_m6_editor_sdk", ProbeStatus::NotRun},
      Contract{"M7", "Input, UI, localization", "runtime.v1_m7_input_ui_localization",
               ProbeStatus::NotRun},
      Contract{"M8", "Gameplay simulation", "runtime.v1_m8_gameplay_simulation",
               ProbeStatus::NotRun},
      Contract{"M9", "Presentation", "runtime.v1_m9_presentation", ProbeStatus::NotRun},
      Contract{"M10", "Large world", "runtime.v1_m10_large_world", ProbeStatus::NotRun},
      Contract{"M11", "Platform lifecycle", "runtime.v1_m11_platform", ProbeStatus::NotRun},
      Contract{"M12", "Shipping", "runtime.v1_m12_shipping", ProbeStatus::NotRun}};
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

ValidationLabView BuildValidationLab(std::span<const ProbeResult> results) {
  ValidationLabView view;
  view.cards.reserve(results.size());
  for (const auto &result : results) {
    const auto milestone = std::stoi(result.milestone.substr(1));
    std::string room = "hub";
    if (milestone >= 4 && milestone <= 6)
      room = "scene";
    else if (milestone == 7)
      room = "input-ui-localization";
    else if (milestone == 8)
      room = "gameplay";
    else if (milestone == 9)
      room = "presentation";
    else if (milestone == 10)
      room = "world";
    else if (milestone >= 11)
      room = "shipping";
    const auto contract =
        std::find_if(result.metrics.begin(), result.metrics.end(),
                     [](const ProbeMetric &metric) { return metric.name == "contract_test"; });
    view.cards.push_back({result, contract == result.metrics.end() ? "" : contract->value,
                          std::move(room), 0x4e580000ULL + static_cast<std::uint64_t>(milestone)});
  }
  view.failure_states = {{"invalid_asset", "Invalid asset", "M5", ProbeStatus::NotRun},
                         {"dependency_cycle", "Dependency cycle", "M0", ProbeStatus::NotRun},
                         {"plugin_abi_mismatch", "Plugin ABI mismatch", "M6", ProbeStatus::NotRun},
                         {"rollback", "Rollback", "M12", ProbeStatus::NotRun}};
  return view;
}

void DrawValidationLab(std::span<std::byte> rgba, std::uint32_t width, std::uint32_t height,
                       const ValidationLabView &view) {
  if (width == 0 || height == 0 || rgba.size() != static_cast<std::size_t>(width) * height * 4U)
    throw std::invalid_argument("validation lab target must be a complete RGBA8 image");
  const auto pixel = [&](std::uint32_t x, std::uint32_t y, std::array<std::uint8_t, 3> color) {
    const auto offset = (static_cast<std::size_t>(y) * width + x) * 4U;
    rgba[offset] = static_cast<std::byte>(color[0]);
    rgba[offset + 1] = static_cast<std::byte>(color[1]);
    rgba[offset + 2] = static_cast<std::byte>(color[2]);
    rgba[offset + 3] = std::byte{255};
  };
  const auto color = [](ProbeStatus status) {
    switch (status) {
    case ProbeStatus::Pass:
      return std::array<std::uint8_t, 3>{45, 180, 95};
    case ProbeStatus::Fail:
      return std::array<std::uint8_t, 3>{220, 65, 70};
    case ProbeStatus::Unsupported:
      return std::array<std::uint8_t, 3>{90, 105, 125};
    case ProbeStatus::NotRun:
      return std::array<std::uint8_t, 3>{210, 155, 45};
    case ProbeStatus::Blocked:
      return std::array<std::uint8_t, 3>{145, 80, 185};
    }
    return std::array<std::uint8_t, 3>{220, 65, 70};
  };
  constexpr std::uint32_t columns = 7;
  const auto cardWidth = std::max(8U, (width - std::min(width, 32U)) / columns);
  const auto cardHeight = std::max(6U, std::min(28U, height / 12U));
  for (std::size_t index = 0; index < view.cards.size(); ++index) {
    const auto x0 = 12U + static_cast<std::uint32_t>(index % columns) * cardWidth;
    const auto y0 = 12U + static_cast<std::uint32_t>(index / columns) * (cardHeight + 5U);
    for (std::uint32_t y = y0; y < std::min(height, y0 + cardHeight); ++y)
      for (std::uint32_t x = x0; x < std::min(width, x0 + cardWidth - 4U); ++x)
        pixel(x, y, color(view.cards[index].result.status));
  }
  for (std::size_t index = 0; index < view.failure_states.size(); ++index) {
    const auto x0 = 12U + static_cast<std::uint32_t>(index) * 18U;
    const auto y0 = std::min(height - 1U, 12U + 2U * (cardHeight + 5U));
    for (std::uint32_t y = y0; y < std::min(height, y0 + 7U); ++y)
      for (std::uint32_t x = x0; x < std::min(width, x0 + 12U); ++x)
        pixel(x, y, color(view.failure_states[index].status));
  }
  for (std::size_t index = 0; index < view.status_legend.size(); ++index) {
    const auto x0 = width > 90U ? width - 90U + static_cast<std::uint32_t>(index) * 16U : 0U;
    for (std::uint32_t y = 4; y < std::min(height, 10U); ++y)
      for (std::uint32_t x = x0; x < std::min(width, x0 + 12U); ++x)
        pixel(x, y, color(view.status_legend[index]));
  }
}

std::string SerializeValidationLabView(const ValidationLabView &view) {
  std::ostringstream output;
  output << "{\"cards\": [";
  for (std::size_t index = 0; index < view.cards.size(); ++index) {
    const auto &card = view.cards[index];
    output << (index ? ", " : "") << "{\"id\": \"" << Escape(card.result.id) << "\", \"status\": \""
           << ToString(card.result.status) << "\", \"contract_test\": \""
           << Escape(card.contract_test) << "\", \"room\": \"" << Escape(card.room_id)
           << "\", \"world_object\": " << card.world_object << "}";
  }
  output << "], \"status_legend\": [";
  for (std::size_t index = 0; index < view.status_legend.size(); ++index)
    output << (index ? ", " : "") << "\"" << ToString(view.status_legend[index]) << "\"";
  output << "], \"failure_states\": [";
  for (std::size_t index = 0; index < view.failure_states.size(); ++index) {
    const auto &state = view.failure_states[index];
    output << (index ? ", " : "") << "{\"code\": \"" << Escape(state.code) << "\", \"label\": \""
           << Escape(state.label) << "\", \"milestone\": \"" << Escape(state.milestone)
           << "\", \"status\": \"" << ToString(state.status) << "\"}";
  }
  output << "]}";
  return output.str();
}

std::vector<RoomState> BuildRoomStates(const CapabilitySet &capabilities) {
  const auto room = [](std::string id, bool available, std::string evidence) {
    return RoomState{std::move(id), available ? ProbeStatus::NotRun : ProbeStatus::Unsupported,
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
  const auto cell = [](std::string_view value) {
    std::string escaped;
    for (const char c : value) {
      if (c == '|')
        escaped += "&#124;";
      else if (c == '\n' || c == '\r')
        escaped += ' ';
      else
        escaped += c;
    }
    return escaped;
  };
  for (const auto &result : results) {
    if (result.metrics.empty() && result.issues.empty())
      continue;
    output << "\n## " << result.milestone << " run details\n\n"
           << "| Input / output metric | Value |\n| --- | --- |\n";
    for (const auto &metric : result.metrics)
      output << "| " << cell(metric.name) << " | " << cell(metric.value) << " |\n";
    for (const auto &issue : result.issues)
      output << "| Issue: " << cell(issue.code) << " | " << cell(issue.message) << " |\n";
  }
  output
      << "\n## Capability-aware rooms\n\n| Room | Status | Headless evidence | Visual complete |\n"
         "| --- | --- | --- | --- |\n";
  for (const auto &room : rooms)
    output << "| `" << room.id << "` | " << ToString(room.status) << " | " << room.evidence << " | "
           << (room.visual_complete ? "yes" : "no") << " |\n";
  return output.str();
}

} // namespace nexora::showcase
