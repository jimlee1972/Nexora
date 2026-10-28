#pragma once
#include "Nexora/TwoD/Scene.h"

namespace nexora::twod::detail {
// Internal geometry preparation; caller validates the immutable Atlas closure separately.
[[nodiscard]] std::optional<SpriteInstance> SpriteGeometry(const SpriteAsset &, SpriteInstance);
} // namespace nexora::twod::detail
