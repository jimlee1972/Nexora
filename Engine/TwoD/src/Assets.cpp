#include "Nexora/TwoD/Assets.h"
#include "Wire.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace nexora::twod {
namespace {
constexpr std::size_t maximumPixels = 4 * 1024 * 1024;
bool Extent(std::uint32_t w, std::uint32_t h) {
  return w > 0 && h > 0 && w <= 4096 && h <= 4096 && std::uint64_t(w) * h <= maximumPixels;
}
} // namespace
const Region *Atlas::Find(std::uint32_t region) const noexcept {
  const auto it = std::lower_bound(regions.begin(), regions.end(), region,
                                   [](const Region &a, std::uint32_t b) { return a.id < b; });
  return it != regions.end() && it->id == region ? &*it : nullptr;
}
bool ValidateAtlas(const Atlas &a) noexcept {
  if (!detail::ValidId(a.id) || !Extent(a.width, a.height) || a.gutter != 1 || a.regions.empty() ||
      a.regions.size() > 1024 || a.rgba.size() != std::size_t(a.width) * a.height * 4)
    return false;
  std::uint32_t previous = 0;
  for (std::size_t i = 0; i < a.regions.size(); ++i) {
    const auto &r = a.regions[i];
    if (r.id <= previous || !r.width || !r.height || r.x < 1 || r.y < 1 || r.x >= a.width ||
        r.y >= a.height || r.width >= a.width - r.x || r.height >= a.height - r.y)
      return false;
    previous = r.id;
    for (std::size_t j = 0; j < i; ++j) {
      const auto &q = a.regions[j];
      if (r.x - 1 < q.x + q.width + 1 && q.x - 1 < r.x + r.width + 1 &&
          r.y - 1 < q.y + q.height + 1 && q.y - 1 < r.y + r.height + 1)
        return false;
    }
  }
  return true;
}
std::optional<Atlas> PackAtlas(AssetId id, std::span<const Image> input, std::uint32_t width,
                               std::uint32_t maximumHeight, bool nearest) {
  if (!detail::ValidId(id) || input.empty() || input.size() > 1024 || width < 3 || width > 4096 ||
      maximumHeight < 3 || maximumHeight > 4096)
    return std::nullopt;
  std::vector<const Image *> images;
  images.reserve(input.size());
  std::size_t inputBytes = 0;
  for (const auto &image : input) {
    if (!image.id || !Extent(image.width, image.height) || image.width > width - 2 ||
        image.height > maximumHeight - 2 ||
        image.rgba.size() != std::size_t(image.width) * image.height * 4 ||
        image.rgba.size() > maximumPixels * 4 - inputBytes)
      return std::nullopt;
    inputBytes += image.rgba.size();
    images.push_back(&image);
  }
  std::sort(images.begin(), images.end(),
            [](const Image *a, const Image *b) { return a->id < b->id; });
  Atlas atlas{id, width, 0, 1, nearest, {}, {}};
  std::uint32_t x = 0, y = 0, rowHeight = 0, previous = 0;
  for (const auto *image : images) {
    if (image->id == previous)
      return std::nullopt;
    previous = image->id;
    if (x + image->width + 2 > width) {
      y += rowHeight;
      x = 0;
      rowHeight = 0;
    }
    const auto h = image->height + 2;
    if (h > maximumHeight - y)
      return std::nullopt;
    atlas.regions.push_back({image->id, x + 1, y + 1, image->width, image->height});
    x += image->width + 2;
    rowHeight = std::max(rowHeight, h);
  }
  atlas.height = y + rowHeight;
  if (!Extent(width, atlas.height))
    return std::nullopt;
  atlas.rgba.resize(std::size_t(width) * atlas.height * 4);
  for (std::size_t i = 0; i < images.size(); ++i) {
    const auto &image = *images[i];
    const auto &r = atlas.regions[i];
    for (std::uint32_t dy = 0; dy < r.height + 2; ++dy)
      for (std::uint32_t dx = 0; dx < r.width + 2; ++dx) {
        const auto sx = std::clamp<std::int64_t>(std::int64_t(dx) - 1, 0, r.width - 1);
        const auto sy = std::clamp<std::int64_t>(std::int64_t(dy) - 1, 0, r.height - 1);
        const auto source = (std::size_t(sy) * r.width + std::size_t(sx)) * 4;
        const auto target = (std::size_t(r.y - 1 + dy) * width + r.x - 1 + dx) * 4;
        std::copy_n(image.rgba.begin() + static_cast<std::ptrdiff_t>(source), 4,
                    atlas.rgba.begin() + static_cast<std::ptrdiff_t>(target));
      }
  }
  return atlas;
}
Bytes EncodeAtlas(const Atlas &atlas) {
  if (!ValidateAtlas(atlas))
    return {};
  detail::Writer w;
  w.U32(0x4132444e);
  w.U32(1);
  w.Id(atlas.id);
  w.U32(atlas.width);
  w.U32(atlas.height);
  w.U32(atlas.gutter);
  w.U32(atlas.nearest ? 1U : 0U);
  w.U32(static_cast<std::uint32_t>(atlas.regions.size()));
  for (const auto &r : atlas.regions) {
    w.U32(r.id);
    w.U32(r.x);
    w.U32(r.y);
    w.U32(r.width);
    w.U32(r.height);
  }
  w.Data(atlas.rgba);
  return std::move(w.bytes);
}
std::optional<Atlas> DecodeAtlas(std::span<const std::byte> bytes) {
  detail::Reader r(bytes);
  if (r.U32() != 0x4132444e || r.U32() != 1)
    return std::nullopt;
  Atlas a;
  a.id = r.Id();
  a.width = r.U32();
  a.height = r.U32();
  a.gutter = r.U32();
  const auto nearest = r.U32(), count = r.U32();
  if (nearest > 1 || count == 0 || count > 1024)
    return std::nullopt;
  a.nearest = nearest != 0;
  a.regions.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    Region region;
    region.id = r.U32();
    region.x = r.U32();
    region.y = r.U32();
    region.width = r.U32();
    region.height = r.U32();
    a.regions.push_back(region);
  }
  const auto pixels = r.Data(maximumPixels * 4);
  a.rgba.assign(pixels.begin(), pixels.end());
  if (!r.Done() || !ValidateAtlas(a))
    return std::nullopt;
  return a;
}
bool ValidateClip(const Clip &clip) noexcept {
  if (!detail::ValidId(clip.id) || !detail::ValidId(clip.atlas) || clip.frames.empty() ||
      clip.frames.size() > 4096)
    return false;
  double total = 0;
  for (const auto &f : clip.frames) {
    if (!f.region || !std::isfinite(f.seconds) || f.seconds < 0.000001 || f.seconds > 3600 ||
        f.event.size() > 256)
      return false;
    for (unsigned char c : f.event)
      if (c < 32 || c > 126)
        return false;
    total += f.seconds;
  }
  return std::isfinite(total);
}
Bytes EncodeClip(const Clip &clip) {
  if (!ValidateClip(clip))
    return {};
  detail::Writer w;
  w.U32(0x4332444e);
  w.U32(1);
  w.Id(clip.id);
  w.Id(clip.atlas);
  w.U32(clip.loop ? 1U : 0U);
  w.U32(static_cast<std::uint32_t>(clip.frames.size()));
  for (const auto &f : clip.frames) {
    w.U32(f.region);
    w.Double(f.seconds);
    w.Text(f.event);
  }
  return std::move(w.bytes);
}
std::optional<Clip> DecodeClip(std::span<const std::byte> bytes) {
  detail::Reader r(bytes);
  if (r.U32() != 0x4332444e || r.U32() != 1)
    return std::nullopt;
  Clip c;
  c.id = r.Id();
  c.atlas = r.Id();
  const auto loop = r.U32(), count = r.U32();
  if (loop > 1 || count == 0 || count > 4096)
    return std::nullopt;
  c.loop = loop != 0;
  c.frames.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    AnimationFrame f;
    f.region = r.U32();
    f.seconds = r.Double();
    f.event = r.Text();
    c.frames.push_back(std::move(f));
  }
  if (!r.Done() || !ValidateClip(c))
    return std::nullopt;
  return c;
}
bool RegisterImporters(runtime::ImporterRegistry &registry) {
  auto candidate = registry;
  const bool a = candidate.Register(
      ".nx2atlas",
      [](const runtime::SourceAsset &source) -> std::optional<runtime::CanonicalAsset> {
        auto atlas = DecodeAtlas(source.bytes);
        if (!atlas || atlas->id != source.id)
          return std::nullopt;
        return runtime::CanonicalAsset{source.id, "sprite_atlas", {}, EncodeAtlas(*atlas)};
      });
  const bool c = candidate.Register(
      ".nx2clip", [](const runtime::SourceAsset &source) -> std::optional<runtime::CanonicalAsset> {
        auto clip = DecodeClip(source.bytes);
        if (!clip || clip->id != source.id)
          return std::nullopt;
        return runtime::CanonicalAsset{source.id, "sprite_clip", {clip->atlas}, EncodeClip(*clip)};
      });
  const bool s = candidate.Register(
      ".nx2sprite",
      [](const runtime::SourceAsset &source) -> std::optional<runtime::CanonicalAsset> {
        auto sprite = DecodeSpriteAsset(source.bytes);
        if (!sprite || sprite->id != source.id)
          return std::nullopt;
        return runtime::CanonicalAsset{
            source.id, "sprite_source", {sprite->atlas}, EncodeSpriteAsset(*sprite)};
      });
  if (!a || !c || !s)
    return false;
  registry = std::move(candidate);
  return true;
}
bool Animator::Play(const Clip &clip, double speed) {
  if (!ValidateClip(clip) || !std::isfinite(speed) || speed <= 0 || speed > 1000)
    return false;
  auto copy = clip;
  clip_ = std::move(copy);
  speed_ = speed;
  frame_ = 0;
  elapsed_ = 0;
  ready_ = true;
  paused_ = false;
  finished_ = false;
  return true;
}
std::optional<std::uint32_t> Animator::RegionId() const noexcept {
  return ready_ ? std::optional(clip_.frames[frame_].region) : std::nullopt;
}
std::optional<AnimationUpdate> Animator::Advance(double seconds) {
  if (!ready_ || !std::isfinite(seconds) || seconds < 0 || !std::isfinite(seconds * speed_))
    return std::nullopt;
  AnimationUpdate result{clip_.frames[frame_].region, {}, finished_};
  if (paused_ || finished_)
    return result;
  double elapsed = elapsed_ + seconds * speed_;
  if (!std::isfinite(elapsed))
    return std::nullopt;
  auto frame = frame_;
  bool finished = false;
  std::size_t transitions = 0;
  // A relative boundary tolerance keeps equivalent frame-time partitions on the same frame.
  while (elapsed + clip_.frames[frame].seconds * 1e-12 >= clip_.frames[frame].seconds) {
    if (++transitions > 65536)
      return std::nullopt;
    elapsed = std::max(0.0, elapsed - clip_.frames[frame].seconds);
    if (frame + 1 == clip_.frames.size() && !clip_.loop) {
      elapsed = 0;
      finished = true;
      break;
    }
    frame = (frame + 1) % clip_.frames.size();
    if (!clip_.frames[frame].event.empty())
      result.events.push_back(clip_.frames[frame].event);
  }
  frame_ = frame;
  elapsed_ = elapsed;
  finished_ = finished;
  result.region = clip_.frames[frame].region;
  result.finished = finished;
  return result;
}
} // namespace nexora::twod
