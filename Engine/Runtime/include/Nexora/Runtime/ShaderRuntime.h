#pragma once

#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Runtime/Api.h"

#include <cstdint>
#include <optional>
#include <string>

namespace nexora::runtime {
enum class ShaderBuildMode : std::uint8_t { Development, Shipping };
enum class ShaderArtifactSource : std::uint8_t { Cooked, DynamicCompile };

// Caller-thread-only transactional artifact slot. Compiler execution remains an Editor/tool
// service; Runtime accepts only its backend-specific output and never owns compiler objects.
class NEXORA_RUNTIME_API ShaderArtifactSlot final {
public:
  ShaderArtifactSlot(rhi::Backend backend, std::uint64_t expected_layout_hash);

  [[nodiscard]] bool Stage(rhi::ShaderModuleArtifact artifact, ShaderArtifactSource source,
                           std::string &error);
  [[nodiscard]] bool Commit(std::string &error);
  void DiscardStaged() noexcept;

  [[nodiscard]] const rhi::ShaderModuleArtifact *Active() const noexcept;
  [[nodiscard]] ShaderBuildMode BuildMode() const noexcept { return mode_; }
  [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }

private:
  rhi::Backend backend_;
  ShaderBuildMode mode_;
  std::uint64_t expected_layout_hash_;
  std::optional<rhi::ShaderModuleArtifact> active_;
  std::optional<rhi::ShaderModuleArtifact> staged_;
  std::uint64_t generation_{};
};
} // namespace nexora::runtime
