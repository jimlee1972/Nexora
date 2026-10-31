#include "Nexora/TwoD/EffectsRendering.h"
#include <cmath>
namespace nexora::twod {
std::optional<LightingFrame> PrepareLighting(const EffectsSnapshot &effects, const Camera &camera,
                                             std::uint32_t offsetX, std::uint32_t offsetY) {
  if (!std::isfinite(camera.center.x) || !std::isfinite(camera.center.y) ||
      !std::isfinite(camera.rotationRadians) || std::abs(camera.rotationRadians) > 1000000 ||
      !std::isfinite(camera.verticalSize) || camera.verticalSize < 0.00001F ||
      camera.verticalSize > 1000000 || !camera.width || !camera.height || camera.width > 16384 ||
      camera.height > 16384 || offsetX > 16384 - camera.width || offsetY > 16384 - camera.height ||
      effects.lights.size() > 32)
    return std::nullopt;
  LightingFrame frame;
  const double scale = double(camera.height) / camera.verticalSize;
  const double cosine = std::cos(double(camera.rotationRadians)),
               sine = std::sin(double(camera.rotationRadians));
  const double cx = double(camera.width) / 2 + offsetX, cy = double(camera.height) / 2 + offsetY;
  auto &u = frame.uniforms;
  u.worldX = {float(cosine / scale), float(sine / scale),
              float(camera.center.x - (cosine * cx + sine * cy) / scale), 0};
  u.worldY = {float(sine / scale), float(-cosine / scale),
              float(camera.center.y + (-sine * cx + cosine * cy) / scale), 0};
  u.ambientStrength = {effects.ambient[0], effects.ambient[1], effects.ambient[2],
                       effects.strength};
  frame.submitted = uint32_t(effects.lights.size());
  for (const auto &light : effects.lights) {
    auto &out = u.lights[u.counts[0]++];
    out.positionRadiusIntensity = {light.position.x, light.position.y, light.radius,
                                   light.intensity};
    out.color = {light.color[0], light.color[1], light.color[2], 0};
    out.masks[0] = light.groups;
  }
  const Nexora::Presentation::TwoDLighting checked{u, {}};
  if (!Nexora::Presentation::ValidateTwoDLighting(&checked, 0))
    return std::nullopt;
  u.counts[0] = 0;
  std::size_t sourceIndex{};
  for (const auto &light : effects.lights) {
    const auto source = u.lights[sourceIndex++];
    const double dx = double(light.position.x) - camera.center.x,
                 dy = double(light.position.y) - camera.center.y;
    const double px = (cosine * dx + sine * dy) * scale, py = (sine * dx - cosine * dy) * scale;
    const double radius = double(light.radius) * scale;
    if (px + radius < -double(camera.width) / 2 || px - radius > double(camera.width) / 2 ||
        py + radius < -double(camera.height) / 2 || py - radius > double(camera.height) / 2 ||
        !light.groups || light.intensity == 0) {
      ++frame.culled;
      continue;
    }
    u.lights[u.counts[0]++] = source;
  }
  return frame;
}
} // namespace nexora::twod
