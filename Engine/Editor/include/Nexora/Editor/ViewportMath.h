#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/Runtime/Runtime.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace nexora::editor {

// UI-neutral Scene View math for picking and gizmo drags. It uses double precision to match
// `runtime::Transform`, performs no rendering, and never reads GPU data.
struct ViewportVector final {
  double x{}, y{}, z{};
};

struct ViewportCamera final {
  ViewportVector position{};
  ViewportVector target{0.0, 0.0, -1.0};
  ViewportVector up{0.0, 1.0, 0.0};
  double vertical_fov_degrees{60.0};
};

struct ViewportRay final {
  ViewportVector origin{};
  ViewportVector direction{}; // unit length
};

// Builds the world-space ray through a pixel (origin top-left, +y down). Returns nullopt for a
// degenerate camera (non-finite values, position == target, `up` parallel to the view direction,
// FOV outside (0, 180)) or viewport, or a pixel outside the viewport.
[[nodiscard]] NEXORA_EDITOR_API std::optional<ViewportRay>
ViewportPickRay(const ViewportCamera &camera, double viewport_width, double viewport_height,
                double pixel_x, double pixel_y);

struct PickCandidate final {
  runtime::Id entity{};
  ViewportVector min{}, max{};
  bool visible{true};
  bool locked{false};
};

struct PickHit final {
  runtime::Id entity{};
  double distance{};
};

// Nearest-hit picking over world-space AABBs. Hidden and locked candidates and malformed boxes
// (non-finite, or min > max) are ignored. A ray starting inside a box hits it at distance 0. Equal
// distances resolve to the lowest entity id so the result never depends on candidate order.
[[nodiscard]] NEXORA_EDITOR_API std::optional<PickHit>
PickNearest(const ViewportRay &ray, std::span<const PickCandidate> candidates,
            double max_distance = std::numeric_limits<double>::infinity());

// Signed distance along `axis_direction` (from `axis_origin`) of the point on the gizmo axis line
// closest to the ray. Returns nullopt when the ray is parallel to the axis or an input is
// degenerate, so a drag has no defined motion rather than a huge or NaN one.
[[nodiscard]] NEXORA_EDITOR_API std::optional<double>
AxisDragDistance(const ViewportRay &ray, const ViewportVector &axis_origin,
                 const ViewportVector &axis_direction);

// Rounds to the nearest multiple of `step` (halves away from zero). A non-positive or non-finite
// step disables snapping and returns the value unchanged.
[[nodiscard]] NEXORA_EDITOR_API double SnapToStep(double value, double step) noexcept;

// Scene View render-target resize filter. Small jitter (a docked panel settling) must not
// reallocate GPU targets every frame: a new size is adopted only once it differs from the current
// target by at least `hysteresis_pixels` on an axis, or has been requested unchanged for
// `stable_frames` consecutive updates. Zero-area requests are ignored.
class NEXORA_EDITOR_API ViewportResizeFilter final {
public:
  ViewportResizeFilter(std::uint32_t hysteresis_pixels, std::uint32_t stable_frames) noexcept
      : hysteresis_(hysteresis_pixels), stable_frames_(stable_frames) {}
  // Returns true when the target size changed and the caller should recreate its render target.
  bool Update(std::uint32_t width, std::uint32_t height) noexcept;
  [[nodiscard]] std::uint32_t Width() const noexcept { return width_; }
  [[nodiscard]] std::uint32_t Height() const noexcept { return height_; }

private:
  std::uint32_t hysteresis_{};
  std::uint32_t stable_frames_{};
  std::uint32_t width_{}, height_{};
  std::uint32_t pending_width_{}, pending_height_{};
  std::uint32_t pending_count_{};
};

} // namespace nexora::editor
