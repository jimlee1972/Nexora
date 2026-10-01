#include "Nexora/Editor/ViewportMath.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace nexora::editor;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

bool Near(double a, double b, double epsilon = 1e-9) { return std::abs(a - b) <= epsilon; }

ViewportCamera FrontCamera() {
  ViewportCamera camera;
  camera.position = {0.0, 0.0, 5.0};
  camera.target = {0.0, 0.0, 0.0};
  camera.vertical_fov_degrees = 90.0;
  return camera;
}

void TestPickRay() {
  const auto center = ViewportPickRay(FrontCamera(), 100.0, 100.0, 50.0, 50.0);
  Require(center && Near(center->direction.x, 0.0) && Near(center->direction.y, 0.0) &&
              Near(center->direction.z, -1.0) && Near(center->origin.z, 5.0),
          "the centre pixel must look straight down the view direction");

  // 90 degree FOV on a square viewport: the top-left corner is (-1, +1, -1) normalised, so the
  // image is not mirrored and +y pixels map to world down.
  const auto corner = ViewportPickRay(FrontCamera(), 100.0, 100.0, 0.0, 0.0);
  const auto inv = 1.0 / std::sqrt(3.0);
  Require(corner && Near(corner->direction.x, -inv) && Near(corner->direction.y, inv) &&
              Near(corner->direction.z, -inv),
          "the top-left pixel ray is mirrored or mis-scaled");
  const auto bottom_right = ViewportPickRay(FrontCamera(), 100.0, 100.0, 100.0, 100.0);
  Require(bottom_right && bottom_right->direction.x > 0.0 && bottom_right->direction.y < 0.0,
          "the bottom-right pixel ray points the wrong way");

  // A wider viewport widens the horizontal field, not the vertical one.
  const auto wide = ViewportPickRay(FrontCamera(), 200.0, 100.0, 0.0, 50.0);
  Require(wide && Near(wide->direction.x, -2.0 / std::sqrt(5.0)) && Near(wide->direction.y, 0.0),
          "aspect ratio did not widen the horizontal field of view");

  const auto nan = std::numeric_limits<double>::quiet_NaN();
  auto bad = FrontCamera();
  bad.target = bad.position;
  Require(!ViewportPickRay(bad, 100, 100, 50, 50), "camera looking at itself was accepted");
  bad = FrontCamera();
  bad.up = {0.0, 0.0, 1.0}; // parallel to the view direction
  Require(!ViewportPickRay(bad, 100, 100, 50, 50),
          "an up vector parallel to the view was accepted");
  bad = FrontCamera();
  bad.vertical_fov_degrees = 180.0;
  Require(!ViewportPickRay(bad, 100, 100, 50, 50), "a 180 degree field of view was accepted");
  bad = FrontCamera();
  bad.position.x = nan;
  Require(!ViewportPickRay(bad, 100, 100, 50, 50), "a NaN camera was accepted");
  Require(!ViewportPickRay(FrontCamera(), 0.0, 100.0, 0.0, 0.0), "a zero-width viewport was used");
  Require(!ViewportPickRay(FrontCamera(), 100.0, 100.0, 101.0, 50.0),
          "a pixel outside the viewport produced a ray");
  Require(!ViewportPickRay(FrontCamera(), 100.0, 100.0, nan, 50.0), "a NaN pixel produced a ray");
}

PickCandidate Box(std::uint64_t entity, double z_near, double z_far) {
  PickCandidate candidate;
  candidate.entity = entity;
  candidate.min = {-1.0, -1.0, z_near};
  candidate.max = {1.0, 1.0, z_far};
  return candidate;
}

