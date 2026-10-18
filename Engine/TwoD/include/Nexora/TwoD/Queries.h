#pragma once
#include "Nexora/Math/Math.h"
#include "Nexora/TwoD/Api.h"
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace nexora::twod {
struct Bounds final {
  math::Vector2 minimum, maximum;
};
enum class ShapeKind : std::uint8_t { Circle, Box };
struct Shape final {
  std::uint64_t id{};
  ShapeKind kind{ShapeKind::Circle};
  math::Vector2 center{}, halfExtent{0.5F, 0.5F};
  float radius{0.5F};
  std::uint32_t group{1}, mask{UINT32_MAX};
};
[[nodiscard]] NEXORA_2D_API bool ValidateShape(const Shape &shape) noexcept;
[[nodiscard]] NEXORA_2D_API Bounds ShapeBounds(const Shape &shape) noexcept;
[[nodiscard]] NEXORA_2D_API bool Overlap(const Shape &a, const Shape &b) noexcept;
struct SweepHit final {
  std::uint64_t id{};
  double fraction{};
};
class NEXORA_2D_API SpatialIndex final {
public:
  explicit SpatialIndex(float cellSize = 4);
  // Transactional rebuild; rejects duplicate IDs, nonfinite geometry and excessive coverage.
  bool Build(std::span<const Shape> shapes);
  [[nodiscard]] std::vector<std::uint64_t> Query(const Shape &shape) const;
  // Swept point/disc against circles and AABBs, including rounded box corners.
  [[nodiscard]] std::vector<SweepHit> Sweep(math::Vector2 from, math::Vector2 to, float radius = 0,
                                            std::uint32_t group = 1,
                                            std::uint32_t mask = UINT32_MAX) const;
  [[nodiscard]] std::size_t Size() const noexcept { return shapes_.size(); }

private:
  using Cell = std::pair<std::int32_t, std::int32_t>;
  struct CellRange {
    Cell minimum, maximum;
  };
  using CellEntry = std::pair<Cell, std::size_t>;
  [[nodiscard]] std::optional<CellRange> Cells(Bounds bounds) const;
  [[nodiscard]] std::vector<std::size_t> Candidates(Bounds bounds) const;
  float cellSize_;
  std::vector<Shape> shapes_;
  std::vector<CellEntry> cells_;
  // Reuse the previous snapshot's bounded storage without mutating the published index.
  std::vector<Shape> stagingShapes_;
  std::vector<CellEntry> stagingCells_;
};
} // namespace nexora::twod
