#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nexora::showcase {

enum class ProbeStatus : std::uint8_t { Pass, Partial, ContractOnly, Unavailable, Fail };

struct ProbeMetric final {
  std::string name;
  std::string value;
};

struct ProbeIssue final {
  std::string code;
  std::string message;
};

struct ProbeResult final {
  std::string id;
  std::string milestone;
  ProbeStatus status{ProbeStatus::Unavailable};
  std::string summary;
  std::vector<ProbeMetric> metrics;
  std::vector<ProbeIssue> issues;
};

struct ProbeDescriptor final {
  std::string id;
  std::string milestone;
  std::string card;
  std::string contract_test;
  std::function<ProbeResult()> run;
};

enum class ErrorInjection : std::uint8_t {
  None,
  InvalidAsset,
  DependencyCycle,
  PluginAbiMismatch,
  Rollback
};

struct CapabilitySet final {
  bool input_ui_localization{};
  bool character_physics_navigation_ai{};
  bool animation_particle_audio_video{};
  bool streaming_hlod_terrain_vegetation{};
};

struct RoomState final {
  std::string id;
  ProbeStatus status{ProbeStatus::Unavailable};
  std::string evidence;
  bool visual_complete{};
};

class ProbeRegistry final {
public:
  void Register(ProbeDescriptor descriptor);
  [[nodiscard]] const ProbeDescriptor *Find(std::string_view id) const noexcept;
  [[nodiscard]] std::vector<ProbeResult>
  RunAll(ErrorInjection injection = ErrorInjection::None) const;
  [[nodiscard]] static ProbeRegistry CreateV1Registry();

private:
  std::vector<ProbeDescriptor> probes_;
};

[[nodiscard]] std::string_view ToString(ProbeStatus status) noexcept;
[[nodiscard]] std::vector<RoomState> BuildRoomStates(const CapabilitySet &capabilities);
[[nodiscard]] std::string SerializeJson(std::span<const ProbeResult> results,
                                        std::span<const RoomState> rooms = {});
[[nodiscard]] std::string SerializeMarkdown(std::span<const ProbeResult> results,
                                            std::span<const RoomState> rooms = {});

} // namespace nexora::showcase
