#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/TwoD/Assets.h"
#include <memory>
#include <unordered_map>

namespace nexora::editor {

struct TwoDPlayAssetLimits final {
  std::size_t assets{4096};
  std::size_t payload_bytes{64 * 1024 * 1024};
};

// Immutable CPU snapshot of one explicitly selected cooked generation. Capture and store
// mutations must be serialized by the owner. This does not establish project/source provenance.
class NEXORA_EDITOR_API TwoDPlayAssets final {
public:
  [[nodiscard]] static std::shared_ptr<const TwoDPlayAssets>
  Capture(runtime::AssetGenerationStore &, std::uint64_t generation,
          std::span<const runtime::AssetUuid> roots, std::string *error = nullptr,
          TwoDPlayAssetLimits limits = {});
  [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }
  [[nodiscard]] std::size_t Size() const noexcept { return atlases_.size() + clips_.size(); }
  // Unique cooked payload bytes, not allocator capacity, RSS or GPU memory.
  [[nodiscard]] std::size_t PayloadBytes() const noexcept { return payload_bytes_; }
  [[nodiscard]] std::shared_ptr<const twod::Atlas> Atlas(runtime::AssetUuid) const;
  [[nodiscard]] std::shared_ptr<const twod::Clip> Clip(runtime::AssetUuid) const;

private:
  TwoDPlayAssets() = default;
  std::uint64_t generation_{};
  std::size_t payload_bytes_{};
  std::unordered_map<runtime::AssetUuid, std::shared_ptr<const twod::Atlas>, runtime::AssetUuidHash>
      atlases_;
  std::unordered_map<runtime::AssetUuid, std::shared_ptr<const twod::Clip>, runtime::AssetUuidHash>
      clips_;
};

} // namespace nexora::editor
