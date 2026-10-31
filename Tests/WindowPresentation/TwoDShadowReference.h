#pragma once
#include "Nexora/Presentation/TwoDLighting.h"
#include <algorithm>
#include <array>
#include <cmath>

// Independent double-precision test oracle. Production shadow maps are GPU compute output.
namespace TwoDShadowReference {
using namespace Nexora::Presentation;
class Frame final {
  std::array<std::array<double, 1024>, 8> maps_{};

public:
  Frame(const TwoDLightingUniform &u, std::span<const TwoDShadowEdge> edges) {
    constexpr double tau = 6.2831853071795864769;
    for (unsigned slot = 0; slot < u.counts[1]; ++slot) {
      const auto &light = u.lights[u.shadowLights[slot / 4][slot % 4]];
      for (unsigned sample = 0; sample < 1024; ++sample) {
        const double angle = (double(sample) + 0.5) * tau / 1024;
        const double dx = std::cos(angle), dy = std::sin(angle);
        double nearest = light.positionRadiusIntensity[2];
        for (const auto &edge : edges) {
          if (!(edge.masks[0] & light.masks[0]))
            continue;
          const double ax = double(edge.points[0]) - light.positionRadiusIntensity[0];
          const double ay = double(edge.points[1]) - light.positionRadiusIntensity[1];
          const double ex = double(edge.points[2]) - edge.points[0];
          const double ey = double(edge.points[3]) - edge.points[1];
          const double denominator = dx * ey - dy * ex;
          if (std::abs(denominator) <= 1e-7 * std::hypot(ex, ey))
            continue;
          const double distance = (ax * ey - ay * ex) / denominator;
          const double along = (ax * dy - ay * dx) / denominator;
          if (distance > light.shadow[1] && distance < nearest && along >= 0 && along <= 1)
            nearest = distance;
        }
        maps_[slot][sample] = nearest;
      }
    }
  }
  double Distance(unsigned slot, unsigned angle) const { return maps_.at(slot).at(angle); }
  double Visibility(const TwoDPointLight &light, double x, double y) const {
    if (!light.masks[1])
      return 1;
    const double dx = x - light.positionRadiusIntensity[0],
                 dy = y - light.positionRadiusIntensity[1];
    const double distance = std::hypot(dx, dy);
    if (!distance)
      return 1;
    double phase = std::atan2(dy, dx) / 6.2831853071795864769;
    phase -= std::floor(phase);
    const double position = phase * 1024 - 0.5, fraction = position - std::floor(position);
    const int center = int(std::floor(position));
    const double width = 1 + 4 * double(light.shadow[0]);
    double visible = 0, total = 0;
    for (int tap = -4; tap <= 4; ++tap) {
      const double weight = std::max(0.0, 1 - std::abs(tap) / width);
      if (!weight)
        continue;
      const auto sample = unsigned(center + tap + 2048) & 1023U, next = (sample + 1) & 1023U;
      const auto &map = maps_[light.masks[1] - 1];
      const double first = distance <= map[sample] + light.shadow[1] ? 1 : 0;
      const double second = distance <= map[next] + light.shadow[1] ? 1 : 0;
      visible += weight * ((1 - fraction) * first + fraction * second);
      total += weight;
    }
    return visible / total;
  }
};
} // namespace TwoDShadowReference