void TestPicking() {
  const ViewportRay ray{{0.0, 0.0, 10.0}, {0.0, 0.0, -1.0}};
  std::vector<PickCandidate> candidates{Box(1, -5.0, -3.0), Box(2, 2.0, 4.0), Box(3, 6.0, 8.0)};
  auto hit = PickNearest(ray, candidates);
  Require(hit && hit->entity == 3 && Near(hit->distance, 2.0), "the nearest box was not picked");

  candidates[2].visible = false;
  hit = PickNearest(ray, candidates);
  Require(hit && hit->entity == 2 && Near(hit->distance, 6.0), "a hidden object was pickable");
  candidates[1].locked = true;
  hit = PickNearest(ray, candidates);
  Require(hit && hit->entity == 1, "a locked object was pickable");
  candidates[0].visible = false;
  Require(!PickNearest(ray, candidates), "picking with nothing eligible returned a hit");

  // Ray origin inside a box hits it immediately; boxes entirely behind the ray do not count.
  const ViewportRay inside{{0.0, 0.0, 3.0}, {0.0, 0.0, -1.0}};
  hit = PickNearest(inside, std::array{Box(7, 2.0, 4.0), Box(8, 6.0, 8.0)});
  Require(hit && hit->entity == 7 && Near(hit->distance, 0.0),
          "a ray starting inside a box must hit at distance zero");

  // Equal distances never depend on candidate order.
  hit = PickNearest(ray, std::array{Box(9, 2.0, 4.0), Box(4, 2.0, 4.0)});
  const auto swapped = PickNearest(ray, std::array{Box(4, 2.0, 4.0), Box(9, 2.0, 4.0)});
  Require(hit && swapped && hit->entity == 4 && swapped->entity == 4,
          "tied picks depend on candidate order");

  Require(!PickNearest(ray, std::array{Box(1, 2.0, 4.0)}, 5.0),
          "a hit beyond the maximum distance was returned");
  Require(PickNearest(ray, std::array{Box(1, 2.0, 4.0)}, 6.0).has_value(),
          "a hit within the maximum distance was dropped");

  // A ray parallel to a slab outside it misses; malformed boxes are ignored, not matched.
  const ViewportRay offside{{5.0, 0.0, 10.0}, {0.0, 0.0, -1.0}};
  Require(!PickNearest(offside, std::array{Box(1, 2.0, 4.0)}), "a parallel ray outside hit a box");
  auto inverted = Box(1, 2.0, 4.0);
  inverted.min.x = 2.0;
  inverted.max.x = -2.0;
  auto non_finite = Box(2, 2.0, 4.0);
  non_finite.max.y = std::numeric_limits<double>::infinity();
  Require(!PickNearest(ray, std::array{inverted, non_finite}), "a malformed box was picked");
  Require(!PickNearest(ViewportRay{{0.0, 0.0, 10.0}, {0.0, 0.0, 0.0}}, candidates),
          "a zero-direction ray picked something");
  Require(!PickNearest(ray, candidates, std::numeric_limits<double>::quiet_NaN()),
          "a NaN maximum distance picked something");

  // End to end: a pixel ray from the camera picks the box in front of it.
  const auto pixel_ray = ViewportPickRay(FrontCamera(), 100.0, 100.0, 50.0, 50.0);
  hit = PickNearest(*pixel_ray, std::array{Box(5, -1.0, 1.0)});
  Require(pixel_ray && hit && hit->entity == 5 && Near(hit->distance, 4.0),
          "pixel ray picking did not reach the box");
}

void TestAxisDrag() {
  const ViewportVector origin{0.0, 0.0, 0.0};
  const ViewportVector x_axis{1.0, 0.0, 0.0};
  const ViewportRay straight{{3.0, 0.0, 5.0}, {0.0, 0.0, -1.0}};
  auto distance = AxisDragDistance(straight, origin, x_axis);
  Require(distance && Near(*distance, 3.0), "dragging along +X measured the wrong distance");
  const ViewportRay skew{{2.0, 1.0, 5.0}, {0.0, 0.0, -1.0}};
  distance = AxisDragDistance(skew, origin, x_axis);
  Require(distance && Near(*distance, 2.0), "a skew ray did not project onto the axis");
  distance = AxisDragDistance(skew, origin, {-1.0, 0.0, 0.0});
  Require(distance && Near(*distance, -2.0), "a negated axis did not flip the sign");
  distance = AxisDragDistance(skew, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0}); // non-unit axis, shifted
  Require(distance && Near(*distance, 1.0), "axis direction was not normalised");

  Require(!AxisDragDistance(ViewportRay{{0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}}, origin, x_axis),
          "a ray parallel to the axis produced a drag distance");
  Require(!AxisDragDistance(straight, origin, {0.0, 0.0, 0.0}), "a zero axis produced a distance");
  Require(!AxisDragDistance(straight, {std::nan(""), 0.0, 0.0}, x_axis),
          "a NaN axis origin produced a distance");
}

void TestSnapping() {
  Require(Near(SnapToStep(1.26, 0.25), 1.25), "snap to the nearest step failed");
  Require(Near(SnapToStep(-0.13, 0.25), -0.25), "negative snap failed");
  Require(Near(SnapToStep(0.125, 0.25), 0.25) && Near(SnapToStep(-0.125, 0.25), -0.25),
          "halves must round away from zero symmetrically");
  Require(SnapToStep(1.26, 0.0) == 1.26 && SnapToStep(1.26, -1.0) == 1.26 &&
              SnapToStep(1.26, std::nan("")) == 1.26,
          "a disabled step must leave the value unchanged");
  Require(std::isnan(SnapToStep(std::nan(""), 0.25)), "snapping must not launder NaN");
}

void TestResizeFilter() {
  ViewportResizeFilter filter{8, 3};
  Require(filter.Update(800, 600) && filter.Width() == 800 && filter.Height() == 600,
          "the first size must be adopted");
  Require(!filter.Update(800, 600), "an unchanged size reported a resize");
  Require(!filter.Update(0, 600) && filter.Width() == 800, "a zero-area size was adopted");

  // Panel jitter inside the dead band is ignored while it keeps changing...
  Require(!filter.Update(802, 600) && !filter.Update(799, 601) && !filter.Update(803, 598) &&
              filter.Width() == 800,
          "jitter reallocated the render target");
  // ...a large change is adopted at once...
  Require(filter.Update(900, 600) && filter.Width() == 900, "a large resize was delayed");
  // ...and a small but settled change is adopted after enough stable updates.
  Require(!filter.Update(903, 600) && !filter.Update(903, 600) && filter.Update(903, 600) &&
              filter.Width() == 903,
          "a settled small resize was never adopted");
  Require(!filter.Update(903, 600), "the adopted size reported another resize");
}
} // namespace

int main() {
  try {
    TestPickRay();
    TestPicking();
    TestAxisDrag();
    TestSnapping();
    TestResizeFilter();
    std::cout << "Editor viewport math contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
