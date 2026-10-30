#include "Nexora/TwoD/EffectsRendering.h"
#include <cmath>
#include <set>
namespace nexora::twod {
std::optional<LightingFrame> PrepareLighting(const EffectsSnapshot &effects, const Camera &camera,
                                             std::uint32_t offsetX, std::uint32_t offsetY) {
  if (!std::isfinite(camera.center.x) || !std::isfinite(camera.center.y) ||
      !std::isfinite(camera.rotationRadians) || std::abs(camera.rotationRadians) > 1000000 ||
      !std::isfinite(camera.verticalSize) || camera.verticalSize < 0.00001F ||
      camera.verticalSize > 1000000 || !camera.width || !camera.height || camera.width > 16384 ||
      camera.height > 16384 || offsetX > 16384 - camera.width || offsetY > 16384 - camera.height ||
      effects.lights.size() > 32 || effects.occluders.size() > 256)
    return std::nullopt;
  std::size_t totalEdges{};
  std::set<uint64_t> occluderIds;
  for (const auto &polygon : effects.occluders) {
    if (!ValidateEffectsOccluder(polygon) || !occluderIds.insert(polygon.id).second)
      return std::nullopt;
    totalEdges += polygon.vertex_count;
    if (totalEdges > 2048)
      return std::nullopt;
  }
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
  unsigned submittedShadows{};
  for (const auto &light : effects.lights) {
    if (light.shadow > 1 || (submittedShadows += light.shadow) > 8)
      return std::nullopt;
    auto &out = u.lights[u.counts[0]++];
    out.positionRadiusIntensity = {light.position.x, light.position.y, light.radius,
                                   light.intensity};
    out.color = {light.color[0], light.color[1], light.color[2], 0};
    out.masks[0] = light.groups;
    out.shadow = {light.softness, light.bias, 0, 0};
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
    const auto index = u.counts[0]++;
    u.lights[index] = source;
    if (light.shadow) {
      const auto slot = u.counts[1]++;
      u.shadowLights[slot / 4][slot % 4] = index;
      u.lights[index].masks[1] = slot + 1;
    }
  }
  if (u.counts[1]) {
    frame.edges.reserve(totalEdges);
    for (const auto &polygon : effects.occluders) {
      double minX = polygon.vertices[0].x, maxX = minX;
      double minY = polygon.vertices[0].y, maxY = minY;
      for (unsigned i = 1; i < polygon.vertex_count; ++i) {
        minX = std::min(minX, double(polygon.vertices[i].x));
        maxX = std::max(maxX, double(polygon.vertices[i].x));
        minY = std::min(minY, double(polygon.vertices[i].y));
        maxY = std::max(maxY, double(polygon.vertices[i].y));
      }
      bool relevant{};
      for (unsigned slot = 0; slot < u.counts[1]; ++slot) {
        const auto &light = u.lights[u.shadowLights[slot / 4][slot % 4]];
        if (!(light.masks[0] & polygon.groups))
          continue;
        const double x = light.positionRadiusIntensity[0], y = light.positionRadiusIntensity[1];
        const double dx = x - std::clamp(x, minX, maxX), dy = y - std::clamp(y, minY, maxY);
        const double radius = light.positionRadiusIntensity[2];
        relevant = relevant || (dx * dx + dy * dy <= radius * radius);
      }
      if (!relevant)
        continue;
      for (unsigned i = 0; i < polygon.vertex_count; ++i) {
        const auto &a = polygon.vertices[i], &b = polygon.vertices[(i + 1) % polygon.vertex_count];
        frame.edges.push_back({{a.x, a.y, b.x, b.y}, {polygon.groups, 0, 0, 0}});
      }
    }
  }
  u.counts[2] = uint32_t(frame.edges.size());
  const Nexora::Presentation::TwoDLighting finalCheck{u, {}, frame.edges};
  if (!Nexora::Presentation::ValidateTwoDLighting(&finalCheck, 0))
    return std::nullopt;
  return frame;
}
} // namespace nexora::twod
