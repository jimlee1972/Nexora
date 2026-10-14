#pragma once
#include "Nexora/TwoD/Scene.h"
#include <set>
#include <tuple>

namespace nexora::twod {
struct Tile final {
  std::int32_t x{}, y{}, layer{};
  std::uint32_t region{}; // Zero erases a cell.
  bool collision{};
};
using TileKey = std::tuple<std::int32_t, std::int32_t, std::int32_t>;
using ChunkKey = std::pair<std::int32_t, std::int32_t>;
class NEXORA_2D_API Tilemap final {
public:
  AssetId atlas;
  math::Vector2 cellSize{1, 1}, origin{};
  std::uint32_t group{1}, mask{UINT32_MAX};
  // Atomic paint batch; duplicate coordinates and out-of-budget cells reject.
  bool Paint(std::span<const Tile> tiles);
  bool SetChunkLoaded(ChunkKey chunk, bool loaded);
  [[nodiscard]] const std::map<TileKey, Tile> &Tiles() const noexcept { return tiles_; }
  [[nodiscard]] std::optional<SpriteFrame> Extract(const Camera &, const AtlasResolver &) const;
  [[nodiscard]] std::optional<std::vector<Shape>> CollisionShapes() const;
  [[nodiscard]] bool Valid() const noexcept;
  [[nodiscard]] static ChunkKey Chunk(std::int32_t x, std::int32_t y) noexcept;
  [[nodiscard]] Bytes Encode() const;
  [[nodiscard]] static std::optional<Tilemap> Decode(std::span<const std::byte>);

private:
  std::map<TileKey, Tile> tiles_;
  std::map<ChunkKey, std::vector<TileKey>> chunks_;
  // Cells are loaded by default. Explicit unloaded chunks affect rendering and collision together.
  std::set<ChunkKey> unloaded_;
};
} // namespace nexora::twod
