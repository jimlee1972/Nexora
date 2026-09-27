#include "Nexora/Renderer/GoldenImage.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
}

int main() {
  try {
    using namespace nexora::renderer;
    const std::array<std::byte, 16> expected{
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{255},
        std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255},
        std::byte{0}, std::byte{255}, std::byte{0}, std::byte{255},
        std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255}};
    auto actual = expected;
    GoldenImageComparison comparison;
    Require(CompareGoldenRgba8(actual, expected, 2, 2, 0, comparison) && comparison.matched,
            "identical RGBA8 golden image did not match");
    actual[5] = std::byte{3};
    Require(CompareGoldenRgba8(actual, expected, 2, 2, 3, comparison) && comparison.matched,
            "golden image tolerance rejected an allowed channel delta");
    actual[5] = std::byte{5};
    Require(CompareGoldenRgba8(actual, expected, 2, 2, 3, comparison) && !comparison.matched &&
                comparison.differing_pixels == 1 && comparison.max_channel_delta == 5,
            "golden image acceptance missed a differing pixel");
    Require(HashRgba8(expected) != 0, "golden image hash was not deterministic");
    std::cout << "Golden image acceptance contracts passed\n";
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << exception.what() << '\n';
    return 1;
  }
}
