#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/TwoD/Assets.h"
#include <functional>
#include <memory>

namespace nexora::editor {
inline constexpr std::size_t kMaximumTwoDSourceBytes = 64 * 1024 * 1024;
inline constexpr std::size_t kMaximumWorkspaceTwoDBytes = 64 * 1024 * 1024;
inline constexpr std::size_t kMaximumWorkspaceTwoDAssets = 4096;
class TwoDAssetPayload;
struct TwoDAssetImportResult final {
  std::shared_ptr<const TwoDAssetPayload> payload;
  std::string error;
  bool cancelled{};
};
// Normalized importer type, bounded owning decode; no IO/publication or dependency resolution.
// A zero expected UUID permits initial discovery of the source's embedded identity.
[[nodiscard]] NEXORA_EDITOR_API TwoDAssetImportResult
ImportTwoDAsset(std::string_view type, std::span<const std::byte> source,
                runtime::AssetUuid expected = {}, const std::function<bool()> &cancelled = {});

class NEXORA_EDITOR_API TwoDAssetPayload final {
public:
  TwoDAssetPayload(const TwoDAssetPayload &) = delete;
  TwoDAssetPayload &operator=(const TwoDAssetPayload &) = delete;
  [[nodiscard]] runtime::AssetUuid Id() const noexcept { return atlas_ ? atlas_->id : clip_->id; }
  [[nodiscard]] std::string_view Type() const noexcept { return atlas_ ? ".nx2atlas" : ".nx2clip"; }
  [[nodiscard]] std::shared_ptr<const twod::Atlas> Atlas() const noexcept { return atlas_; }
  [[nodiscard]] std::shared_ptr<const twod::Clip> Clip() const noexcept { return clip_; }
  [[nodiscard]] std::size_t RetainedBytes() const noexcept { return retained_bytes_; }
  [[nodiscard]] std::vector<runtime::AssetUuid> Dependencies() const {
    return clip_ ? std::vector{clip_->atlas} : std::vector<runtime::AssetUuid>{};
  }

private:
  friend NEXORA_EDITOR_API TwoDAssetImportResult ImportTwoDAsset(std::string_view,
                                                                 std::span<const std::byte>,
                                                                 runtime::AssetUuid,
                                                                 const std::function<bool()> &);
  TwoDAssetPayload() = default;
  std::shared_ptr<const twod::Atlas> atlas_;
  std::shared_ptr<const twod::Clip> clip_;
  std::size_t retained_bytes_{};
};
} // namespace nexora::editor
