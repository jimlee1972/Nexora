#include "Nexora/Runtime/PresentationV2.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <ranges>
#include <unordered_set>

namespace nexora::runtime::presentation_v2 {
namespace {
bool Finite(float value) { return std::isfinite(value); }
bool Finite(double value) { return std::isfinite(value); }
bool ValidRect(Rect rect) {
  return Finite(rect.x) && Finite(rect.y) && Finite(rect.width) && Finite(rect.height) &&
         rect.width >= 0.0F && rect.height >= 0.0F;
}
presentation::Vec3 Subtract(presentation::Vec3 a, presentation::Vec3 b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
presentation::Vec3 Cross(presentation::Vec3 a, presentation::Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float Dot(presentation::Vec3 a, presentation::Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
Style MergeStyle(Style base, const Style &overlay) {
  if (overlay.opacity)
    base.opacity = overlay.opacity;
  if (overlay.font_scale)
    base.font_scale = overlay.font_scale;
  if (overlay.color_token)
    base.color_token = overlay.color_token;
  return base;
}
} // namespace

Timeline::Timeline(double duration) noexcept { SetDuration(duration); }
bool Timeline::SetDuration(double duration) noexcept {
  if (!Finite(duration) || duration < 0.0 ||
      std::ranges::any_of(tracks_, [duration](const auto &track) {
        return !track.keys.empty() && track.keys.back().time > duration;
      }))
    return false;
  duration_ = duration;
  time_ = std::min(time_, duration_);
  return true;
}
bool Timeline::AddTrack(TimelineTrack track) {
  if (track.binding.empty() || track.keys.empty() ||
      std::ranges::any_of(tracks_, [&](const auto &item) { return item.binding == track.binding; }))
    return false;
  double previous = -1.0;
  for (const auto &key : track.keys) {
    if (!Finite(key.time) || !Finite(key.value) || key.time < 0.0 || key.time > duration_ ||
        key.time <= previous)
      return false;
    previous = key.time;
  }
  tracks_.push_back(std::move(track));
  return true;
}
bool Timeline::Seek(double time) noexcept {
  if (!Finite(time))
    return false;
  time_ = std::clamp(time, 0.0, duration_);
  return true;
}
std::optional<float> Timeline::Evaluate(std::string_view binding) const noexcept {
  const auto found = std::ranges::find(tracks_, binding, &TimelineTrack::binding);
  if (found == tracks_.end())
    return std::nullopt;
  const auto upper = std::ranges::upper_bound(found->keys, time_, {}, &TimelineKey::time);
  if (upper == found->keys.begin())
    return upper->value;
  if (upper == found->keys.end())
    return found->keys.back().value;
  const auto &right = *upper;
  const auto &left = *(upper - 1);
  const auto alpha = static_cast<float>((time_ - left.time) / (right.time - left.time));
  return left.value + (right.value - left.value) * alpha;
}

bool CameraRig::SetLayers(std::vector<CameraRigLayer> layers) {
  if (layers.empty())
    return false;
  for (const auto &layer : layers)
    if (!Finite(layer.weight) || layer.weight < 0.0F || !Finite(layer.state.yaw) ||
        !Finite(layer.state.pitch) || !Finite(layer.state.distance) || layer.state.distance <= 0.0F ||
        !Finite(layer.state.field_of_view) || layer.state.field_of_view <= 0.0F ||
        layer.state.field_of_view >= 180.0F)
      return false;
  std::ranges::stable_sort(layers, {}, &CameraRigLayer::priority);
  layers_ = std::move(layers);
  return true;
}
std::optional<CameraState> CameraRig::Resolve() const noexcept {
  if (layers_.empty())
    return std::nullopt;
  const auto highest = layers_.back().priority;
  float total = 0.0F;
  CameraState result{0.0F, 0.0F, 0.0F, 0.0F};
  for (const auto &layer : layers_) {
    if (layer.priority != highest || layer.weight <= 0.0F)
      continue;
    total += layer.weight;
    result.yaw += layer.state.yaw * layer.weight;
    result.pitch += layer.state.pitch * layer.weight;
    result.distance += layer.state.distance * layer.weight;
    result.field_of_view += layer.state.field_of_view * layer.weight;
  }
  if (total <= 0.0F)
    return std::nullopt;
  result.yaw /= total;
  result.pitch /= total;
  result.distance /= total;
  result.field_of_view /= total;
  return result;
}

std::vector<LayoutBox> ResolveFlex(Rect container, std::span<const FlexItem> items,
                                   FlexDirection direction, float gap) {
  if (!ValidRect(container) || !Finite(gap) || gap < 0.0F || items.empty())
    return {};
  const float main_size = direction == FlexDirection::Row ? container.width : container.height;
  float basis_sum = 0.0F;
  float grow_sum = 0.0F;
  std::unordered_set<UIElementId> ids;
  for (const auto &item : items) {
    if (item.id == 0 || !ids.insert(item.id).second || !Finite(item.basis) || item.basis < 0.0F ||
        !Finite(item.grow) || item.grow < 0.0F)
      return {};
    basis_sum += item.basis;
    grow_sum += item.grow;
  }
  const float gap_sum = gap * static_cast<float>(items.size() - 1);
  if (gap_sum > main_size)
    return {};
  const float available = main_size - gap_sum;
  const float shrink = basis_sum > available && basis_sum > 0.0F ? available / basis_sum : 1.0F;
  const float extra = std::max(0.0F, available - basis_sum);
  float cursor = direction == FlexDirection::Row ? container.x : container.y;
  std::vector<LayoutBox> result;
  result.reserve(items.size());
  for (const auto &item : items) {
    const float main =
        item.basis * shrink + (grow_sum > 0.0F ? extra * item.grow / grow_sum : 0.0F);
    Rect rect = direction == FlexDirection::Row
                    ? Rect{cursor, container.y, main, container.height}
                    : Rect{container.x, cursor, container.width, main};
    result.push_back({item.id, rect});
    cursor += main + gap;
  }
  return result;
}

std::vector<LayoutBox> ResolveGrid(Rect container, std::size_t columns, std::size_t rows,
                                   std::span<const GridItem> items, float column_gap,
                                   float row_gap) {
  if (!ValidRect(container) || columns == 0 || rows == 0 || !Finite(column_gap) ||
      !Finite(row_gap) || column_gap < 0.0F || row_gap < 0.0F)
    return {};
  const float usable_width = container.width - column_gap * static_cast<float>(columns - 1);
  const float usable_height = container.height - row_gap * static_cast<float>(rows - 1);
  if (usable_width < 0.0F || usable_height < 0.0F)
    return {};
  const float cell_width = usable_width / static_cast<float>(columns);
  const float cell_height = usable_height / static_cast<float>(rows);
  std::vector<LayoutBox> result;
  result.reserve(items.size());
  std::unordered_set<UIElementId> ids;
  for (const auto &item : items) {
    if (item.id == 0 || !ids.insert(item.id).second || item.column >= columns || item.row >= rows ||
        item.column_span == 0 || item.row_span == 0 || item.column + item.column_span > columns ||
        item.row + item.row_span > rows)
      return {};
    result.push_back({item.id,
                      {container.x + static_cast<float>(item.column) * (cell_width + column_gap),
                       container.y + static_cast<float>(item.row) * (cell_height + row_gap),
                       cell_width * static_cast<float>(item.column_span) +
                           column_gap * static_cast<float>(item.column_span - 1),
                       cell_height * static_cast<float>(item.row_span) +
                           row_gap * static_cast<float>(item.row_span - 1)}});
  }
  return result;
}

void RichText::Add(RichTextSpan span) { spans_.push_back(std::move(span)); }
std::string RichText::Resolve(const LocalizationTable &localization) const {
  std::string result;
  for (const auto &span : spans_)
    result += span.localization_key ? localization.Resolve(span.value) : span.value;
  return result;
}

bool Theme::SetColor(std::string token, std::string value) {
  if (token.empty() || value.empty())
    return false;
  colors_[std::move(token)] = std::move(value);
  return true;
}
std::string Theme::ResolveColor(std::string_view token) const {
  const auto found = colors_.find(std::string(token));
  return found == colors_.end() ? std::string{} : found->second;
}
bool StyleSheet::SetRule(std::string style_class, Style style) {
  if (style_class.empty())
    return false;
  if ((style.opacity && (!Finite(*style.opacity) || *style.opacity < 0.0F || *style.opacity > 1.0F)) ||
      (style.font_scale && (!Finite(*style.font_scale) || *style.font_scale <= 0.0F)))
    return false;
  rules_[std::move(style_class)] = std::move(style);
  return true;
}
ResolvedStyle StyleSheet::Resolve(std::span<const std::string> classes, const Style &inline_style,
                                  const Theme &theme) const {
  Style merged;
  for (const auto &name : classes) {
    const auto found = rules_.find(name);
    if (found != rules_.end())
      merged = MergeStyle(std::move(merged), found->second);
  }
  merged = MergeStyle(std::move(merged), inline_style);
  ResolvedStyle result;
  if (merged.opacity)
    result.opacity = *merged.opacity;
  if (merged.font_scale)
    result.font_scale = *merged.font_scale;
  if (merged.color_token)
    result.color = theme.ResolveColor(*merged.color_token);
  return result;
}

bool AccessibilityTree::SetNodes(std::vector<AccessibilityNode> nodes) {
  std::unordered_map<UIElementId, UIElementId> parent;
  std::unordered_set<UIElementId> ids;
  for (const auto &node : nodes) {
    if (node.id == 0 || !ids.insert(node.id).second ||
        ((node.role == AccessibilityRole::Button || node.role == AccessibilityRole::TextField) &&
         node.name.empty()))
      return false;
    parent[node.id] = node.parent;
  }
  for (const auto &node : nodes) {
    if (node.parent != 0 && !ids.contains(node.parent))
      return false;
    std::unordered_set<UIElementId> chain;
    for (UIElementId current = node.id; current != 0;) {
      if (!chain.insert(current).second)
        return false;
      const auto found = parent.find(current);
      current = found == parent.end() ? 0 : found->second;
    }
  }
  nodes_ = std::move(nodes);
  return true;
}
const AccessibilityNode *AccessibilityTree::Find(UIElementId id) const noexcept {
  const auto found = std::ranges::find(nodes_, id, &AccessibilityNode::id);
  return found == nodes_.end() ? nullptr : &*found;
}
std::vector<UIElementId> AccessibilityTree::ReadingOrder() const {
  std::vector<UIElementId> result;
  result.reserve(nodes_.size());
  for (const auto &node : nodes_)
    if (node.enabled)
      result.push_back(node.id);
  return result;
}

std::optional<SurfacePointerHit>
ProjectSurfacePointer(const SurfaceRay &ray, const SurfaceVertex &a, const SurfaceVertex &b,
                      const SurfaceVertex &c, Rect ui_rect, PointerId pointer, bool pressed) {
  if (!ValidRect(ui_rect))
    return std::nullopt;
  constexpr float epsilon = 1.0e-6F;
  const auto edge1 = Subtract(b.position, a.position);
  const auto edge2 = Subtract(c.position, a.position);
  const auto p = Cross(ray.direction, edge2);
  const float determinant = Dot(edge1, p);
  if (std::abs(determinant) < epsilon)
    return std::nullopt;
  const float inverse = 1.0F / determinant;
  const auto t = Subtract(ray.origin, a.position);
  const float bary_b = Dot(t, p) * inverse;
  if (bary_b < 0.0F || bary_b > 1.0F)
    return std::nullopt;
  const auto q = Cross(t, edge1);
  const float bary_c = Dot(ray.direction, q) * inverse;
  if (bary_c < 0.0F || bary_b + bary_c > 1.0F)
    return std::nullopt;
  const float distance = Dot(edge2, q) * inverse;
  if (distance < 0.0F)
    return std::nullopt;
  const float bary_a = 1.0F - bary_b - bary_c;
  const float u = a.u * bary_a + b.u * bary_b + c.u * bary_c;
  const float v = a.v * bary_a + b.v * bary_b + c.v * bary_c;
  if (!Finite(u) || !Finite(v) || u < 0.0F || u > 1.0F || v < 0.0F || v > 1.0F)
    return std::nullopt;
  return SurfacePointerHit{distance, u, v,
                           {pointer, ui_rect.x + u * ui_rect.width,
                            ui_rect.y + v * ui_rect.height, pressed}};
}

bool RoomAudioGraph::AddPortal(std::uint64_t room_a, std::uint64_t room_b, float transmission) {
  if (room_a == 0 || room_b == 0 || room_a == room_b || !Finite(transmission) ||
      transmission < 0.0F || transmission > 1.0F)
    return false;
  const auto duplicate = std::ranges::find_if(portals_, [&](const auto &portal) {
    return (portal.a == room_a && portal.b == room_b) || (portal.a == room_b && portal.b == room_a);
  });
  if (duplicate != portals_.end())
    return false;
  portals_.push_back({room_a, room_b, transmission});
  return true;
}
float RoomAudioGraph::Transmission(std::uint64_t source_room, std::uint64_t listener_room) const {
  if (source_room == 0 || listener_room == 0)
    return 0.0F;
  if (source_room == listener_room)
    return 1.0F;
  using Entry = std::pair<float, std::uint64_t>;
  std::priority_queue<Entry> queue;
  std::unordered_map<std::uint64_t, float> best{{source_room, 1.0F}};
  queue.push({1.0F, source_room});
  while (!queue.empty()) {
    const auto [gain, room] = queue.top();
    queue.pop();
    if (room == listener_room)
      return gain;
    if (gain < best[room])
      continue;
    for (const auto &portal : portals_) {
      std::uint64_t next = 0;
      if (portal.a == room)
        next = portal.b;
      else if (portal.b == room)
        next = portal.a;
      else
        continue;
      const float candidate = gain * portal.transmission;
      if (!best.contains(next) || candidate > best[next]) {
        best[next] = candidate;
        queue.push({candidate, next});
      }
    }
  }
  return 0.0F;
}

bool AdaptiveMediaStream::Configure(MediaProtocol protocol, std::vector<MediaSegment> segments) {
  if (segments.empty())
    return false;
  std::uint64_t previous_sequence = 0;
  double previous_end = -1.0;
  for (const auto &segment : segments) {
    if (segment.sequence == 0 || segment.sequence <= previous_sequence || !Finite(segment.start) ||
        !Finite(segment.duration) || segment.start < 0.0 || segment.duration <= 0.0 ||
        segment.start < previous_end || segment.uri.empty())
      return false;
    previous_sequence = segment.sequence;
    previous_end = segment.start + segment.duration;
  }
  protocol_ = protocol;
  segments_ = std::move(segments);
  return true;
}
bool AdaptiveMediaStream::Seek(double time, presentation::VideoPlayer &player) noexcept {
  if (!Finite(time) || time < 0.0 || !SegmentFor(time))
    return false;
  player.Seek(time);
  return true;
}
std::optional<MediaSegment> AdaptiveMediaStream::SegmentFor(double time) const {
  if (!Finite(time) || time < 0.0)
    return std::nullopt;
  const auto found = std::ranges::find_if(segments_, [&](const auto &segment) {
    return segment.start <= time && time < segment.start + segment.duration;
  });
  return found == segments_.end() ? std::nullopt : std::optional<MediaSegment>{*found};
}
bool AdaptiveMediaStream::SubmitDecoded(presentation::VideoPlayer &player,
                                        presentation::DecodedVideoFrame frame) const {
  return player.SubmitDecoded(frame);
}

} // namespace nexora::runtime::presentation_v2
