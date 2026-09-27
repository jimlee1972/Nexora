#include "Nexora/Runtime/ShaderRuntime.h"

#include <limits>
#include <utility>

namespace nexora::runtime {
ShaderArtifactSlot::ShaderArtifactSlot(rhi::Backend backend,
                                       std::uint64_t expected_layout_hash)
    : backend_(backend),
#if defined(NEXORA_SHIPPING_ENABLED) && NEXORA_SHIPPING_ENABLED
      mode_(ShaderBuildMode::Shipping),
#else
      mode_(ShaderBuildMode::Development),
#endif
      expected_layout_hash_(expected_layout_hash) {}

bool ShaderArtifactSlot::Stage(rhi::ShaderModuleArtifact artifact, ShaderArtifactSource source,
                               std::string &error) {
  if (source == ShaderArtifactSource::DynamicCompile && mode_ == ShaderBuildMode::Shipping) {
    error = "Shipping Runtime accepts cooked shader artifacts only";
    return false;
  }
  if (!rhi::IsArtifactCompatible(artifact, backend_, expected_layout_hash_)) {
    error = "shader artifact target, payload, or canonical reflection is incompatible";
    return false;
  }
  if (active_ && active_->shader_id != artifact.shader_id) {
    error = "shader reload cannot change the active shader identity";
    return false;
  }
  staged_ = std::move(artifact);
  error.clear();
  return true;
}

bool ShaderArtifactSlot::Commit(std::string &error) {
  if (!staged_) {
    error = "no validated shader artifact is staged";
    return false;
  }
  if (generation_ == std::numeric_limits<std::uint64_t>::max()) {
    error = "shader artifact generation is exhausted";
    return false;
  }
  active_ = std::move(staged_);
  staged_.reset();
  ++generation_;
  error.clear();
  return true;
}

void ShaderArtifactSlot::DiscardStaged() noexcept { staged_.reset(); }

const rhi::ShaderModuleArtifact *ShaderArtifactSlot::Active() const noexcept {
  return active_ ? &*active_ : nullptr;
}
} // namespace nexora::runtime
