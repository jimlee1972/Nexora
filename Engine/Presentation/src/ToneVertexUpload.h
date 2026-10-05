#pragma once
#include <array>
namespace Nexora::Presentation {
// One full-screen triangle: float2 position then float2 UV. Native protecting frames own uploads.
inline constexpr std::array<float, 12> toneVertices{-1, -1, 0, 1, 3, -1, 2, 1, -1, 3, 0, -1};
} // namespace Nexora::Presentation
