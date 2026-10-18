#include "Nexora/TwoD/Queries.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <tuple>
namespace nexora::twod {
namespace {
bool Finite(math::Vector2 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::abs(v.x) <= 1000000 &&
         std::abs(v.y) <= 1000000;
}
bool Filters(const Shape &a, const Shape &b) {
  return (a.group & b.mask) != 0 && (b.group & a.mask) != 0;
}
double Squared(double x, double y) { return x * x + y * y; }
std::optional<double> Disc(math::Vector2 from, math::Vector2 to, double centerX, double centerY,
                           double radius) {
  const double x = double(from.x) - centerX, y = double(from.y) - centerY;
  const double dx = double(to.x) - from.x, dy = double(to.y) - from.y;
  const double c = Squared(x, y) - radius * radius;
  if (c <= 0)
    return 0;
  const double a = Squared(dx, dy);
  if (a == 0)
    return std::nullopt;
  const double b = x * dx + y * dy, discriminant = b * b - a * c;
  if (discriminant < 0)
    return std::nullopt;
  const double t = (-b - std::sqrt(discriminant)) / a;
  return t >= 0 && t <= 1 ? std::optional(t) : std::nullopt;
}
struct PointD {
  double x, y;
};
struct BoundsD {
  PointD minimum, maximum;
};
std::optional<double> Rectangle(math::Vector2 from, math::Vector2 to, BoundsD bounds) {
  double low = 0, high = 1;
  const double origin[]{from.x, from.y}, delta[]{double(to.x) - from.x, double(to.y) - from.y};
  const double minimum[]{bounds.minimum.x, bounds.minimum.y},
      maximum[]{bounds.maximum.x, bounds.maximum.y};
  for (unsigned i = 0; i < 2; ++i) {
    if (delta[i] == 0) {
      if (origin[i] < minimum[i] || origin[i] > maximum[i])
        return std::nullopt;
      continue;
    }
    double a = (minimum[i] - origin[i]) / delta[i], b = (maximum[i] - origin[i]) / delta[i];
    if (a > b)
      std::swap(a, b);
    low = std::max(low, a);
    high = std::min(high, b);
    if (low > high)
      return std::nullopt;
  }
  return low;
}
std::optional<double> RoundedBox(math::Vector2 from, math::Vector2 to, BoundsD b, double radius) {
  auto result = Rectangle(
      from, to, {{b.minimum.x - radius, b.minimum.y}, {b.maximum.x + radius, b.maximum.y}});
  const auto keep = [&](std::optional<double> hit) {
    if (hit && (!result || *hit < *result))
      result = hit;
  };
  keep(Rectangle(from, to,
                 {{b.minimum.x, b.minimum.y - radius}, {b.maximum.x, b.maximum.y + radius}}));
  for (const double x : {b.minimum.x, b.maximum.x})
    for (const double y : {b.minimum.y, b.maximum.y})
      keep(Disc(from, to, x, y, radius));
  return result;
}
} // namespace
bool ValidateShape(const Shape &s) noexcept {
  return Finite(s.center) && std::isfinite(s.radius) && s.radius >= 0 && s.radius <= 1000000 &&
         Finite(s.halfExtent) && s.halfExtent.x >= 0 && s.halfExtent.y >= 0 &&
         (s.kind == ShapeKind::Circle || s.kind == ShapeKind::Box);
}
Bounds ShapeBounds(const Shape &s) noexcept {
  const math::Vector2 extent =
      s.kind == ShapeKind::Circle ? math::Vector2{s.radius, s.radius} : s.halfExtent;
  const auto lower = [](double value) {
    return std::nextafter(static_cast<float>(value), -std::numeric_limits<float>::infinity());
  };
  const auto upper = [](double value) {
    return std::nextafter(static_cast<float>(value), std::numeric_limits<float>::infinity());
  };
  return {{lower(double(s.center.x) - extent.x), lower(double(s.center.y) - extent.y)},
          {upper(double(s.center.x) + extent.x), upper(double(s.center.y) + extent.y)}};
}
bool Overlap(const Shape &a, const Shape &b) noexcept {
  if (!ValidateShape(a) || !ValidateShape(b) || !Filters(a, b))
    return false;
  if (a.kind == ShapeKind::Circle && b.kind == ShapeKind::Circle) {
    const double radius = double(a.radius) + b.radius;
    return Squared(double(a.center.x) - b.center.x, double(a.center.y) - b.center.y) <=
           radius * radius;
  }
  if (a.kind == ShapeKind::Box && b.kind == ShapeKind::Box)
    return std::abs(double(a.center.x) - b.center.x) <= double(a.halfExtent.x) + b.halfExtent.x &&
           std::abs(double(a.center.y) - b.center.y) <= double(a.halfExtent.y) + b.halfExtent.y;
  const auto &circle = a.kind == ShapeKind::Circle ? a : b;
  const auto &box = a.kind == ShapeKind::Box ? a : b;
  const double x =
      std::max(0.0, std::abs(double(circle.center.x) - box.center.x) - box.halfExtent.x);
  const double y =
      std::max(0.0, std::abs(double(circle.center.y) - box.center.y) - box.halfExtent.y);
  return Squared(x, y) <= double(circle.radius) * circle.radius;
}
SpatialIndex::SpatialIndex(float cellSize) : cellSize_(cellSize) {
  if (!std::isfinite(cellSize) || cellSize < 0.001F || cellSize > 1000000)
    throw std::invalid_argument("2D grid cell size");
}
std::optional<SpatialIndex::CellRange> SpatialIndex::Cells(Bounds b) const {
  const auto x0 = std::floor(double(b.minimum.x) / cellSize_),
             x1 = std::floor(double(b.maximum.x) / cellSize_);
  const auto y0 = std::floor(double(b.minimum.y) / cellSize_),
             y1 = std::floor(double(b.maximum.y) / cellSize_);
  if (!std::isfinite(x0) || !std::isfinite(x1) || !std::isfinite(y0) || !std::isfinite(y1) ||
      x0 < INT32_MIN || x1 > INT32_MAX || y0 < INT32_MIN || y1 > INT32_MAX || x1 < x0 || y1 < y0 ||
      (x1 - x0 + 1) * (y1 - y0 + 1) > 4096)
    return std::nullopt;
  return CellRange{{static_cast<std::int32_t>(x0), static_cast<std::int32_t>(y0)},
                   {static_cast<std::int32_t>(x1), static_cast<std::int32_t>(y1)}};
}
bool SpatialIndex::Build(std::span<const Shape> input) {
  if (input.size() > 100000)
    return false;
  auto &shapes = stagingShapes_;
  auto &cells = stagingCells_;
  if (input.size() > shapes.capacity())
    shapes.reserve(input.size());
  shapes.assign(input.begin(), input.end());
  const auto byId = [](const Shape &a, const Shape &b) { return a.id < b.id; };
  if (!std::is_sorted(shapes.begin(), shapes.end(), byId))
    std::sort(shapes.begin(), shapes.end(), byId);
  cells.clear();
  std::size_t coverage = 0;
  std::uint64_t previous = 0;
  for (std::size_t i = 0; i < shapes.size(); ++i) {
    const auto &s = shapes[i];
    if (!s.id || s.id == previous || !ValidateShape(s))
      return false;
    previous = s.id;
    auto keys = Cells(ShapeBounds(s));
    if (!keys)
      return false;
    const auto count = std::size_t(std::int64_t(keys->maximum.first) - keys->minimum.first + 1) *
                       std::size_t(std::int64_t(keys->maximum.second) - keys->minimum.second + 1);
    if (count > 1000000 - coverage)
      return false;
    coverage += count;
    if (coverage > cells.capacity())
      cells.reserve(std::min(std::size_t{1000000}, std::max(coverage, cells.capacity() * 2)));
    for (std::int64_t y = keys->minimum.second; y <= keys->maximum.second; ++y)
      for (std::int64_t x = keys->minimum.first; x <= keys->maximum.first; ++x)
        cells.push_back({{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)}, i});
  }
  std::sort(cells.begin(), cells.end());
  shapes_.swap(shapes);
  cells_.swap(cells);
  return true;
}
std::vector<std::size_t> SpatialIndex::Candidates(Bounds bounds) const {
  auto keys = Cells(bounds);
  std::vector<std::size_t> result;
  // Very large queries use a bounded complete scan instead of silently losing coverage.
  if (!keys) {
    result.resize(shapes_.size());
    std::iota(result.begin(), result.end(), 0);
    return result;
  }
  for (std::int64_t y = keys->minimum.second; y <= keys->maximum.second; ++y)
    for (std::int64_t x = keys->minimum.first; x <= keys->maximum.first; ++x) {
      const Cell key{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
      auto it = std::lower_bound(
          cells_.begin(), cells_.end(), key,
          [](const CellEntry &entry, const Cell &cell) { return entry.first < cell; });
      for (; it != cells_.end() && it->first == key; ++it)
        result.push_back(it->second);
    }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}
std::vector<std::uint64_t> SpatialIndex::Query(const Shape &shape) const {
  std::vector<std::uint64_t> result;
  if (!ValidateShape(shape))
    return result;
  for (const auto i : Candidates(ShapeBounds(shape)))
    if (shape.id != shapes_[i].id && Overlap(shape, shapes_[i]))
      result.push_back(shapes_[i].id);
  return result;
}
std::vector<SweepHit> SpatialIndex::Sweep(math::Vector2 from, math::Vector2 to, float radius,
                                          std::uint32_t group, std::uint32_t mask) const {
  std::vector<SweepHit> result;
  if (!Finite(from) || !Finite(to) || !std::isfinite(radius) || radius < 0 || radius > 1000000)
    return result;
  Shape query;
  query.group = group;
  query.mask = mask;
  const Bounds bounds{{std::min(from.x, to.x) - radius, std::min(from.y, to.y) - radius},
                      {std::max(from.x, to.x) + radius, std::max(from.y, to.y) + radius}};
  for (const auto i : Candidates(bounds)) {
    const auto &s = shapes_[i];
    if (!Filters(query, s))
      continue;
    auto hit =
        s.kind == ShapeKind::Circle
            ? Disc(from, to, s.center.x, s.center.y, double(radius) + s.radius)
            : RoundedBox(
                  from, to,
                  {{double(s.center.x) - s.halfExtent.x, double(s.center.y) - s.halfExtent.y},
                   {double(s.center.x) + s.halfExtent.x, double(s.center.y) + s.halfExtent.y}},
                  radius);
    if (hit)
      result.push_back({s.id, *hit});
  }
  std::sort(result.begin(), result.end(), [](const SweepHit &a, const SweepHit &b) {
    return std::tie(a.fraction, a.id) < std::tie(b.fraction, b.id);
  });
  return result;
}
} // namespace nexora::twod
