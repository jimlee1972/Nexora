#include "Nexora/TwoD/EffectsRendering.h"
#include <cassert>
#include <cmath>
#include <limits>
using namespace nexora;
using namespace twod;
int main() {
  EffectsSnapshot effects;
  Camera camera{{2, -3}, 20, 1.04719755F, 1920, 1080};
  auto frame = PrepareLighting(effects, camera, 13, 17);
  assert(frame);
  // Independently project world points with the existing orthographic camera convention,
  // then invert through the exact GPU uniform. Includes rotation and safe-frame offset.
  for (const auto point : {math::Vector2{2, -3}, math::Vector2{-4, 1}, math::Vector2{6, -5}}) {
    const double dx = point.x - camera.center.x, dy = point.y - camera.center.y;
    const double scale = double(camera.height) / camera.verticalSize;
    const double c = std::cos(double(camera.rotationRadians)),
                 s = std::sin(double(camera.rotationRadians));
    const double x = (c * dx + s * dy) * scale + camera.width / 2.0 + 13;
    const double y = (s * dx - c * dy) * scale + camera.height / 2.0 + 17;
    const auto &u = frame->uniforms;
    assert(std::abs(x * u.worldX[0] + y * u.worldX[1] + u.worldX[2] - point.x) < 0.00001);
    assert(std::abs(x * u.worldY[0] + y * u.worldY[1] + u.worldY[2] - point.y) < 0.00001);
  }
  camera = {{}, 10, 0, 200, 100};
  NexoraTwoDEffectsLightV1 light{1, {10.25F, 0}, {1, 0.5F, 0.25F}, 1, 0.5F, 0, 0, 1, 0};
  effects.lights.push_back(light); // Radius intersects the right frame edge.
  light.position.x = 10.51F;
  effects.lights.push_back(light); // Entirely outside.
  light.position = {0, 5.25F};
  effects.lights.push_back(light); // Radius intersects the top edge.
  light.groups = 0;
  effects.lights.push_back(light);
  light.groups = 1;
  light.intensity = 0;
  effects.lights.push_back(light);
  frame = PrepareLighting(effects, camera);
  assert(frame && frame->submitted == 5 && frame->culled == 3 && frame->uniforms.counts[0] == 2);
  assert(frame->uniforms.lights[0].positionRadiusIntensity[0] == 10.25F);
  assert(frame->uniforms.lights[1].positionRadiusIntensity[1] == 5.25F);
  // Invalid data must not escape validation by being culled first.
  effects.lights[1].color[0] = std::numeric_limits<float>::quiet_NaN();
  assert(!PrepareLighting(effects, camera));
  effects.lights[1].color[0] = 1;
  camera.width = 0;
  assert(!PrepareLighting(effects, camera));
  camera.width = 200;
  assert(!PrepareLighting(effects, camera, 16384, 0));
  effects.lights.resize(33, light);
  assert(!PrepareLighting(effects, camera));

  Image image{1, 1, 1, {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}}};
  auto packed = PackAtlas({1, 2}, std::span(&image, 1), 8, 8, true);
  assert(packed);
  auto atlas = std::make_shared<Atlas>(std::move(*packed));
  const AtlasResolver resolver = [&](AssetId) { return AtlasBinding{atlas, 123}; };
  std::array<SpriteInstance, 3> sprites{};
  const std::array<uint64_t, 3> ids{3, 1, 2};
  for (std::size_t i = 0; i < sprites.size(); ++i) {
    auto &v = sprites[i];
    v.id = ids[i];
    v.sprite.atlas = atlas->id;
    v.sprite.region = 1;
    v.matrix = runtime::ToMatrix(runtime::Transform{});
    v.lighting.groups = v.id == 2 ? 2 : 1;
  }
  auto extracted = ExtractSprites(sprites, camera, resolver);
  assert(extracted && extracted->batches.size() == 3 && extracted->lightingStyles.size() == 3);
  assert(extracted->lightingStyles[0].groups == 1 && extracted->lightingStyles[1].groups == 2 &&
         extracted->lightingStyles[2].groups == 1);
  // Matching styles separated by another Sprite must never regroup transparent draws.
  for (auto &v : sprites)
    v.lighting.groups = 1;
  extracted = ExtractSprites(sprites, camera, resolver);
  assert(extracted && extracted->batches.size() == 1 && extracted->batches[0].elementCount == 18);
  sprites[0].sprite.visible = false;
  sprites[0].lighting.receiveLight = 2;
  assert(!ExtractSprites(sprites, camera, resolver));
}
