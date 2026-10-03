#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/Runtime/Runtime.h"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

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

// Exact ray hit on a rotated box. The center and positive half-extents are in world units;
// rotation is a unit quaternion (x,y,z,w). The returned distance is measured along the ray in
// world units, including zero when the ray begins inside the box. Invalid inputs miss.
[[nodiscard]] NEXORA_EDITOR_API std::optional<double>
PickOrientedBox(const ViewportRay &ray, const ViewportVector &center,
                const ViewportVector &half_extents, const std::array<double, 4> &rotation,
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

// ---- Rotate/scale gizmos, Global/Local axes, Pivot/Center, and parents (Unity conventions) ----
//
// A gesture is driven from its start: capture `GizmoTargets` (and `SelectionCenter`) once at Begin,
// then each frame build one `GizmoOperation` holding the whole delta since Begin and pass the
// captured targets to `ApplyGizmo`. The results are local transforms for
// `GizmoTransaction::Update`, so a gesture never accumulates rounding from frame to frame and
// Cancel restores the start exactly.

// Unity's Global/Local toggle: world axes, or the axes of the entity's world rotation.
enum class GizmoSpace { World, Local };
// Unity's Pivot/Center toggle: rotate and scale each entity about its own origin, or the whole
// selection about the mean of the selected world positions.
enum class GizmoPivot { Pivot, Center };

// The gizmo's X, Y, and Z axes (unit length). In Local space they come from the world rotation
// only, so a mirrored (negative-scale) axis is still shown in its unmirrored direction, as in
// Unity.
[[nodiscard]] NEXORA_EDITOR_API std::array<ViewportVector, 3>
GizmoAxes(const runtime::Transform &world, GizmoSpace space) noexcept;

// Signed angle in radians (right-hand rule about `axis`) swept from where `start` meets the
// rotation plane (through `center`, perpendicular to `axis`) to where `current` meets it, in (-pi,
// pi]. nullopt when a ray is (nearly) parallel to the plane, meets it behind its origin, or meets
// it too close to the centre for a direction to be defined. For turns beyond half a revolution, sum
// the per-frame angles.
[[nodiscard]] NEXORA_EDITOR_API std::optional<double>
RotationDragAngle(const ViewportRay &start, const ViewportRay &current,
                  const ViewportVector &center, const ViewportVector &axis);

// The smallest factor a scale drag produces. A drag never crosses zero, so it can neither create
// nor remove a mirror; mirroring is only ever typed into the Inspector.
inline constexpr double kMinGizmoScaleFactor = 1e-3;

// Scale-handle factor: the current handle distance along the axis divided by the distance where the
// drag started (both from `AxisDragDistance`), clamped to at least `kMinGizmoScaleFactor`. nullopt
// when the start distance is (nearly) zero or an input is not finite.
[[nodiscard]] NEXORA_EDITOR_API std::optional<double> ScaleDragFactor(double start_distance,
                                                                      double current_distance);

// One entity of a gesture, captured at Begin.
struct GizmoTarget final {
  runtime::Id entity{};
  runtime::Transform local{};        // the entity's local transform
  runtime::Transform parent_world{}; // its parent's world transform; identity for a root
};

struct GizmoOperation final {
  enum class Kind { Translate, Rotate, Scale };
  Kind kind{Kind::Translate};
  GizmoPivot pivot{GizmoPivot::Pivot};
  ViewportVector center{}; // the selection centre; used only with GizmoPivot::Center
  // Translate: world-space offset.
  ViewportVector translation{};
  // Rotate: world-space axis (any non-zero length) and angle in radians.
  ViewportVector axis{0.0, 1.0, 0.0};
  double angle{};
  // Scale: factors along the gizmo's three axes (all equal for the uniform handle); each must be
  // finite and positive. They multiply each entity's local scale, as Unity's scale tool does, so
  // they are exact for the uniform handle and for entities aligned with `axes`.
  ViewportVector factors{1.0, 1.0, 1.0};
  // Scale with Center: the gizmo's axes (orthogonal, as `GizmoAxes` returns), along which the
  // offsets from the centre are scaled.
  std::array<ViewportVector, 3> axes{{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
};

// The new local transform of every target, in order. Translation changes only local positions;
// rotation changes local rotations, and with Center also positions; scaling changes local scales,
// and with Center also positions. Results go back through each target's own parent, so children of
// rotated or scaled parents move as dragged in world space. nullopt (reject the frame, keep the
// previous one) when the operation is malformed or any result would be an invalid transform.
[[nodiscard]] NEXORA_EDITOR_API std::optional<std::vector<runtime::Transform>>
ApplyGizmo(std::span<const GizmoTarget> targets, const GizmoOperation &operation);

// The entities a gizmo should move: the selection without duplicates, missing entities, and any
// entity whose ancestor is also selected (it already moves with that ancestor), in selection order.
[[nodiscard]] NEXORA_EDITOR_API std::vector<runtime::Id>
GizmoRoots(const runtime::World &world, std::span<const runtime::Id> selection);

// Targets for `entities` (normally `GizmoRoots`); nullopt if any entity is missing.
[[nodiscard]] NEXORA_EDITOR_API std::optional<std::vector<GizmoTarget>>
GizmoTargets(const runtime::World &world, std::span<const runtime::Id> entities);

// Mean world position of `entities` (Unity's Center uses bounds, which the portable core does not
// have); nullopt when empty or an entity is missing.
[[nodiscard]] NEXORA_EDITOR_API std::optional<ViewportVector>
SelectionCenter(const runtime::World &world, std::span<const runtime::Id> entities);

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
