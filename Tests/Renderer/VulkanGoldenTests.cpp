// Linux Vulkan (Mesa lavapipe) offscreen golden for the triangle frame.
//
// The baseline in Tests/Renderer/Golden/ was captured from this same path and is a *Linux software
// rasterizer* reference only. It says nothing about DX12, Metal, or physical-GPU output, which keep
// their own target-host baselines. Set NEXORA_GOLDEN_UPDATE=<path> to (re)write the baseline.

#include "Nexora/RHI/Device.h"
#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Renderer/FramePipeline.h"
#include "Nexora/Renderer/GoldenImage.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#ifndef NEXORA_GOLDEN_BASELINE
#error "NEXORA_GOLDEN_BASELINE must name the committed baseline file"
#endif

namespace {
using namespace nexora;

constexpr std::uint32_t Extent = 64;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

std::vector<std::byte> RenderTriangle(rhi::Device &device, rhi::PipelineHandle pipeline) {
  const rhi::TextureDescriptor descriptor{Extent, Extent, rhi::TextureFormat::Rgba8Unorm,
                                          rhi::ResourceState::Present, "golden target"};
  const auto target = device.CreateTexture(descriptor);
  static_cast<void>(renderer::ExecuteTriangleFrame(device, target, descriptor, pipeline));
  std::vector<std::byte> pixels(static_cast<std::size_t>(Extent) * Extent * 4);
  device.ReadTextureForTesting(target, pixels);
  device.DestroyTexture(target);
  return pixels;
}

std::vector<std::byte> ReadFile(const char *path) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return {};
  const std::vector<char> raw{std::istreambuf_iterator<char>(file), {}};
  std::vector<std::byte> bytes(raw.size());
  for (std::size_t index = 0; index < raw.size(); ++index)
    bytes[index] = static_cast<std::byte>(raw[index]);
  return bytes;
}

int RunTests() {
  if (!rhi::IsBackendAvailable(rhi::Backend::Vulkan)) {
    const auto *required = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
    Require(!(required && *required == '1'), "Vulkan is required but unavailable");
    std::cout << "Vulkan unavailable; golden render skipped\n";
    return 0;
  }
  auto device = rhi::CreateDevice(rhi::Backend::Vulkan);
  const auto layout = rhi::TrianglePipelineLayout();
  const auto pipeline = device->CreatePipeline(
      {layout.layout_hash, 0x1234, rhi::TextureFormat::Rgba8Unorm, "golden triangle"});

  const auto first = RenderTriangle(*device, pipeline);
  const auto second = RenderTriangle(*device, pipeline);
  Require(first == second, "two renders of the same frame differ");

  // The image must actually contain a rendered triangle, not a cleared or untouched target.
  std::size_t distinct_from_corner = 0;
  for (std::size_t pixel = 4; pixel < first.size(); pixel += 4) {
    for (std::size_t channel = 0; channel < 4; ++channel) {
      if (first[pixel + channel] != first[channel]) {
        ++distinct_from_corner;
        break;
      }
    }
  }
  Require(distinct_from_corner > 0 && distinct_from_corner < first.size() / 4,
          "render has no triangle coverage or covers the whole target");

  if (const auto *update = std::getenv("NEXORA_GOLDEN_UPDATE"); update && *update != '\0') {
    std::ofstream out(update, std::ios::binary);
    out.write(reinterpret_cast<const char *>(first.data()),
              static_cast<std::streamsize>(first.size()));
    Require(static_cast<bool>(out), "unable to write the golden baseline");
    std::cout << "golden baseline written to " << update << '\n';
  } else {
    const auto baseline = ReadFile(NEXORA_GOLDEN_BASELINE);
    Require(!baseline.empty(), "golden baseline is missing; run with NEXORA_GOLDEN_UPDATE=<path>");
    // Edge pixels may differ by rounding between Mesa releases; flat interior must not.
    renderer::GoldenImageComparison comparison;
    Require(renderer::CompareGoldenRgba8(first, baseline, Extent, Extent, 2, comparison),
            "golden baseline has the wrong size");
    std::cout << "golden: differing=" << comparison.differing_pixels
              << " max_delta=" << static_cast<int>(comparison.max_channel_delta)
              << " hash=" << renderer::HashRgba8(first) << '\n';
    Require(comparison.differing_pixels <= (Extent * Extent) / 100,
            "Linux Vulkan triangle render deviates from the golden baseline");
  }
  device->DestroyPipeline(pipeline);
  device->WaitIdle();
  return 0;
}
} // namespace

int main() {
  try {
    return RunTests();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
