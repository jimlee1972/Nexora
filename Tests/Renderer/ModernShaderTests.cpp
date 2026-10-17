#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "Nexora/RHI/Capabilities.h"
#include "Nexora/RHI/Device.h"
#include "Nexora/Renderer/PipelineCache.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
using namespace nexora;
namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
template <class F> void Reject(F function) {
  try {
    function();
  } catch (const std::exception &) {
    return;
  }
  throw std::runtime_error("invalid modern program was admitted");
}
constexpr std::uint32_t Sentinel = 0xcdcdcdcd;
void Run(rhi::Device &device, rhi::ComputeShaderVariant kind, std::uint32_t seed) {
  rhi::PipelineDescriptor descriptor;
  descriptor.layout_hash = 31;
  descriptor.shader_hash = 41;
  descriptor.type = rhi::PipelineType::Compute;
  descriptor.compute_shader = kind;
  if (kind != rhi::ComputeShaderVariant::LongVector69)
    Reject([&] { (void)device.CreatePipeline(descriptor); });
  const auto baseline_key = renderer::MakePipelineCacheKey(descriptor);
  descriptor.allow_preview_shader_models = true;
  Require(baseline_key != renderer::MakePipelineCacheKey(descriptor),
          "preview permission aliases cache identity");
  auto pipeline = device.CreatePipeline(descriptor);
  std::array<std::uint32_t, 32> input{};
  std::array<std::uint32_t, 8> expected;
  expected.fill(Sentinel);
  if (kind == rhi::ComputeShaderVariant::LinearAlgebra610) {
    for (std::uint32_t i = 0; i < 16; ++i)
      input[i] = std::bit_cast<std::uint32_t>(static_cast<float>(i + seed) * .25F);
    for (std::uint32_t i = 0; i < 4; ++i)
      input[16 + i] = std::bit_cast<std::uint32_t>(static_cast<float>(i + 1));
    for (std::uint32_t row = 0; row < 4; ++row) {
      double result = 0;
      for (std::uint32_t column = 0; column < 4; ++column)
        result += static_cast<double>(std::bit_cast<float>(input[row * 4 + column])) * (column + 1);
      expected[row] = std::bit_cast<std::uint32_t>(static_cast<float>(result));
    }
  } else {
    for (std::uint32_t i = 0; i < 8; ++i)
      input[i] = seed + i * 3;
    std::uint32_t result = 0;
    if (kind == rhi::ComputeShaderVariant::LongVector69)
      for (std::uint32_t i = 0; i < 8; ++i)
        result += input[i] * (i + 1);
    else
      for (std::uint32_t i = 0; i < 16384; ++i)
        result += input[i % 8] + i;
    expected[0] = result;
  }
  std::array buffers{device.CreateBuffer({128, "modern input"}),
                     device.CreateBuffer({32, "modern output"}),
                     device.CreateBuffer({16, "unused2"}), device.CreateBuffer({16, "unused3"})};
  std::array<std::uint32_t, 8> empty;
  empty.fill(Sentinel);
  device.WriteBuffer(buffers[0], 0, std::as_bytes(std::span{input}));
  device.WriteBuffer(buffers[1], 0, std::as_bytes(std::span{empty}));
  const auto record = [&] {
    auto list = device.CreateCommandList(rhi::QueueType::Compute);
    list->BindPipeline(pipeline);
    for (std::uint32_t slot = 0; slot < 4; ++slot)
      list->BindStorageBuffer(slot, buffers[slot]);
    Reject([&] { list->Dispatch(2, 1, 1); });
    list->Dispatch(1, 1, 1);
    return list;
  };
  { auto abandoned = record(); }
  auto stale = record();
  device.WriteBuffer(buffers[0], 0, std::as_bytes(std::span{input}));
  Reject([&] { (void)device.Submit(*stale); });
  stale.reset();
  const auto small = device.CreateBuffer({4, "invalid modern input"});
  for (const auto bad_input : {buffers[1], small}) {
    auto invalid = device.CreateCommandList(rhi::QueueType::Compute);
    invalid->BindPipeline(pipeline);
    invalid->BindStorageBuffer(0, bad_input);
    for (std::uint32_t slot = 1; slot < 4; ++slot)
      invalid->BindStorageBuffer(slot, buffers[slot]);
    Reject([&] { invalid->Dispatch(1, 1, 1); });
  }
  device.DestroyBuffer(small);
  const auto before = device.Diagnostics();
  auto list = record();
  device.WaitForSubmission(device.Submit(*list));
  Require(device.Diagnostics().compute_dispatches == before.compute_dispatches + 1,
          "rejected or abandoned work published a dispatch");
  std::array<std::uint32_t, 8> actual{};
  device.ReadBufferForTesting(buffers[1], 0, std::as_writable_bytes(std::span{actual}));
  Require(actual == expected, "native modern compute output or canary differs from CPU oracle");
  list.reset();
  for (auto buffer : buffers)
    device.DestroyBuffer(buffer);
  device.DestroyPipeline(pipeline);
  std::cout << "Modern native variant=" << static_cast<unsigned>(kind) << " seed=" << seed
            << "; CPU parity, canary, one-group bound, stale and abandoned recording passed\n";
}
} // namespace
int main() {
  try {
    rhi::DeviceCreationOptions options;
    options.enable_d3d12_preview_runtime = true;
    auto device = rhi::CreateDeviceWithOptions(rhi::Backend::Direct3D12, options);
#if defined(_WIN32)
    std::array<wchar_t, 32768> module_path{}, executable_path{};
    const auto warp = GetModuleHandleW(L"d3d10warp.dll");
    Require(
        warp &&
            GetModuleFileNameW(warp, module_path.data(), static_cast<DWORD>(module_path.size())) &&
            GetModuleFileNameW(nullptr, executable_path.data(),
                               static_cast<DWORD>(executable_path.size())),
        "loaded WARP module identity is unavailable");
    Require(std::filesystem::path(module_path.data()).parent_path() ==
                std::filesystem::path(executable_path.data()).parent_path(),
            "preview execution loaded system WARP instead of the app-local pinned package");
    std::cout << "App-local WARP=" << std::filesystem::path(module_path.data()).string() << '\n';
#endif
#if defined(_WIN32)
    // An omitted caller minimum must not let a modern artifact bypass preview admission.
    const auto *group_path = std::getenv("NEXORA_MODERN_GROUP_SHARED_DXIL_PATH");
    Require(group_path && *group_path, "generic preview negative artifact is unset");
    const auto *previous_compute = std::getenv("NEXORA_SLANG_COMPUTE_DXIL_PATH");
    const bool had_compute = previous_compute != nullptr;
    const std::string saved_compute = previous_compute ? previous_compute : "";
    Require(_putenv_s("NEXORA_SLANG_COMPUTE_DXIL_PATH", group_path) == 0,
            "cannot stage generic preview artifact");
    auto generic_device = rhi::CreateDeviceWithOptions(rhi::Backend::Direct3D12, options);
    Require(_putenv_s("NEXORA_SLANG_COMPUTE_DXIL_PATH", had_compute ? saved_compute.c_str() : "") ==
                0,
            "cannot restore generic compute artifact");
    rhi::PipelineDescriptor generic;
    generic.layout_hash = 101;
    generic.shader_hash = 103;
    generic.type = rhi::PipelineType::Compute;
    Reject([&] { (void)generic_device->CreatePipeline(generic); });
    generic.allow_preview_shader_models = true;
    const auto admitted_generic = generic_device->CreatePipeline(generic);
    generic_device->DestroyPipeline(admitted_generic);
    Require(generic_device->Diagnostics().compute_dispatches == 0,
            "generic artifact admission submitted GPU work");
    generic_device.reset();
    std::cout << "Generic SM6.10 artifact without declared minimum: unopted reject / opted PSO "
                 "admission passed\n";
#endif
    const auto caps = device->Capabilities();
    Require(caps.d3d12_preview_runtime_enabled && caps.software_device &&
                caps.highest_dxil_shader_model >= rhi::FeatureVersion{6, 10},
            "isolated SM6.10 WARP unavailable");
    std::cout << "Preview WARP SM=" << caps.highest_dxil_shader_model.major << '.'
              << caps.highest_dxil_shader_model.minor
              << " shared_bytes=" << caps.maximum_compute_shared_memory_bytes
              << " algebra=" << static_cast<unsigned>(caps.linear_algebra_support)
              << " f32_matvec=" << static_cast<unsigned>(caps.linear_algebra_f32_matvec_support)
              << '\n';
#if defined(_WIN32)
    const auto *correct_path = std::getenv("NEXORA_MODERN_LINEAR_ALGEBRA_DXIL_PATH");
    Require(correct_path && std::getenv("NEXORA_MODERN_LONG_VECTOR_DXIL_PATH") &&
                std::getenv("NEXORA_MODERN_GROUP_SHARED_DXIL_PATH"),
            "negative artifact paths are unset");
    const std::string correct = correct_path;
    rhi::PipelineDescriptor wrong;
    wrong.layout_hash = 73;
    wrong.shader_hash = 79;
    wrong.type = rhi::PipelineType::Compute;
    wrong.compute_shader = rhi::ComputeShaderVariant::LinearAlgebra610;
    wrong.allow_preview_shader_models = true;
    Require(_putenv_s("NEXORA_MODERN_LINEAR_ALGEBRA_DXIL_PATH",
                      std::getenv("NEXORA_MODERN_LONG_VECTOR_DXIL_PATH")) == 0,
            "cannot stage low-profile negative");
    Reject([&] { (void)device->CreatePipeline(wrong); });
    Require(_putenv_s("NEXORA_MODERN_LINEAR_ALGEBRA_DXIL_PATH",
                      std::getenv("NEXORA_MODERN_GROUP_SHARED_DXIL_PATH")) == 0,
            "cannot stage changed-recipe negative");
    Reject([&] { (void)device->CreatePipeline(wrong); });
    Require(_putenv_s("NEXORA_MODERN_LINEAR_ALGEBRA_DXIL_PATH", correct.c_str()) == 0,
            "cannot restore correct artifact");
#endif
    for (const auto kind :
         {rhi::ComputeShaderVariant::LongVector69, rhi::ComputeShaderVariant::LinearAlgebra610,
          rhi::ComputeShaderVariant::GroupShared610})
      for (const auto seed : {1U, 7U, 93U})
        Run(*device, kind, seed);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
