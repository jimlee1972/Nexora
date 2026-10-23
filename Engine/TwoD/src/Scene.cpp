#include "Nexora/TwoD/Scene.h"
#include "Wire.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
namespace nexora::twod {
namespace {
void WriteSprite(detail::Writer &w, const Sprite &s) {
  w.Id(s.atlas);
  w.U32(s.region);
  w.Float(s.size.x);
  w.Float(s.size.y);
  w.Float(s.pivot.x);
  w.Float(s.pivot.y);
  for (const float c : s.color)
    w.Float(c);
  w.U32(std::bit_cast<std::uint32_t>(s.layer));
  w.U32(std::bit_cast<std::uint32_t>(s.order));
  w.U32((s.flipX ? 1U : 0U) | (s.flipY ? 2U : 0U) | (s.sortY ? 4U : 0U) | (s.visible ? 8U : 0U));
}
std::optional<Sprite> ReadSprite(detail::Reader &r) {
  Sprite s;
  s.atlas = r.Id();
  s.region = r.U32();
  s.size = {r.Float(), r.Float()};
  s.pivot = {r.Float(), r.Float()};
  for (float &c : s.color)
    c = r.Float();
  s.layer = std::bit_cast<std::int32_t>(r.U32());
  s.order = std::bit_cast<std::int32_t>(r.U32());
  const auto flags = r.U32();
  if (flags > 15)
    return std::nullopt;
  s.flipX = (flags & 1) != 0;
  s.flipY = (flags & 2) != 0;
  s.sortY = (flags & 4) != 0;
  s.visible = (flags & 8) != 0;
  return r.Valid() && ValidateSprite(s) ? std::optional(s) : std::nullopt;
}
void WriteCollider(detail::Writer &w, const Collider &c) {
  w.U32(static_cast<std::uint32_t>(c.kind));
  w.Float(c.offset.x);
  w.Float(c.offset.y);
  w.Float(c.halfExtent.x);
  w.Float(c.halfExtent.y);
  w.Float(c.radius);
  w.U32(c.group);
  w.U32(c.mask);
}
std::optional<Collider> ReadCollider(detail::Reader &r) {
  Collider c;
  const auto kind = r.U32();
  if (kind > 1)
    return std::nullopt;
  c.kind = static_cast<ShapeKind>(kind);
  c.offset = {r.Float(), r.Float()};
  c.halfExtent = {r.Float(), r.Float()};
  c.radius = r.Float();
  c.group = r.U32();
  c.mask = r.U32();
  return r.Valid() && ValidateCollider(c) ? std::optional(c) : std::nullopt;
}
struct DecodedScene {
  std::string world;
  std::map<runtime::Id, Sprite> sprites;
  std::map<runtime::Id, Collider> colliders;
};
std::optional<DecodedScene> DecodeScene(std::span<const std::byte> data) {
  detail::Reader r(data);
  if (r.U32() != 0x5332444e || r.U32() != 2)
    return std::nullopt;
  DecodedScene scene;
  scene.world = r.Text(16 * 1024 * 1024);
  const auto count = r.U32();
  if (!r.Valid() || count > 100000)
    return std::nullopt;
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto id = r.U64();
    auto sprite = ReadSprite(r);
    if (!id || !sprite || !scene.sprites.emplace(id, *sprite).second)
      return std::nullopt;
  }
  const auto colliderCount = r.U32();
  if (!r.Valid() || colliderCount > 100000)
    return std::nullopt;
  for (std::uint32_t i = 0; i < colliderCount; ++i) {
    const auto id = r.U64();
    auto collider = ReadCollider(r);
    if (!id || !collider || !scene.colliders.emplace(id, *collider).second)
      return std::nullopt;
  }
  if (!r.Done())
    return std::nullopt;
  runtime::World staging;
  const auto id = staging.LoadSceneSnapshot(scene.world);
  if (!id)
    return std::nullopt;
  const auto *loaded = staging.FindScene(*id);
  std::set<runtime::Id> members;
  for (const auto &e : loaded->entities)
    members.insert(e.id);
  for (const auto &[entity, sprite] : scene.sprites) {
    (void)sprite;
    if (!members.contains(entity))
      return std::nullopt;
  }
  for (const auto &[entity, collider] : scene.colliders) {
    (void)collider;
    if (!members.contains(entity))
      return std::nullopt;
  }
  return scene;
}
bool CameraValid(const Camera &c) {
  return std::isfinite(c.center.x) && std::isfinite(c.center.y) && std::isfinite(c.verticalSize) &&
         c.verticalSize >= 0.00001F && c.verticalSize <= 1000000 &&
         std::isfinite(c.rotationRadians) && std::abs(c.rotationRadians) <= 1000000 &&
         c.width > 0 && c.height > 0 && c.width <= 16384 && c.height <= 16384;
}
} // namespace
bool ValidateSprite(const Sprite &s) noexcept {
  if (!detail::ValidId(s.atlas) || !s.region || !std::isfinite(s.size.x) ||
      !std::isfinite(s.size.y) || s.size.x <= 0 || s.size.y <= 0 || s.size.x > 1000000 ||
      s.size.y > 1000000 || !std::isfinite(s.pivot.x) || !std::isfinite(s.pivot.y) ||
      std::abs(s.pivot.x) > 10 || std::abs(s.pivot.y) > 10)
    return false;
  for (float c : s.color)
    if (!std::isfinite(c) || c < 0 || c > 1)
      return false;
  return true;
}
bool ValidateCollider(const Collider &c) noexcept {
  Shape s;
  s.kind = c.kind;
  s.center = c.offset;
  s.halfExtent = c.halfExtent;
  s.radius = c.radius;
  return ValidateShape(s);
}
bool SpriteScene::SetCollider(runtime::Id id, Collider collider) {
  const auto *scene = world_->FindScene(scene_);
  if (!scene || !ValidateCollider(collider) || scene->state == runtime::SceneState::Unloaded ||
      scene->state == runtime::SceneState::Unloading ||
      (!colliders_.contains(id) && colliders_.size() >= 100000) ||
      std::none_of(scene->entities.begin(), scene->entities.end(),
                   [&](const auto &e) { return e.id == id; }))
    return false;
  colliders_.insert_or_assign(id, collider);
  return true;
}
bool SpriteScene::RemoveCollider(runtime::Id id) { return colliders_.erase(id) != 0; }
const Collider *SpriteScene::FindCollider(runtime::Id id) const noexcept {
  auto it = colliders_.find(id);
  return it == colliders_.end() ? nullptr : &it->second;
}
std::optional<std::vector<Shape>> SpriteScene::CollisionShapes() const {
  auto poses = world_->SceneWorldPoses(scene_);
  if (!poses)
    return std::nullopt;
  std::vector<Shape> shapes;
  shapes.reserve(colliders_.size());
  for (const auto &pose : *poses) {
    const auto *c = FindCollider(pose.id);
    if (!c)
      continue;
    const auto &m = pose.matrix;
    Shape s;
    s.id = pose.id;
    s.kind = c->kind;
    s.group = c->group;
    s.mask = c->mask;
    s.center = {float(m[0] * c->offset.x + m[4] * c->offset.y + m[12]),
                float(m[1] * c->offset.x + m[5] * c->offset.y + m[13])};
    // Circle uses the largest singular value; boxes become conservative world AABBs.
    const double a = m[0] * m[0] + m[1] * m[1], b = m[4] * m[4] + m[5] * m[5],
                 d = m[0] * m[4] + m[1] * m[5];
    s.radius = float(c->radius * std::sqrt((a + b + std::sqrt((a - b) * (a - b) + 4 * d * d)) / 2));
    s.halfExtent = {float(std::abs(m[0]) * c->halfExtent.x + std::abs(m[4]) * c->halfExtent.y),
                    float(std::abs(m[1]) * c->halfExtent.x + std::abs(m[5]) * c->halfExtent.y)};
    if (!ValidateShape(s))
      return std::nullopt;
    shapes.push_back(s);
  }
  return shapes;
}
SpriteScene::SpriteScene(runtime::World &world, runtime::Id scene) : world_(&world), scene_(scene) {
  const auto *record = world.FindScene(scene);
  if (!record || record->state == runtime::SceneState::Unloaded ||
      record->state == runtime::SceneState::Unloading)
    throw std::invalid_argument("2D scene must be live");
}
bool SpriteScene::Set(runtime::Id entity, Sprite sprite) {
  const auto *scene = world_->FindScene(scene_);
  if (!scene || scene->state == runtime::SceneState::Unloading ||
      scene->state == runtime::SceneState::Unloaded || !ValidateSprite(sprite))
    return false;
  if (std::none_of(scene->entities.begin(), scene->entities.end(),
                   [&](const runtime::Entity &e) { return e.id == entity; }))
    return false;
  if (!sprites_.contains(entity) && sprites_.size() >= 100000)
    return false;
  sprites_.insert_or_assign(entity, sprite);
  return true;
}
bool SpriteScene::Remove(runtime::Id entity) { return sprites_.erase(entity) != 0; }
const Sprite *SpriteScene::Find(runtime::Id id) const noexcept {
  auto it = sprites_.find(id);
  return it == sprites_.end() ? nullptr : &it->second;
}
std::optional<SpriteFrame> SpriteScene::Extract(const Camera &camera,
                                                const AtlasResolver &resolve) const {
  if (!CameraValid(camera) || !resolve)
    return std::nullopt;
  auto poses = world_->SceneWorldPoses(scene_);
  if (!poses)
    return std::nullopt;
  std::vector<SpriteInstance> instances;
  instances.reserve(sprites_.size());
  for (const auto &pose : *poses) {
    const auto *sprite = Find(pose.id);
    if (sprite)
      instances.push_back({pose.id, *sprite, pose.matrix});
  }
  auto frame = ExtractSprites(instances, camera, resolve);
  if (frame)
    frame->statistics.input = sprites_.size();
  return frame;
}
std::optional<SpriteFrame> ExtractSprites(std::span<const SpriteInstance> instances,
                                          const Camera &camera, const AtlasResolver &resolve) {
  if (!CameraValid(camera) || !resolve || instances.size() > 100000)
    return std::nullopt;
  struct Item {
    std::uint64_t id;
    const Sprite *sprite;
    runtime::TransformMatrix matrix;
    double y;
  };
  std::vector<Item> items;
  items.reserve(instances.size());
  std::vector<std::uint64_t> ids;
  ids.reserve(instances.size());
  for (const auto &instance : instances) {
    if (!instance.id || !ValidateSprite(instance.sprite))
      return std::nullopt;
    ids.push_back(instance.id);
    if (instance.matrix[3] != 0 || instance.matrix[7] != 0 || instance.matrix[11] != 0 ||
        instance.matrix[15] != 1)
      return std::nullopt;
    for (const auto value : instance.matrix)
      if (!std::isfinite(value) || std::abs(value) > 1e12)
        return std::nullopt;
    const auto &sprite = instance.sprite;
    if (sprite.visible && sprite.color[3] > 0)
      items.push_back(
          {instance.id, &sprite, instance.matrix, sprite.sortY ? -instance.matrix[13] : 0});
  }
  if (!std::is_sorted(ids.begin(), ids.end()))
    std::sort(ids.begin(), ids.end());
  if (std::adjacent_find(ids.begin(), ids.end()) != ids.end())
    return std::nullopt;
  // Sort compact references while preserving the pre-resolver matrix/identity snapshot.
  std::vector<const Item *> ordered;
  ordered.reserve(items.size());
  for (const auto &item : items)
    ordered.push_back(&item);
  const auto byOrder = [](const Item *a, const Item *b) {
    return std::tie(a->sprite->layer, a->sprite->order, a->y, a->id) <
           std::tie(b->sprite->layer, b->sprite->order, b->y, b->id);
  };
  if (!std::is_sorted(ordered.begin(), ordered.end(), byOrder))
    std::sort(ordered.begin(), ordered.end(), byOrder);
  SpriteFrame frame;
  frame.statistics.input = instances.size();
  frame.vertices.reserve(items.size() * 4);
  frame.indices.reserve(items.size() * 6);
  std::unordered_map<AssetId, std::size_t, runtime::AssetUuidHash> assets;
  std::set<std::uint64_t> textureIds;
  std::unordered_set<AssetId, runtime::AssetUuidHash> unavailable;
  const double scale = double(camera.height) / camera.verticalSize,
               cosine = std::cos(camera.rotationRadians), sine = std::sin(camera.rotationRadians);
  for (const auto *item : ordered) {
    const auto &s = *item->sprite;
    const auto &matrix = item->matrix;
    std::array<Nexora::Presentation::UiVertex, 4> vertices{};
    double minimumX = INFINITY, minimumY = INFINITY, maximumX = -INFINITY, maximumY = -INFINITY;
    for (std::size_t i = 0; i < 4; ++i) {
      const double x =
          ((i == 1 || i == 2) ? 1 : 0) * double(s.size.x) - double(s.pivot.x) * s.size.x;
      const double y = (i >= 2 ? 1 : 0) * double(s.size.y) - double(s.pivot.y) * s.size.y;
      const double wx = matrix[0] * x + matrix[4] * y + matrix[12] - camera.center.x;
      const double wy = matrix[1] * x + matrix[5] * y + matrix[13] - camera.center.y;
      const double px = (cosine * wx + sine * wy) * scale + double(camera.width) / 2;
      const double py = -(-sine * wx + cosine * wy) * scale + double(camera.height) / 2;
      if (!std::isfinite(px) || !std::isfinite(py) ||
          std::abs(px) > std::numeric_limits<float>::max() ||
          std::abs(py) > std::numeric_limits<float>::max())
        return std::nullopt;
      vertices[i].position[0] = static_cast<float>(px);
      vertices[i].position[1] = static_cast<float>(py);
      minimumX = std::min(minimumX, px);
      maximumX = std::max(maximumX, px);
      minimumY = std::min(minimumY, py);
      maximumY = std::max(maximumY, py);
    }
    if (maximumX < 0 || maximumY < 0 || minimumX >= camera.width || minimumY >= camera.height) {
      ++frame.statistics.culled;
      continue;
    }
    if (unavailable.contains(s.atlas)) {
      ++frame.statistics.unavailable;
      continue;
    }
    auto cached = assets.find(s.atlas);
    std::size_t bindingIndex = 0;
    if (cached == assets.end()) {
      auto binding = resolve(s.atlas);
      if (!binding) {
        unavailable.insert(s.atlas);
        ++frame.statistics.unavailable;
        continue;
      }
      if (!binding->atlas || binding->atlas->id != s.atlas || !ValidateAtlas(*binding->atlas) ||
          !binding->texture || binding->texture >= UINT64_MAX - 1 ||
          !textureIds.insert(binding->texture).second || assets.size() >= 64)
        return std::nullopt;
      bindingIndex = frame.retainedAtlases.size();
      assets.emplace(s.atlas, bindingIndex);
      frame.retainedAtlases.push_back(std::move(*binding));
    } else
      bindingIndex = cached->second;
    const auto &binding = frame.retainedAtlases[bindingIndex];
    const auto &atlas = *binding.atlas;
    const auto *region = atlas.Find(s.region);
    if (!region)
      return std::nullopt;
    std::uint32_t color = 0;
    for (unsigned i = 0; i < 4; ++i)
      color |= static_cast<std::uint32_t>(std::lround(s.color[i] * 255)) << (8 * i);
    for (std::size_t i = 0; i < 4; ++i) {
      bool right = i == 1 || i == 2, top = i >= 2;
      if (s.flipX)
        right = !right;
      if (s.flipY)
        top = !top;
      vertices[i].uv[0] = float(region->x + (right ? region->width : 0)) / float(atlas.width);
      vertices[i].uv[1] = float(region->y + (top ? 0 : region->height)) / float(atlas.height);
      vertices[i].color = color;
    }
    const auto base = static_cast<std::uint32_t>(frame.vertices.size());
    const auto offset = static_cast<std::uint32_t>(frame.indices.size());
    frame.vertices.insert(frame.vertices.end(), vertices.begin(), vertices.end());
    for (const auto index : {0U, 1U, 2U, 0U, 2U, 3U})
      frame.indices.push_back(base + index);
    if (!frame.batches.empty() && frame.batches.back().textureId == binding.texture)
      frame.batches.back().elementCount += 6;
    else {
      if (frame.batches.size() >= 65536)
        return std::nullopt;
      frame.batches.push_back({0, 0, camera.width, camera.height, binding.texture, 6, offset, 0});
    }
    ++frame.statistics.visible;
    frame.statistics.coveredPixels +=
        (std::min(maximumX, double(camera.width)) - std::max(minimumX, 0.0)) *
        (std::min(maximumY, double(camera.height)) - std::max(minimumY, 0.0));
  }
  frame.statistics.batches = frame.batches.size();
  frame.statistics.vertices = frame.vertices.size();
  frame.statistics.indices = frame.indices.size();
  return frame;
}
void SpriteFrame::OffsetViewport(std::uint32_t x, std::uint32_t y) {
  for (auto &v : vertices) {
    v.position[0] += float(x);
    v.position[1] += float(y);
  }
  for (auto &v : batches) {
    v.clipX += int32_t(x);
    v.clipY += int32_t(y);
  }
}
std::vector<Nexora::Presentation::UiTextureUpload> SpriteFrame::Uploads() const {
  std::vector<Nexora::Presentation::UiTextureUpload> uploads;
  uploads.reserve(retainedAtlases.size());
  for (const auto &binding : retainedAtlases) {
    const auto &a = *binding.atlas;
    uploads.push_back({binding.texture, a.width, a.height, a.width * 4, a.rgba, a.nearest, true});
  }
  return uploads;
}
Nexora::Presentation::SurfaceStatus
SpriteFrame::Draw(Nexora::Presentation::RenderSurface &surface,
                  const Nexora::Presentation::UiDrawData &overlay, std::uint32_t background) const {
  auto uploads = Uploads();
  return surface.DrawSprites({vertices, std::as_bytes(std::span(indices)), batches, uploads, true},
                             overlay, true, background);
}
std::optional<Bytes> SpriteScene::Snapshot() const {
  auto world = world_->SaveScene(scene_, 16 * 1024 * 1024);
  if (!world)
    return std::nullopt;
  const auto *scene = world_->FindScene(scene_);
  std::set<runtime::Id> members;
  for (const auto &e : scene->entities)
    members.insert(e.id);
  detail::Writer w;
  w.U32(0x5332444e);
  w.U32(2);
  w.Text(*world);
  w.U32(static_cast<std::uint32_t>(sprites_.size()));
  for (const auto &[id, sprite] : sprites_) {
    if (!members.contains(id) || !ValidateSprite(sprite))
      return std::nullopt;
    w.U64(id);
    WriteSprite(w, sprite);
  }
  w.U32(static_cast<std::uint32_t>(colliders_.size()));
  for (const auto &[id, collider] : colliders_) {
    if (!members.contains(id) || !ValidateCollider(collider))
      return std::nullopt;
    w.U64(id);
    WriteCollider(w, collider);
  }
  return std::move(w.bytes);
}
bool SpriteScene::Restore(std::span<const std::byte> bytes) {
  auto candidate = DecodeScene(bytes);
  if (!candidate || !world_->ReplaceSceneSnapshot(scene_, candidate->world))
    return false;
  sprites_.swap(candidate->sprites);
  colliders_.swap(candidate->colliders);
  return true;
}
std::optional<SpriteScene> SpriteScene::Load(runtime::World &world,
                                             std::span<const std::byte> bytes) {
  auto candidate = DecodeScene(bytes);
  if (!candidate)
    return std::nullopt;
  auto id = world.LoadSceneSnapshot(candidate->world);
  if (!id)
    return std::nullopt;
  SpriteScene result(world, *id);
  result.sprites_ = std::move(candidate->sprites);
  result.colliders_ = std::move(candidate->colliders);
  return result;
}
} // namespace nexora::twod
