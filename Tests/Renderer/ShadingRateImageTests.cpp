#include "Nexora/RHI/Capabilities.h"
#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Renderer/RenderGraph.h"
#include <algorithm>
#include <array>
#include <barrier>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace nexora;
namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
template <class F> void Reject(F function, const char *message) {
  try {
    function();
  } catch (const std::exception &) {
    return;
  }
  throw std::runtime_error(message);
}
using Pixels = std::vector<std::byte>;
Pixels Read(rhi::Device &device, rhi::TextureHandle target) {
  Pixels pixels(64 * 64 * 4);
  device.ReadTextureForTesting(target, pixels);
  return pixels;
}
void Record(rhi::CommandList &list, rhi::TextureHandle target, rhi::PipelineHandle pipeline,
            rhi::ShadingRateImageHandle image = {}, bool draw = true) {
  list.Transition({target, rhi::ResourceState::ShaderRead, rhi::ResourceState::RenderTarget});
  list.BeginRendering({target, 64, 64});
  if (draw) {
    list.BindPipeline(pipeline);
    if (image.IsValid())
      list.BindShadingRateImage(image);
    list.Draw(3);
  }
  list.EndRendering();
  list.Transition({target, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderRead});
}
Pixels Draw(rhi::Device &device, rhi::TextureHandle target, rhi::PipelineHandle pipeline,
            rhi::ShadingRateImageHandle image = {}, bool draw = true) {
  auto list = device.CreateCommandList(rhi::QueueType::Graphics);
  Record(*list, target, pipeline, image, draw);
  device.WaitForSubmission(device.Submit(*list));
  return Read(device, target);
}
void Compare(const Pixels &reference, const Pixels &image) {
  unsigned covered = 0;
  for (std::size_t i = 0; i < reference.size(); i += 4) {
    const auto visible = [](const Pixels &pixels, std::size_t p) {
      return pixels[p] != std::byte{0} || pixels[p + 1] != std::byte{0} ||
             pixels[p + 2] != std::byte{0};
    };
    Require(reference[i + 3] == image[i + 3] && visible(reference, i) == visible(image, i),
            "rate image changed alpha or raster coverage");
    covered += visible(reference, i);
    for (std::size_t channel = 0; channel < 3; ++channel)
      Require(std::abs(std::to_integer<int>(reference[i + channel]) -
                       std::to_integer<int>(image[i + channel])) <= 32,
              "rate image exceeded the bounded gradient32/255 error budget");
  }
  Require(covered > 100, "rate-image geometry is empty");
}
std::vector<std::uint8_t> Rates(std::uint32_t tile, unsigned pattern, unsigned extent = 64) {
  const auto width = (extent + tile - 1) / tile;
  std::vector<std::uint8_t> rates(width * width);
  constexpr std::array<std::uint8_t, 4> codes{0, 1, 4, 5};
  for (std::uint32_t y = 0; y < width; ++y)
    for (std::uint32_t x = 0; x < width; ++x)
      rates[y * width + x] = codes[pattern < 4 ? pattern : (x + 3 * y) % 4];
  return rates;
}
void Portable() {
  rhi::ShadingRateImageDescriptor descriptor{64, 64, "shape"};
  Require(rhi::IsShadingRateImageShapeValid(descriptor, 16, 16) &&
              !rhi::IsShadingRateImageShapeValid(descriptor, 0, 16) &&
              !rhi::IsShadingRateImageShapeValid(descriptor, 16, 15),
          "rate-image shape bounds lost");
  Require(rhi::IsShadingRateImageCodeValid(0) && rhi::IsShadingRateImageCodeValid(5) &&
              !rhi::IsShadingRateImageCodeValid(2) && !rhi::IsShadingRateImageCodeValid(255),
          "rate-code bounds lost");
  rhi::PipelineDescriptor image{1, 2, rhi::TextureFormat::Rgba8Unorm, "image"};
  image.shading_rate = rhi::ShadingRate::Image;
  Require(rhi::EffectivePipelineRequirements(image).minimum_tiers[static_cast<std::size_t>(
              rhi::GraphicsFeature::VariableRateShading)] == rhi::FeatureVersion{2, 0},
          "rate images lost independent tier2 admission");
  auto model = rhi::CreateValidationDevice();
  Reject([&] { (void)model->CreatePipeline(image); }, "Validation simulated rate-image PSO");
  Reject([&] { (void)model->CreateShadingRateImage(descriptor, Rates(16, 0)); },
         "Validation simulated rate image");
}
void Lifetime(rhi::Device &device, rhi::PipelineHandle full, rhi::PipelineHandle selected,
              rhi::ShadingRateImageHandle image, rhi::TextureFormat format, const Pixels &reference,
              std::uint32_t tile) {
  const auto target =
      device.CreateTexture({64, 64, format, rhi::ResourceState::ShaderRead, "lifetime"});
  const auto sentinel = Draw(device, target, full, {}, false);
  const auto before = device.Diagnostics();
  {
    auto abandoned = device.CreateCommandList(rhi::QueueType::Graphics);
    Record(*abandoned, target, selected, image);
  }
  Require(Read(device, target) == sentinel && device.Diagnostics().shading_rate_image_draw_calls ==
                                                  before.shading_rate_image_draw_calls,
          "abandoned rate-image list committed");
  {
    auto list = device.CreateCommandList(rhi::QueueType::Graphics);
    list->Transition({target, rhi::ResourceState::ShaderRead, rhi::ResourceState::RenderTarget});
    list->BeginRendering({target, 64, 64});
    list->BindPipeline(selected);
    Reject([&] { list->Draw(3); }, "image draw without source admitted");
    Reject([&] { list->BindShadingRateImage({}); }, "invalid image handle admitted");
    list->BindShadingRateImage(image);
    list->BindPipeline(full);
    list->Draw(3);
    list->EndRendering();
    list->Transition({target, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderRead});
    device.WaitForSubmission(device.Submit(*list));
    Require(Read(device, target) == reference, "full-rate rebind retained image state");
  }
  const auto codes = Rates(tile, 3);
  auto temporary = device.CreateShadingRateImage({64, 64, "stale"}, codes);
  {
    auto stale = device.CreateCommandList(rhi::QueueType::Graphics);
    Record(*stale, target, selected, temporary);
    device.DestroyShadingRateImage(temporary);
    const auto replacement = device.CreateShadingRateImage({64, 64, "replacement"}, codes);
    Require(replacement != temporary, "image generation reused");
    Reject([&] { (void)device.Submit(*stale); }, "destroyed rate image submitted");
    device.DestroyShadingRateImage(replacement);
  }
  const auto wrong = device.CreateShadingRateImage({128, 128, "wrong extent"}, Rates(tile, 3, 128));
  {
    auto list = device.CreateCommandList(rhi::QueueType::Graphics);
    list->Transition({target, rhi::ResourceState::ShaderRead, rhi::ResourceState::RenderTarget});
    list->BeginRendering({target, 64, 64});
    list->BindPipeline(selected);
    Reject([&] { list->BindShadingRateImage(wrong); }, "mismatched image raster admitted");
  }
  device.DestroyShadingRateImage(wrong);
  std::array<rhi::ShadingRateImageHandle, rhi::ShadingRateImageMaximumBindingsPerList + 1> images;
  for (auto &source : images)
    source = device.CreateShadingRateImage({64, 64, "capacity"}, codes);
  {
    auto list = device.CreateCommandList(rhi::QueueType::Graphics);
    list->Transition({target, rhi::ResourceState::ShaderRead, rhi::ResourceState::RenderTarget});
    list->BeginRendering({target, 64, 64});
    list->BindPipeline(selected);
    for (std::size_t i = 0; i < images.size() - 1; ++i)
      list->BindShadingRateImage(images[i]);
    list->BindShadingRateImage(
        images[0]); // Rebinding a captured immutable source consumes no slot.
    Reject([&] { list->BindShadingRateImage(images.back()); }, "rate-image capacity exceeded");
  }
  for (auto source : images)
    device.DestroyShadingRateImage(source);
  device.DestroyTexture(target);
}
void Parallel(rhi::Device &device, rhi::PipelineHandle full, rhi::PipelineHandle selected,
              rhi::ShadingRateImageHandle image, rhi::TextureFormat format,
              const Pixels &reference) {
  const std::array targets{
      device.CreateTexture({64, 64, format, rhi::ResourceState::ShaderRead, "worker0"}),
      device.CreateTexture({64, 64, format, rhi::ResourceState::ShaderRead, "worker1"})};
  const auto sentinel = Draw(device, targets[0], full, {}, false);
  (void)Draw(device, targets[1], full, {}, false);
  core::JobSystem jobs{2};
  jobs.Start();
  renderer::RenderGraph graph;
  const rhi::TextureDescriptor descriptor{64, 64, format, rhi::ResourceState::ShaderRead,
                                          "image worker"};
  std::array<renderer::GraphTexture, 2> imports;
  bool fail = true, cancel = false;
  core::CancellationSource cancellation;
  std::barrier overlap{2};
  for (std::size_t i = 0; i < 2; ++i) {
    imports[i] = graph.ImportTexture(targets[i], descriptor);
    renderer::RenderGraph::PassDescriptor pass{"rate-image worker",
                                               rhi::QueueType::Graphics,
                                               {},
                                               {{imports[i], rhi::ResourceState::RenderTarget}},
                                               [&, i](auto &list, auto) {
                                                 list.BeginRendering({targets[i], 64, 64});
                                                 list.BindPipeline(selected);
                                                 if (image.IsValid())
                                                   list.BindShadingRateImage(image);
                                                 list.Draw(3);
                                                 list.EndRendering();
                                                 overlap.arrive_and_wait();
                                                 if (i == 1 && fail)
                                                   throw std::runtime_error(
                                                       "forced image worker failure");
                                                 if (i == 1 && cancel)
                                                   cancellation.Cancel();
                                               }};
    pass.allow_parallel_recording = true;
    (void)graph.AddPass(std::move(pass));
  }
  for (auto imported : imports)
    (void)graph.AddPass({"restore image target",
                         rhi::QueueType::Graphics,
                         {{imported, rhi::ResourceState::ShaderRead}},
                         {},
                         [](auto &, auto) {}});
  graph.Compile();
  renderer::RenderGraph::ExecutionOptions options{renderer::RenderGraph::RecordingMode::Parallel,
                                                  &jobs, 2};
  const auto before = device.Diagnostics();
  Reject([&] { graph.Execute(device, options); }, "image worker failure accepted");
  fail = false;
  cancel = true;
  options.cancellation = cancellation.Token();
  Reject([&] { graph.Execute(device, options); }, "cancelled image cohort accepted");
  Require(device.Diagnostics().submitted_command_lists == before.submitted_command_lists &&
              device.Diagnostics().shading_rate_image_draw_calls ==
                  before.shading_rate_image_draw_calls &&
              Read(device, targets[0]) == sentinel && Read(device, targets[1]) == sentinel,
          "aborted image cohort committed work");
  cancel = false;
  options.cancellation = {};
  graph.Execute(device, options);
  Require(graph.GetStatistics().parallel_recorded_pass_count == 2, "image workers did not overlap");
  Compare(reference, Read(device, targets[0]));
  Compare(reference, Read(device, targets[1]));
  jobs.Stop();
  for (auto target : targets)
    device.DestroyTexture(target);
}
#if defined(_WIN32)
class Environment final {
public:
  Environment(const char *name, const char *value) : name_(name) {
    if (const auto *old = std::getenv(name)) {
      old_ = old;
      present_ = true;
    }
    Require(_putenv_s(name, value ? value : "") == 0, "environment override failed");
  }
  ~Environment() { (void)_putenv_s(name_.c_str(), present_ ? old_.c_str() : ""); }

private:
  std::string name_, old_;
  bool present_{};
};
void Witness(bool enhanced) {
  const auto *vs = std::getenv("NEXORA_VRS_WITNESS_VERTEX_PATH"),
             *ps = std::getenv("NEXORA_VRS_WITNESS_FRAGMENT_PATH");
#if defined(NEXORA_TEST_VRS_IMAGE_PREVIEW)
  Require(vs && ps, "required shading-rate witness artifacts missing");
#else
  if (!vs || !ps)
    return;
#endif
  const std::string vertex{vs}, fragment{ps};
  Environment vertex_environment{"NEXORA_SLANG_DXIL_VERTEX_PATH", vertex.c_str()};
  Environment fragment_environment{"NEXORA_SLANG_DXIL_FRAGMENT_PATH", fragment.c_str()};
  rhi::DeviceCreationOptions options;
  options.enable_d3d12_preview_runtime = true;
  options.enable_enhanced_barriers = enhanced;
  auto device = rhi::CreateDeviceWithOptions(rhi::Backend::Direct3D12, options);
  const auto tile = device->Capabilities().shading_rate_image_tile_size;
  Require(tile != 0, "witness tile size unavailable");
  rhi::PipelineDescriptor descriptor{rhi::TrianglePipelineLayout().layout_hash, 0x56525332,
                                     rhi::TextureFormat::Rgba8Unorm, "SV_ShadingRate witness"};
  descriptor.shading_rate = rhi::ShadingRate::Image;
  descriptor.graphics_requirements.minimum_dxil_shader_model = {6, 4};
  const auto pipeline = device->CreatePipeline(descriptor);
  const auto target = device->CreateTexture(
      {64, 64, rhi::TextureFormat::Rgba8Unorm, rhi::ResourceState::ShaderRead, "witness"});
  for (unsigned pattern = 0; pattern < 5; ++pattern) {
    const auto expected_rates = Rates(tile, pattern);
    auto rates = expected_rates;
    const auto source = device->CreateShadingRateImage({64, 64, "witness source"}, rates);
    std::fill(rates.begin(), rates.end(), 255);
    const auto pixels = Draw(*device, target, pipeline, source);
    unsigned covered = 0, exact = 0;
    for (unsigned y = 0; y < 64; ++y)
      for (unsigned x = 0; x < 64; ++x) {
        const auto offset = (y * 64 + x) * 4;
        if (pixels[offset + 2] == std::byte{0})
          continue;
        ++covered;
        const auto r = std::to_integer<unsigned>(pixels[offset]),
                   g = std::to_integer<unsigned>(pixels[offset + 1]);
        Require((r == 0 || r == 85) && (g == 0 || g == 85), "invalid SV_ShadingRate encoding");
        const auto actual = (r / 85) * 4 + g / 85;
        const auto requested = expected_rates[(y / tile) * ((64 + tile - 1) / tile) + x / tile];
        Require((actual >> 2) <= (requested >> 2) && (actual & 3) <= (requested & 3),
                "rate image produced a coarser unrequested rate");
        exact += actual == requested;
      }
    Require(covered > 100 && exact > 16, "shading-rate witness did not observe requested rates");
    std::cout << "VRS image witness pattern=" << pattern << " covered=" << covered
              << " exact=" << exact << '\n';
    device->DestroyShadingRateImage(source);
  }
  device->DestroyTexture(target);
  device->DestroyPipeline(pipeline);
}
#endif
void Native(bool enhanced) {
#if defined(_WIN32)
  constexpr auto backend = rhi::Backend::Direct3D12;
#elif defined(__APPLE__)
  constexpr auto backend = rhi::Backend::Metal;
#else
  constexpr auto backend = rhi::Backend::Vulkan;
#endif
  rhi::DeviceCreationOptions options{false, false, enhanced};
#if defined(NEXORA_TEST_VRS_IMAGE_PREVIEW)
  options.enable_d3d12_preview_runtime = true;
#else
  if (!rhi::IsBackendAvailable(backend)) {
    const auto *required = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
    Require(!required || *required != '1', "required image/reference backend unavailable");
    return;
  }
#endif
  auto device = rhi::CreateDeviceWithOptions(backend, options);
  rhi::PipelineDescriptor descriptor{rhi::TrianglePipelineLayout().layout_hash, 0x56525331,
                                     rhi::TextureFormat::Rgba8Unorm, "rate image"};
  auto preferred = descriptor;
  preferred.shading_rate = rhi::ShadingRate::Image;
  const bool enabled = rhi::CheckGraphicsRequirements(device->Capabilities(),
                                                      rhi::EffectivePipelineRequirements(preferred))
                           .Succeeded();
#if defined(NEXORA_TEST_VRS_IMAGE_PREVIEW)
  Require(enabled, "required native VRS tier2 image path unavailable");
#endif
  const auto tile = device->Capabilities().shading_rate_image_tile_size;
  for (auto format : {rhi::TextureFormat::Rgba8Unorm, rhi::TextureFormat::Bgra8Unorm}) {
    descriptor.color_format = preferred.color_format = format;
    const auto full = device->CreatePipeline(descriptor);
    const auto selected = enabled ? device->CreatePipeline(preferred) : full;
    const auto target =
        device->CreateTexture({64, 64, format, rhi::ResourceState::ShaderRead, "rate target"});
    const auto reference = Draw(*device, target, full);
    for (unsigned pattern = 0; pattern < 5; ++pattern) {
      auto rates = Rates(enabled ? tile : 16, pattern);
      rhi::ShadingRateImageHandle source{};
      if (enabled) {
        source = device->CreateShadingRateImage({64, 64, "immutable source"}, rates);
        std::fill(rates.begin(), rates.end(),
                  255); // Caller memory cannot redirect an uploaded image.
      } else {
        Reject([&] { (void)device->CreateShadingRateImage({64, 64, "unavailable"}, rates); },
               "unavailable image admitted");
        Reject([&] { (void)device->CreatePipeline(preferred); }, "unavailable tier2 PSO admitted");
      }
      Compare(reference, Draw(*device, target, selected, source));
      Require(Draw(*device, target, full) == reference, "full-rate reset failed");
      if (source.IsValid())
        device->DestroyShadingRateImage(source);
    }
    rhi::ShadingRateImageHandle source{};
    if (enabled)
      source = device->CreateShadingRateImage({64, 64, "shared worker image"}, Rates(tile, 3));
    Parallel(*device, full, selected, source, format, reference);
    if (enabled) {
      Lifetime(*device, full, selected, source, format, reference, tile);
      device->DestroyShadingRateImage(source);
    }
    device->DestroyTexture(target);
    if (enabled)
      device->DestroyPipeline(selected);
    device->DestroyPipeline(full);
  }
  Require(enabled ? device->Diagnostics().shading_rate_image_draw_calls > 0
                  : device->Diagnostics().shading_rate_image_draw_calls == 0,
          "rate-image diagnostics disagree with admission");
  std::cout << "VRS image backend=" << static_cast<unsigned>(backend) << " enabled=" << enabled
            << " tile=" << tile << " draws=" << device->Diagnostics().shading_rate_image_draw_calls
            << "; quality/coverage/reset/lifetime/workers passed\n";
#if defined(_WIN32)
  if (enabled)
    Witness(enhanced);
#endif
}
} // namespace
int main(int argc, char **argv) {
  try {
    const bool enhanced = argc == 2 && std::string_view(argv[1]) == "--enhanced-barriers";
    Require(argc == 1 || enhanced, "usage: [--enhanced-barriers]");
    Portable();
    Native(enhanced);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
