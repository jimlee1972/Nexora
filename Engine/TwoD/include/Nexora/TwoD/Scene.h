#pragma once
#include "Nexora/Math/Math.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/Runtime/Runtime.h"
#include "Nexora/TwoD/Assets.h"
#include "Nexora/TwoD/Queries.h"
#include <functional>
#include <map>
#include <memory>

namespace nexora::twod {
struct Sprite final {
  AssetId atlas;
  std::uint32_t region{};
  math::Vector2 size{1, 1}, pivot{0.5F, 0.5F};
  std::array<float, 4> color{1, 1, 1, 1};
  std::int32_t layer{}, order{};
  bool flipX{}, flipY{}, sortY{}, visible{true};
};
[[nodiscard]] NEXORA_2D_API bool ValidateSprite(const Sprite &sprite) noexcept;
struct Camera final {
  math::Vector2 center{};
  float verticalSize{20}, rotationRadians{};
  std::uint32_t width{1280}, height{720};
};
struct AtlasBinding final {
  std::shared_ptr<const Atlas> atlas;
  std::uint64_t texture{};
};
using AtlasResolver = std::function<std::optional<AtlasBinding>(AssetId)>;
struct FrameStatistics final {
  std::size_t input{}, visible{}, culled{}, unavailable{}, batches{}, vertices{}, indices{};
  double coveredPixels{};
};
struct NEXORA_2D_API SpriteFrame final {
  std::vector<Nexora::Presentation::UiVertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<Nexora::Presentation::UiDrawCommand> batches;
  std::vector<AtlasBinding> retainedAtlases;
  FrameStatistics statistics;
  [[nodiscard]] std::vector<Nexora::Presentation::UiTextureUpload> Uploads() const;
  void OffsetViewport(std::uint32_t x, std::uint32_t y);
  [[nodiscard]] Nexora::Presentation::SurfaceStatus
  Draw(Nexora::Presentation::RenderSurface &surface,
       const Nexora::Presentation::UiDrawData &overlay = {},
       std::uint32_t background = 0xFF29140A) const;
};
struct Collider final {
  ShapeKind kind{ShapeKind::Circle};
  math::Vector2 offset{}, halfExtent{0.5F, 0.5F};
  float radius{0.5F};
  std::uint32_t group{1}, mask{UINT32_MAX};
};
[[nodiscard]] NEXORA_2D_API bool ValidateCollider(const Collider &) noexcept;
struct SpriteInstance final {
  std::uint64_t id{};
  Sprite sprite;
  runtime::TransformMatrix matrix;
};
[[nodiscard]] NEXORA_2D_API std::optional<SpriteFrame>
ExtractSprites(std::span<const SpriteInstance>, const Camera &, const AtlasResolver &);
// Sprite state augments an existing World/scene. The World must outlive this owner; methods
// serialize with World mutations. No duplicate transforms or native backend objects are stored.
class NEXORA_2D_API SpriteScene final {
public:
  SpriteScene(runtime::World &world, runtime::Id scene);
  bool Set(runtime::Id entity, Sprite sprite);
  bool Remove(runtime::Id entity);
  bool SetCollider(runtime::Id entity, Collider collider);
  bool RemoveCollider(runtime::Id entity);
  [[nodiscard]] const Collider *FindCollider(runtime::Id entity) const noexcept;
  [[nodiscard]] std::optional<std::vector<Shape>> CollisionShapes() const;
  [[nodiscard]] const Sprite *Find(runtime::Id entity) const noexcept;
  [[nodiscard]] const std::map<runtime::Id, Sprite> &Sprites() const noexcept { return sprites_; }
  [[nodiscard]] std::optional<SpriteFrame> Extract(const Camera &camera,
                                                   const AtlasResolver &resolve) const;
  [[nodiscard]] std::optional<Bytes> Snapshot() const;
  // Editor-only atomic replacement; parse/validate in a staging World before publishing.
  bool Restore(std::span<const std::byte> snapshot);
  [[nodiscard]] static std::optional<SpriteScene> Load(runtime::World &world,
                                                       std::span<const std::byte> snapshot);
  [[nodiscard]] runtime::Id SceneId() const noexcept { return scene_; }
  [[nodiscard]] runtime::World &WorldOwner() const noexcept { return *world_; }

private:
  runtime::World *world_;
  runtime::Id scene_;
  std::map<runtime::Id, Sprite> sprites_;
  std::map<runtime::Id, Collider> colliders_;
};
} // namespace nexora::twod
