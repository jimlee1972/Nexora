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
} // namespace

int main() {
  try {
    using namespace nexora::renderer;
    const std::array<std::byte, 16> expected{
        std::byte{0},   std::byte{0},   std::byte{0},   std::byte{255},
        std::byte{255}, std::byte{0},   std::byte{0},   std::byte{255},
        std::byte{0},   std::byte{255}, std::byte{0},   std::byte{255},
        std::byte{0},   std::byte{0},   std::byte{255}, std::byte{255}};
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
    // Published FNV-1a 64 test vectors: the empty input hashes to the offset basis, "a" to
    // 0xaf63dc4c8601ec8c. External tools comparing captured images depend on the real algorithm.
    const std::array<std::byte, 1> letter_a{std::byte{0x61}};
    Require(HashRgba8({}) == 0xcbf29ce484222325ULL && HashRgba8(letter_a) == 0xaf63dc4c8601ec8cULL,
            "golden image hash is not standard FNV-1a");
    const std::array<GoldenImageCase, 1> cases{
        {{"shader-library-smoke", 2, 2, 0, 0,
          std::vector<std::byte>(expected.begin(), expected.end())}}};
    const auto results = RunGoldenImageHarness(cases, [&expected](const GoldenImageCase &) {
      return std::vector<std::byte>(expected.begin(), expected.end());
    });
    Require(results.size() == 1 && results[0].passed && results[0].name == "shader-library-smoke",
            "golden image harness did not execute the named image case");
    std::cout << "Golden image acceptance contracts passed\n";
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << exception.what() << '\n';
    return 1;
  }
}
