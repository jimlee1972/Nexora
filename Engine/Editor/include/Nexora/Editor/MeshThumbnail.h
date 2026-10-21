#pragma once

#include "Nexora/Core/JobSystem.h"
#include "Nexora/Editor/ContentBrowser.h"

#include <array>
#include <functional>

namespace nexora::editor {
inline constexpr std::size_t kThumbnailMaximumVertices = 16384;
inline constexpr std::size_t kThumbnailMaximumIndices = 24576;
struct MeshThumbnailPixels final {
  static constexpr std::size_t kSide = 64;
  // Top-left origin, opaque RGBA8. Neutral flat shading, no materials/textures or GPU resources.
  std::array<std::uint8_t, kSide * kSide * 4> rgba{};
};
struct MeshThumbnailBuild final {
  std::unique_ptr<MeshThumbnailPixels> pixels;
  std::string error;
  bool cancelled{};
};
// Pure bounded CPU rasterization; derives framing from positions, not cached mesh bounds.
// The caller serializes source mutation. Allocation exceptions propagate. Cancellation is checked
// during validation and every triangle/scanline. No files, documents or native resources are used.
[[nodiscard]] NEXORA_EDITOR_API MeshThumbnailBuild
BuildMeshThumbnail(const MeshGeometry &mesh, const std::function<bool()> &cancelled = {});

enum class MeshThumbnailPhase {
  Queued,
  Rendering,
  AwaitingPublish,
  Ready,
  Failed,
  Cancelled,
  Stale
};
struct MeshThumbnailSnapshot final {
  std::uint64_t operation{}, project_generation{};
  runtime::AssetUuid asset;
  std::string artifact_revision;
  MeshThumbnailPhase phase{MeshThumbnailPhase::Queued};
  std::string message;
  std::shared_ptr<const MeshThumbnailPixels> pixels;
};
struct MeshThumbnailLimits final {
  std::size_t entries{16}; // 1..64, including failed/cancelled entries
  std::size_t pending{4};  // 1..4, including cancelled jobs awaiting collection
};
// One owner per project session. All calls/destruction are serialized on its non-worker thread.
// Content and the running JobSystem outlive this owner. Workers retain only bounded mesh copies;
// they never borrow Content, documents, this owner or a GPU. Poll alone publishes current results.
// Clear on project replacement, including reopens reusing the same generation. Generation changes
// also clear on Request/Poll. Immutable returned pixels may outlive eviction, Clear and
// destruction.
class NEXORA_EDITOR_API MeshThumbnailCache final {
public:
  MeshThumbnailCache(core::JobSystem &jobs, const ContentBrowserModel &content,
                     MeshThumbnailLimits limits = {});
  ~MeshThumbnailCache();
  MeshThumbnailCache(const MeshThumbnailCache &) = delete;
  MeshThumbnailCache &operator=(const MeshThumbnailCache &) = delete;
  // Deduplicates the exact artifact revision and immutable geometry owner, including failures.
  // False reports invalid input, stopped/busy state or submission failure without evicting a hit.
  // A changed revision replaces its observation; Invalidate explicitly enables same-key retry.
  bool Request(runtime::AssetUuid asset, std::string *error = nullptr);
  std::size_t Poll();
  [[nodiscard]] std::optional<MeshThumbnailSnapshot> Get(runtime::AssetUuid asset);
  bool Cancel(runtime::AssetUuid asset) noexcept;
  bool Invalidate(runtime::AssetUuid asset) noexcept;
  void Clear() noexcept;
  void Shutdown() noexcept; // cancel and drain only owned jobs, before releasing worker inputs
  [[nodiscard]] std::size_t Size() const noexcept;
  [[nodiscard]] std::size_t Pending() const noexcept;

private:
  struct Implementation;
  std::unique_ptr<Implementation> implementation_;
};
} // namespace nexora::editor
