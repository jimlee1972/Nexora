#pragma once

#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Runtime/Api.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nexora::runtime {
enum class ShaderBuildMode : std::uint8_t { Development, Shipping };
enum class ShaderArtifactSource : std::uint8_t { Cooked, DynamicCompile };

[[nodiscard]] NEXORA_RUNTIME_API bool SerializeCookedShaderArtifact(
    const rhi::ShaderModuleArtifact &artifact, std::vector<std::byte> &output,
    std::string &error);
[[nodiscard]] NEXORA_RUNTIME_API bool DeserializeCookedShaderArtifact(
    std::span<const std::byte> input, rhi::ShaderModuleArtifact &artifact, std::string &error);
[[nodiscard]] NEXORA_RUNTIME_API bool LoadCookedShaderArtifact(
    const std::filesystem::path &path, rhi::ShaderModuleArtifact &artifact, std::string &error);

// Caller-thread-only transactional artifact slot. Compiler execution remains an Editor/tool
// service; Runtime accepts only its backend-specific output and never owns compiler objects.
class NEXORA_RUNTIME_API ShaderArtifactSlot final {
public:
  ShaderArtifactSlot(rhi::Backend backend, std::uint64_t expected_layout_hash);

  [[nodiscard]] bool Stage(rhi::ShaderModuleArtifact artifact, ShaderArtifactSource source,
                           std::string &error);
  [[nodiscard]] bool StageCookedFile(const std::filesystem::path &path, std::string &error);
  [[nodiscard]] bool Commit(std::string &error);
  [[nodiscard]] bool Commit(std::uint64_t retire_fence, std::string &error);
  void DiscardStaged() noexcept;
  std::size_t CollectRetired(std::uint64_t completed_fence) noexcept;

  [[nodiscard]] const rhi::ShaderModuleArtifact *Active() const noexcept;
  [[nodiscard]] ShaderBuildMode BuildMode() const noexcept { return mode_; }
  [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }
  [[nodiscard]] std::size_t RetiredCount() const noexcept { return retired_.size(); }

private:
  struct RetiredArtifact final {
    std::uint64_t retire_fence{};
    rhi::ShaderModuleArtifact artifact;
  };

  rhi::Backend backend_;
  ShaderBuildMode mode_;
  std::uint64_t expected_layout_hash_;
  std::optional<rhi::ShaderModuleArtifact> active_;
  std::optional<rhi::ShaderModuleArtifact> staged_;
  std::vector<RetiredArtifact> retired_;
  std::uint64_t generation_{};
};
} // namespace nexora::runtime
