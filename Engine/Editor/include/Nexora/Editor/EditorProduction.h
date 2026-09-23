#pragma once

#include "Nexora/Editor/Api.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nexora::editor {

enum class CapabilityState { Implemented, ReadOnly, Unavailable };

struct ToolDescriptor final {
  std::string id;
  std::string title;
  CapabilityState state{CapabilityState::Unavailable};
  std::string reason;
};

class NEXORA_EDITOR_API SpecializedToolRegistry final {
public:
  bool Register(ToolDescriptor descriptor);
  [[nodiscard]] const ToolDescriptor *Find(std::string_view id) const noexcept;
  [[nodiscard]] std::span<const ToolDescriptor> Tools() const noexcept { return tools_; }

private:
  std::vector<ToolDescriptor> tools_;
};

struct BuildProfile final {
  std::string name;
  std::string target;
  std::string configuration;
  std::string command;
};

struct BuildArtifact final {
  std::string path;
  std::string checksum;
  std::uint64_t bytes{};
};

struct BuildManifest final {
  std::uint32_t schema_version{1};
  BuildProfile profile;
  std::vector<BuildArtifact> artifacts;
};

class NEXORA_EDITOR_API BuildFrontend final {
public:
  [[nodiscard]] static bool Validate(const BuildManifest &manifest, std::string *error = nullptr);
  [[nodiscard]] static bool Write(const BuildManifest &manifest, const std::filesystem::path &path,
                                  std::string *error = nullptr);
};

struct FrameSample final {
  std::uint64_t frame{};
  double cpu_ms{};
  double gpu_ms{};
  std::uint64_t memory_bytes{};
};

class NEXORA_EDITOR_API ProfileSession final {
public:
  bool Add(FrameSample sample);
  [[nodiscard]] std::span<const FrameSample> Samples() const noexcept { return samples_; }
  [[nodiscard]] std::optional<FrameSample> Peak() const noexcept;

private:
  std::vector<FrameSample> samples_;
};

struct NEXORA_EDITOR_API ExtensionPolicy final {
  bool require_signature{true};
  std::vector<std::string> trusted_publishers;
  [[nodiscard]] bool Allows(std::string_view publisher, bool signature_valid) const noexcept;
};

class NEXORA_EDITOR_API TelemetryConsent final {
public:
  void Set(bool enabled) noexcept { enabled_ = enabled; }
  [[nodiscard]] bool Enabled() const noexcept { return enabled_; }
  bool Record(std::string event);
  [[nodiscard]] std::span<const std::string> Events() const noexcept { return events_; }

private:
  bool enabled_{};
  std::vector<std::string> events_;
};

class NEXORA_EDITOR_API VirtualHierarchy final {
public:
  explicit VirtualHierarchy(std::size_t count) : count_(count) {}
  [[nodiscard]] std::pair<std::size_t, std::size_t> Visible(std::size_t first,
                                                            std::size_t capacity) const noexcept;

private:
  std::size_t count_{};
};

} // namespace nexora::editor
