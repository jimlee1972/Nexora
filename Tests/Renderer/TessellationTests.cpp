#include "Nexora/RHI/Capabilities.h"
#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/RHI/SpirvArtifact.h"
#include "Nexora/Renderer/PipelineCache.h"
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
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class Function> void Reject(Function function, const char *message) {
  bool rejected = false;
  try {
    function();
  } catch (const std::exception &) {
    rejected = true;
  }
  Require(rejected, message);
}
void Word(std::vector<std::byte> &bytes, std::uint32_t value) {
  for (unsigned index = 0; index < 4; ++index)
    bytes.push_back(static_cast<std::byte>((value >> (index * 8U)) & 255U));
}
void Entry(std::vector<std::byte> &bytes, std::uint32_t model, const std::string &name) {
  const auto words = (name.size() + 4) / 4;
  Word(bytes, (static_cast<std::uint32_t>(3 + words) << 16U) | 15U);
  Word(bytes, model);
  Word(bytes, 2);
  for (char character : name)
    bytes.push_back(static_cast<std::byte>(character));
  bytes.push_back(std::byte{0});
  while (bytes.size() % 4)
    bytes.push_back(std::byte{0});
}
void TestPortable() {
  using namespace rhi;
  // Metadata fixtures intentionally contain no executable shader body.
  std::vector<std::byte> bytes;
  for (auto value : {0x07230203U, 0x00010300U, 0U, 16U, 0U})
    Word(bytes, value);
  Entry(bytes, 1, "control");
  Entry(bytes, 2, "evaluation");
  Require(HasSpirvEntryPoint(bytes, ShaderStage::TessellationControl, "control") &&
              HasSpirvEntryPoint(bytes, ShaderStage::TessellationEvaluation, "evaluation"),
          "SPIR-V stage metadata");
  Require(!HasSpirvEntryPoint(bytes, ShaderStage::Compute, "evaluation"),
          "wrong execution model admitted");
  for (std::size_t size = 0; size < bytes.size(); ++size)
    Require(!HasSpirvEntryPoint(std::span{bytes}.first(size), ShaderStage::TessellationEvaluation,
                                "evaluation"),
            "truncated entry admitted");
  auto duplicate = bytes;
  Entry(duplicate, 2, "evaluation");
  Require(!HasSpirvEntryPoint(duplicate, ShaderStage::TessellationEvaluation, "evaluation"),
          "duplicate entry admitted");
  std::vector<std::byte> unaligned{std::byte{1}};
  unaligned.insert(unaligned.end(), bytes.begin(), bytes.end());
  Require(HasSpirvEntryPoint(std::span{unaligned}.subspan(1), ShaderStage::TessellationEvaluation,
                             "evaluation"),
          "unaligned entry metadata");
  for (const auto offset : {0U, 4U, 12U, 16U, 20U}) {
    auto invalid = bytes;
    std::fill_n(invalid.begin() + offset, 4, std::byte{0});
    if (offset == 16)
      invalid[offset] = std::byte{1};
    Require(!HasSpirvEntryPoint(invalid, ShaderStage::TessellationEvaluation, "evaluation"),
            "invalid SPIR-V envelope admitted");
  }
  PipelineDescriptor patch{1, 2, TextureFormat::Rgba8Unorm, "patch"};
  patch.topology = PrimitiveTopology::PatchList;
  patch.patch_control_points = 3;
  auto fallback = patch;
  fallback.topology = PrimitiveTopology::TriangleList;
  fallback.patch_control_points = 0;
  fallback.graphics_shader = GraphicsShaderVariant::PreTessellatedPatch;
  auto baseline = fallback;
  baseline.graphics_shader = GraphicsShaderVariant::Triangle;
  const auto required = EffectivePipelineRequirements(patch);
  Require(required.minimum_tiers[static_cast<std::size_t>(GraphicsFeature::Tessellation)] ==
              FeatureVersion{1, 0},
          "topology must imply tessellation admission");
  Require(renderer::MakePipelineCacheKey(patch) != renderer::MakePipelineCacheKey(fallback) &&
              renderer::MakePipelineCacheKey(fallback) != renderer::MakePipelineCacheKey(baseline),
          "geometry variants alias in PSO identity");
  auto bad = patch;
  bad.patch_control_points = 4;
  Reject([&] { (void)EffectivePipelineRequirements(bad); }, "unsupported patch size admitted");
  bad = fallback;
  bad.patch_control_points = 3;
  Reject([&] { (void)EffectivePipelineRequirements(bad); }, "triangle patch size admitted");
  bad = patch;
  bad.type = PipelineType::Compute;
  Reject([&] { (void)EffectivePipelineRequirements(bad); }, "compute patch topology admitted");
  auto device = CreateValidationDevice();
  Reject([&] { (void)device->CreatePipeline(patch); }, "Validation claimed native tessellation");
  const auto pipeline = device->CreatePipeline(fallback);
  const auto target = device->CreateTexture(
      {8, 8, TextureFormat::Rgba8Unorm, ResourceState::RenderTarget, "model"});
  auto list = device->CreateCommandList(QueueType::Graphics);
  list->BeginRendering({target, 8, 8});
  list->BindPipeline(pipeline);
  Reject([&] { list->Draw(3); }, "baked mesh draw must be bounded");
  Reject([&] { list->DrawIndirect(1); }, "baked mesh indirect draw admitted");
  list->Draw(PreTessellatedPatchVertexCount);
  list->EndRendering();
  device->WaitForSubmission(device->Submit(*list));
  list.reset();
  device->DestroyPipeline(pipeline);
  device->DestroyTexture(target);
  std::string error;
  ShaderResourceBindingMetadata binding{
      0, 0, BindingType::ConstantBuffer,
      static_cast<std::uint8_t>(static_cast<unsigned>(ShaderStage::TessellationControl) |
                                static_cast<unsigned>(ShaderStage::TessellationEvaluation))};
  Require(ValidateShaderResourceBindings(std::span{&binding, 1}, {}, error),
          "tessellation reflection stage bits rejected");
}
class Environment final {
public:
  Environment(const char *name, const char *replacement) : name_(name) {
    if (const auto *original = std::getenv(name)) {
      existed_ = true;
      value_ = original;
    }
    Set(replacement);
  }
  ~Environment() { Set(existed_ ? value_.c_str() : nullptr); }

private:
  void Set(const char *value) {
#if defined(_WIN32)
    _putenv_s(name_, value ? value : "");
#else
    if (value)
      setenv(name_, value, 1);
    else
      unsetenv(name_);
#endif
  }
  const char *name_;
  bool existed_{};
  std::string value_;
};
using Pixels = std::vector<std::byte>;
Pixels Read(rhi::Device &device, rhi::TextureHandle target) {
  Pixels result(64 * 64 * 4);
  device.ReadTextureForTesting(target, result);
  return result;
}
Pixels Draw(rhi::Device &device, rhi::TextureHandle target, rhi::PipelineHandle pipeline,
            unsigned vertices) {
  auto list = device.CreateCommandList(rhi::QueueType::Graphics);
  list->Transition({target, rhi::ResourceState::ShaderRead, rhi::ResourceState::RenderTarget});
  list->BeginRendering({target, 64, 64});
  if (vertices) {
    list->BindPipeline(pipeline);
    list->Draw(vertices);
  }
  list->EndRendering();
  list->Transition({target, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderRead});
  device.WaitForSubmission(device.Submit(*list));
  list.reset();
  return Read(device, target);
}
void Same(const Pixels &a, const Pixels &b) {
  Require(a.size() == b.size(), "image extent differs");
  for (std::size_t index = 0; index < a.size(); ++index) {
    const auto x = std::to_integer<int>(a[index]), y = std::to_integer<int>(b[index]);
    Require(std::abs(x - y) <= 1,
            "native tessellation differs from baked mesh by more than one channel step");
  }
}
void TestGraph(rhi::Device &device, rhi::PipelineHandle first, unsigned first_vertices,
               rhi::PipelineHandle fallback, const Pixels &expected, rhi::TextureFormat format) {
  const rhi::TextureDescriptor descriptor{64, 64, format, rhi::ResourceState::ShaderRead,
                                          "patch graph"};
  const std::array targets{device.CreateTexture(descriptor), device.CreateTexture(descriptor)};
  const std::array sentinel{Draw(device, targets[0], fallback, 0),
                            Draw(device, targets[1], fallback, 0)};
  renderer::RenderGraph graph;
  core::CancellationSource cancellation;
  std::barrier overlap{2};
  bool fail = false, cancel = false;
  std::array<renderer::GraphTexture, 2> imported;
  for (std::size_t index = 0; index < 2; ++index) {
    auto texture = graph.ImportTexture(targets[index], descriptor);
    imported[index] = texture;
    renderer::RenderGraph::PassDescriptor pass{
        "curved patch",
        rhi::QueueType::Graphics,
        {},
        {{texture, rhi::ResourceState::RenderTarget}},
        [&, index](auto &commands, auto) {
          commands.BeginRendering({targets[index], 64, 64});
          commands.BindPipeline(index == 0 ? first : fallback);
          commands.Draw(index == 0 ? first_vertices : rhi::PreTessellatedPatchVertexCount);
          commands.EndRendering();
          overlap.arrive_and_wait();
          if (index == 0 && fail)
            throw std::runtime_error("intentional curved patch recording failure");
          if (index == 1 && cancel)
            cancellation.Cancel();
        }};
    pass.allow_parallel_recording = true;
    (void)graph.AddPass(std::move(pass));
  }
  for (std::size_t index = 0; index < 2; ++index) {
    (void)graph.AddPass({"restore patch target",
                         rhi::QueueType::Graphics,
                         {{imported[index], rhi::ResourceState::ShaderRead}},
                         {},
                         [](auto &, auto) {}});
  }
  graph.Compile();
  core::JobSystem jobs{2};
  jobs.Start();
  renderer::RenderGraph::ExecutionOptions options{renderer::RenderGraph::RecordingMode::Parallel,
                                                  &jobs, 2};
  for (unsigned mode = 0; mode < 2; ++mode) {
    fail = mode == 0;
    cancel = mode == 1;
    options.cancellation = cancel ? cancellation.Token() : core::CancellationToken{};
    const auto submitted = device.Diagnostics().submitted_command_lists;
    Reject([&] { graph.Execute(device, options); }, "failed/cancelled patch graph completed");
    Require(device.Diagnostics().submitted_command_lists == submitted,
            "aborted patch cohort submitted");
    for (std::size_t index = 0; index < 2; ++index)
      Require(Read(device, targets[index]) == sentinel[index],
              "aborted patch graph changed pixels");
  }
  fail = cancel = false;
  options.cancellation = {};
  graph.Execute(device, options);
  Require(graph.GetStatistics().parallel_recorded_pass_count == 2,
          "patch graph did not use both workers");
  for (auto target : targets)
    Same(Read(device, target), expected);
  jobs.Stop();
  for (auto target : targets)
    device.DestroyTexture(target);
}
bool Required(const char *name) {
  const auto *value = std::getenv(name);
  return value && *value == '1';
}
void TestNative() {
#if defined(_WIN32)
  constexpr auto backend = rhi::Backend::Direct3D12;
  constexpr auto fallback_environment = "NEXORA_SLANG_PRETESSELLATED_DXIL_PATH";
#elif defined(__APPLE__)
  constexpr auto backend = rhi::Backend::Metal;
  constexpr auto fallback_environment = "NEXORA_SLANG_PRETESSELLATED_METAL_PATH";
#else
  constexpr auto backend = rhi::Backend::Vulkan;
  constexpr auto fallback_environment = "NEXORA_SLANG_PRETESSELLATED_SPIRV_PATH";
#endif
  if (!std::getenv(fallback_environment) || !rhi::IsBackendAvailable(backend)) {
    Require(!Required("NEXORA_REQUIRE_NATIVE_BACKENDS"),
            "native tessellation/fallback backend required");
    std::cout << "Portable tessellation contracts passed; native artifacts unavailable\n";
    return;
  }
  auto device = rhi::CreateDevice(backend);
  const auto caps = device->Capabilities();
  rhi::PipelineDescriptor patch{rhi::TrianglePipelineLayout().layout_hash, 0x54455353,
                                rhi::TextureFormat::Rgba8Unorm, "curved patch"};
  patch.topology = rhi::PrimitiveTopology::PatchList;
  patch.patch_control_points = 3;
  auto fallback = patch;
  fallback.topology = rhi::PrimitiveTopology::TriangleList;
  fallback.patch_control_points = 0;
  fallback.graphics_shader = rhi::GraphicsShaderVariant::PreTessellatedPatch;
  const auto selection = rhi::SelectGraphicsPath(caps, rhi::EffectivePipelineRequirements(patch),
                                                 rhi::EffectivePipelineRequirements(fallback));
  const bool native = selection.path == rhi::GraphicsPath::Preferred;
  if (Required("NEXORA_REQUIRE_NATIVE_TESSELLATION"))
    Require(native, "required native patch path unavailable");
  if (!native)
    Reject([&] { (void)device->CreatePipeline(patch); },
           "unsupported native tessellation admitted");
  {
    Environment missing{fallback_environment, "missing-pre-tessellated-artifact"};
    Reject([&] { (void)device->CreatePipeline(fallback); },
           "missing requested baked artifact admitted");
  }
  if (native && backend == rhi::Backend::Vulkan) {
    const auto *baseline = std::getenv("NEXORA_SLANG_SPIRV_PATH");
    Require(baseline, "SPIR-V baseline required for wrong-stage admission");
    Environment wrong{"NEXORA_SLANG_TESSELLATION_SPIRV_PATH", baseline};
    Reject([&] { (void)device->CreatePipeline(patch); }, "wrong requested SPIR-V stages admitted");
  }
#if defined(_WIN32)
  if (native) {
    const auto *vertex = std::getenv("NEXORA_SLANG_DXIL_VERTEX_PATH");
    Require(vertex, "DXIL baseline required for wrong-stage admission");
    {
      Environment wrong{"NEXORA_SLANG_TESSELLATION_CONTROL_DXIL_PATH", vertex};
      Reject([&] { (void)device->CreatePipeline(patch); },
             "wrong requested DXIL control stage admitted");
    }
    if (caps.highest_dxil_shader_model >= rhi::FeatureVersion{6, 1}) {
      auto newer = patch;
      newer.graphics_requirements.minimum_dxil_shader_model = {6, 1};
      Reject([&] { (void)device->CreatePipeline(newer); },
             "newer requirement ignored actual hull/domain profile");
      newer = fallback;
      newer.graphics_requirements.minimum_dxil_shader_model = {6, 1};
      Reject([&] { (void)device->CreatePipeline(newer); },
             "newer requirement ignored selected fallback vertex profile");
    }
  }
#endif
  for (const auto format : {rhi::TextureFormat::Rgba8Unorm, rhi::TextureFormat::Bgra8Unorm}) {
    patch.color_format = fallback.color_format = format;
    auto ordinary = fallback;
    ordinary.graphics_shader = rhi::GraphicsShaderVariant::Triangle;
    const auto baked = device->CreatePipeline(fallback),
               baseline = device->CreatePipeline(ordinary);
    const auto actual = native ? device->CreatePipeline(patch) : baked;
    const auto target = device->CreateTexture(
        {64, 64, format, rhi::ResourceState::ShaderRead, "curved patch pixels"});
    const auto clear = Draw(*device, target, baked, 0),
               reference = Draw(*device, target, baked, 48);
    Require(reference != clear, "baked patch output is empty");
    Require(Draw(*device, target, baseline, 3) != reference,
            "curved patch fixture is indistinguishable from the ordinary triangle");
    Same(Draw(*device, target, actual, native ? 3 : 48), reference);
    {
      auto abandoned = device->CreateCommandList(rhi::QueueType::Graphics);
      abandoned->Transition(
          {target, rhi::ResourceState::ShaderRead, rhi::ResourceState::RenderTarget});
      abandoned->BeginRendering({target, 64, 64});
      abandoned->BindPipeline(baked);
      Reject([&] { abandoned->Draw(3); }, "native baked vertex range was not checked");
      Reject([&] { abandoned->DrawIndirect(1); }, "native baked indirect path was not rejected");
      if (native) {
        abandoned->BindPipeline(actual);
        Reject([&] { abandoned->Draw(48); }, "native patch vertex range was not checked");
      }
      abandoned->EndRendering();
    }
    Same(Read(*device, target), reference);
    TestGraph(*device, actual, native ? 3 : 48, baked, reference, format);
    device->DestroyTexture(target);
    if (native)
      device->DestroyPipeline(actual);
    device->DestroyPipeline(baked);
    device->DestroyPipeline(baseline);
  }
  std::cout << "Native curved patch backend=" << static_cast<unsigned>(backend)
            << " native=" << native << " software_flag=" << caps.software_device
            << "; RGBA/BGRA baked parity, changed geometry, two-worker cancellation/failure/replay "
               "passed\n";
}
} // namespace
int main() {
  try {
    TestPortable();
    TestNative();
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
