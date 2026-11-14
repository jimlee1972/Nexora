#pragma once
#include "Nexora/TwoD/Scene.h"

namespace nexora::twod::detail {
// Internal geometry preparation; caller validates the immutable Atlas closure separately.
[[nodiscard]] std::optional<SpriteInstance> SpriteGeometry(const SpriteAsset &, SpriteInstance);
// Caller must first validate this exact immutable Atlas, including every region overlap.
[[nodiscard]] std::optional<SpriteInstance>
ResolveSpriteAssetWithValidatedAtlas(const SpriteAsset &, const Atlas &, SpriteInstance);
} // namespace nexora::twod::detail
