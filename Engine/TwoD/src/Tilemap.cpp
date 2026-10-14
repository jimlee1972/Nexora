#include "Nexora/TwoD/Tilemap.h"
#include "Wire.h"
#include <cmath>
namespace nexora::twod {
namespace {
bool TileValid(const Tile &t) {
  return std::abs(std::int64_t(t.x)) <= 100000 && std::abs(std::int64_t(t.y)) <= 100000 &&
         std::abs(std::int64_t(t.layer)) <= 1024;
}
std::uint64_t TileId(const Tile &t) {
  return 1 + (std::uint64_t(t.layer + 1024) * 200001 + std::uint64_t(t.y + 100000)) * 200001 +
         std::uint64_t(t.x + 100000);
}
} // namespace
ChunkKey Tilemap::Chunk(std::int32_t x, std::int32_t y) noexcept {
  const auto floor = [](std::int32_t n) { return n / 16 - (n < 0 && n % 16 != 0 ? 1 : 0); };
  return {floor(x), floor(y)};
}
bool Tilemap::Valid() const noexcept {
  return detail::ValidId(atlas) && std::isfinite(cellSize.x) && std::isfinite(cellSize.y) &&
         cellSize.x >= 0.001F && cellSize.y >= 0.001F && cellSize.x <= 10 && cellSize.y <= 10 &&
         std::isfinite(origin.x) && std::isfinite(origin.y) && std::abs(origin.x) <= 100000 &&
         std::abs(origin.y) <= 100000;
}
bool Tilemap::Paint(std::span<const Tile> input) {
  if (!Valid() || input.size() > 100000)
    return false;
  auto candidate = tiles_;
  std::set<TileKey> seen;
  for (const auto &t : input) {
    const TileKey key{t.layer, t.y, t.x};
    if (!TileValid(t) || !seen.insert(key).second)
      return false;
    if (t.region)
      candidate.insert_or_assign(key, t);
    else
      candidate.erase(key);
  }
  if (candidate.size() > 100000)
    return false;
  std::map<ChunkKey, std::vector<TileKey>> chunks;
  for (const auto &[key, tile] : candidate)
    chunks[Chunk(tile.x, tile.y)].push_back(key);
  tiles_.swap(candidate);
  chunks_.swap(chunks);
  return true;
}
bool Tilemap::SetChunkLoaded(ChunkKey chunk, bool loaded) {
  if (std::abs(std::int64_t(chunk.first)) > 6251 || std::abs(std::int64_t(chunk.second)) > 6251)
    return false;
  if (loaded)
    unloaded_.erase(chunk);
  else {
    if (!unloaded_.contains(chunk) && unloaded_.size() >= 100000)
      return false;
    unloaded_.insert(chunk);
  }
  return true;
}
std::optional<TilemapInstances> Tilemap::Collect(const Camera &camera) const {
  if (!Valid() || !std::isfinite(camera.center.x) || !std::isfinite(camera.center.y) ||
      !std::isfinite(camera.verticalSize) || camera.verticalSize < 0.00001F ||
      camera.verticalSize > 1e6F || !std::isfinite(camera.rotationRadians) ||
      std::abs(camera.rotationRadians) > 1e6F || !camera.width || !camera.height ||
      camera.width > 16384 || camera.height > 16384)
    return std::nullopt;
  std::vector<SpriteInstance> instances;
  instances.reserve(tiles_.size());
  std::size_t chunkCulled = 0, loaded = 0;
  // Conservative world AABB of a rotated camera; exact per-cell culling follows extraction.
  const double halfY = double(camera.verticalSize) / 2;
  const double halfX = camera.height ? halfY * camera.width / camera.height : 0;
  const double cosine = std::abs(std::cos(camera.rotationRadians)),
               sine = std::abs(std::sin(camera.rotationRadians));
  const double viewX = halfX * cosine + halfY * sine, viewY = halfX * sine + halfY * cosine;
  for (const auto &[chunk, keys] : chunks_) {
    if (unloaded_.contains(chunk))
      continue;
    loaded += keys.size();
    const double minX = origin.x + double(chunk.first) * 16 * cellSize.x,
                 minY = origin.y + double(chunk.second) * 16 * cellSize.y;
    if (minX + 16 * cellSize.x < camera.center.x - viewX || minX > camera.center.x + viewX ||
        minY + 16 * cellSize.y < camera.center.y - viewY || minY > camera.center.y + viewY) {
      chunkCulled += keys.size();
      continue;
    }
    for (const auto &key : keys) {
      const auto &t = tiles_.at(key);
      Sprite s;
      s.atlas = atlas;
      s.region = t.region;
      s.layer = t.layer;
      s.size = cellSize;
      s.pivot = {0, 0};
      runtime::Transform transform;
      transform.x = origin.x + double(t.x) * cellSize.x;
      transform.y = origin.y + double(t.y) * cellSize.y;
      instances.push_back({TileId(t), s, runtime::ToMatrix(transform)});
    }
  }
  return TilemapInstances{std::move(instances), loaded, chunkCulled};
}
std::optional<SpriteFrame> Tilemap::Extract(const Camera &camera,
                                            const AtlasResolver &resolve) const {
  auto inputs = Collect(camera);
  if (!inputs)
    return std::nullopt;
  auto frame = ExtractSprites(inputs->instances, camera, resolve);
  if (frame) {
    frame->statistics.input = inputs->loaded;
    frame->statistics.culled += inputs->chunkCulled;
  }
  return frame;
}
std::optional<std::vector<Shape>> Tilemap::CollisionShapes() const {
  if (!Valid())
    return std::nullopt;
  std::vector<Shape> shapes;
  for (const auto &[key, t] : tiles_) {
    (void)key;
    if (!t.collision || unloaded_.contains(Chunk(t.x, t.y)))
      continue;
    Shape s;
    s.id = TileId(t);
    s.kind = ShapeKind::Box;
    s.group = group;
    s.mask = mask;
    s.center = {origin.x + (float(t.x) + 0.5F) * cellSize.x,
                origin.y + (float(t.y) + 0.5F) * cellSize.y};
    s.halfExtent = {cellSize.x / 2, cellSize.y / 2};
    if (!ValidateShape(s))
      return std::nullopt;
    shapes.push_back(s);
  }
  return shapes;
}
Bytes Tilemap::Encode() const {
  if (!Valid())
    return {};
  detail::Writer w;
  w.U32(0x5432444e);
  w.U32(1);
  w.Id(atlas);
  w.Float(cellSize.x);
  w.Float(cellSize.y);
  w.Float(origin.x);
  w.Float(origin.y);
  w.U32(group);
  w.U32(mask);
  w.U32(static_cast<std::uint32_t>(tiles_.size()));
  for (const auto &[key, t] : tiles_) {
    (void)key;
    w.U32(std::bit_cast<std::uint32_t>(t.x));
    w.U32(std::bit_cast<std::uint32_t>(t.y));
    w.U32(std::bit_cast<std::uint32_t>(t.layer));
    w.U32(t.region);
    w.U32(t.collision ? 1U : 0U);
  }
  w.U32(static_cast<std::uint32_t>(unloaded_.size()));
  for (auto [x, y] : unloaded_) {
    w.U32(std::bit_cast<std::uint32_t>(x));
    w.U32(std::bit_cast<std::uint32_t>(y));
  }
  return std::move(w.bytes);
}
std::optional<Tilemap> Tilemap::Decode(std::span<const std::byte> bytes) {
  detail::Reader r(bytes);
  if (r.U32() != 0x5432444e || r.U32() != 1)
    return std::nullopt;
  Tilemap m;
  m.atlas = r.Id();
  m.cellSize = {r.Float(), r.Float()};
  m.origin = {r.Float(), r.Float()};
  m.group = r.U32();
  m.mask = r.U32();
  const auto count = r.U32();
  if (!r.Valid() || !m.Valid() || count > 100000)
    return std::nullopt;
  std::vector<Tile> tiles;
  tiles.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    Tile t;
    t.x = std::bit_cast<std::int32_t>(r.U32());
    t.y = std::bit_cast<std::int32_t>(r.U32());
    t.layer = std::bit_cast<std::int32_t>(r.U32());
    t.region = r.U32();
    const auto flags = r.U32();
    if (!t.region || flags > 1)
      return std::nullopt;
    t.collision = flags != 0;
    tiles.push_back(t);
  }
  if (!m.Paint(tiles))
    return std::nullopt;
  const auto unloaded = r.U32();
  if (!r.Valid() || unloaded > 100000)
    return std::nullopt;
  for (std::uint32_t i = 0; i < unloaded; ++i) {
    const auto x = std::bit_cast<std::int32_t>(r.U32());
    const auto y = std::bit_cast<std::int32_t>(r.U32());
    if (m.unloaded_.contains({x, y}) || !m.SetChunkLoaded({x, y}, false))
      return std::nullopt;
  }
  return r.Done() ? std::optional(std::move(m)) : std::nullopt;
}
} // namespace nexora::twod
