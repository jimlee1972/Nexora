#pragma once
#include "Nexora/TwoD/GameplayEffects.h"
#include "Nexora/TwoD/Scene.h"

namespace nexora::twod {
struct LightingFrame final {
  Nexora::Presentation::TwoDLightingUniform uniforms;
  std::uint32_t submitted{}, culled{};
};
// Pure copied preparation from a frozen snapshot/camera. Viewport offset is in framebuffer
// pixels; DPI/UI scale is not applied again. Light culling preserves submitted light order.
[[nodiscard]] NEXORA_2D_API std::optional<LightingFrame> PrepareLighting(const EffectsSnapshot &,
                                                                         const Camera &,
                                                                         std::uint32_t offsetX = 0,
                                                                         std::uint32_t offsetY = 0);
} // namespace nexora::twod
