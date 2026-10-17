#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Renderer/RenderGraph.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <typename Function> void Reject(Function function) {
  bool rejected = false;
  try {
    function();
  } catch (const std::logic_error &) {
    rejected = true;
  }
  Require(rejected, "invalid/stale texture operation reached native execution");
}
struct Target final {
  rhi::TextureHandle handle;
  rhi::TextureDescriptor descriptor;
};
Target MakeTarget(rhi::Device &device, std::uint32_t index) {
  rhi::TextureDescriptor descriptor{32 + index * 2, 48 + index,
                                    index % 2 ? rhi::TextureFormat::Bgra8Unorm
                                              : rhi::TextureFormat::Rgba8Unorm,
                                    rhi::ResourceState::ShaderRead, "isolated graphics target"};
  return {device.CreateTexture(descriptor), descriptor};
}
std::vector<std::byte> Read(rhi::Device &device, const Target &target) {
  std::vector<std::byte> pixels(target.descriptor.width * target.descriptor.height * 4);
  device.ReadTextureForTesting(target.handle, pixels);
  return pixels;
}
void Render(rhi::CommandList &commands, const Target &target, rhi::PipelineHandle pipeline,
            bool draw = true, bool restore = true) {
  commands.Transition(
      {target.handle, rhi::ResourceState::ShaderRead, rhi::ResourceState::RenderTarget});
  commands.BeginRendering({target.handle, target.descriptor.width, target.descriptor.height});
  if (draw) {
    commands.BindPipeline(pipeline);
    commands.Draw(3);
  }
  commands.EndRendering();
  if (restore)
    commands.Transition(
        {target.handle, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderRead});
}
void TestIndependentGraphics(rhi::Device &device,
                             const std::array<rhi::PipelineHandle, 2> &pipelines) {
  constexpr std::size_t worker_count = 4;
  constexpr std::size_t scopes_per_list = 6;
  std::vector<Target> targets;
  for (std::uint32_t index = 0; index < worker_count * scopes_per_list; ++index)
    targets.push_back(MakeTarget(device, index));
  std::vector<std::vector<std::byte>> reference;
  core::JobSystem jobs{worker_count};
  jobs.Start();
  for (const bool parallel : {false, true}) {
    const auto before = device.Diagnostics();
    std::array<std::unique_ptr<rhi::CommandList>, worker_count> lists;
    for (auto &list : lists)
      list = device.CreateCommandList(rhi::QueueType::Graphics);
    std::mutex mutex;
    std::condition_variable condition;
    std::size_t arrived{};
    const auto record = [&](std::size_t worker) {
      if (parallel) {
        std::unique_lock lock{mutex};
        ++arrived;
        condition.notify_all();
        Require(condition.wait_for(lock, std::chrono::seconds(5),
                                   [&] { return arrived == worker_count; }),
                "graphics recording workers did not overlap");
      }
      for (std::size_t scope = 0; scope < scopes_per_list; ++scope) {
        const auto index = worker * scopes_per_list + scope;
        Render(*lists[worker], targets[index], pipelines[index % 2]);
      }
    };
    if (parallel) {
      std::array<core::JobHandle, worker_count> handles;
      std::exception_ptr failure;
      try {
        for (std::size_t worker = 0; worker < worker_count; ++worker)
          handles[worker] =
              jobs.Submit({[&, worker](const core::CancellationToken &) { record(worker); },
                           core::JobPriority::Normal,
                           {},
                           "isolated graphics"});
      } catch (...) {
        failure = std::current_exception();
      }
      for (const auto &handle : handles) {
        if (!handle.IsValid())
          continue;
        try {
          jobs.Wait(handle);
        } catch (...) {
          if (!failure)
            failure = std::current_exception();
        }
      }
      if (failure)
        std::rethrow_exception(failure);
    } else {
      for (std::size_t worker = 0; worker < worker_count; ++worker)
        record(worker);
    }
    Require(device.Diagnostics().submitted_command_lists == before.submitted_command_lists &&
                device.Diagnostics().readbacks == before.readbacks,
            "graphics recording submitted or read back on a worker");
    // Reverse order proves that no command list relies on another list's recording state.
    for (std::size_t worker = worker_count; worker-- > 0;) {
      device.WaitForSubmission(device.Submit(*lists[worker]));
      lists[worker].reset();
    }
    Require(device.Diagnostics().draw_calls - before.draw_calls == targets.size(),
            "multi-scope command lists lost recorded draws");
    for (std::size_t index = 0; index < targets.size(); ++index) {
      auto pixels = Read(device, targets[index]);
      bool colored = false;
      for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4)
        colored |= pixels[pixel] != std::byte{} || pixels[pixel + 1] != std::byte{} ||
                   pixels[pixel + 2] != std::byte{};
      Require(colored, "graphics reference is an empty clear rather than a triangle");
      if (parallel)
        Require(pixels == reference[index], "parallel graphics pixels differ from serial");
      else
        reference.push_back(std::move(pixels));
    }
  }
  jobs.Stop();
  for (const auto &target : targets)
    device.DestroyTexture(target.handle);
  std::cout << "Vulkan graphics: 24 targets, mixed formats/extents, 6 scopes/list, 4 workers; "
               "24 exact serial/parallel pixel comparisons passed\n";
}
void TestAbandonedAndStale(rhi::Device &device, rhi::PipelineHandle pipeline) {
  const auto target = MakeTarget(device, 0);
  const auto record = [&](bool draw = true, bool restore = true) {
    auto list = device.CreateCommandList(rhi::QueueType::Graphics);
    Render(*list, target, pipeline, draw, restore);
    return list;
  };
  {
    auto clear = record(false);
    device.WaitForSubmission(device.Submit(*clear));
  }
  const auto sentinel = Read(device, target);
  const auto before = device.Diagnostics();
  { auto abandoned = record(true, false); }
  Require(device.Diagnostics().submitted_command_lists == before.submitted_command_lists &&
              Read(device, target) == sentinel,
          "abandoned graphics recording changed committed state/output");
  {
    auto retry = record();
    device.WaitForSubmission(device.Submit(*retry));
  }
  const auto reference = Read(device, target);
  Require(reference != sentinel, "retry did not render after abandoned texture state");
  {
    auto first = record();
    auto stale = record();
    device.WaitForSubmission(device.Submit(*first));
    const auto submitted = device.Diagnostics().submitted_command_lists;
    Reject([&] { (void)device.Submit(*stale); });
    Require(device.Diagnostics().submitted_command_lists == submitted,
            "stale submission reached QueueSubmit");
  }
  {
    auto stale = record();
    Require(Read(device, target) == reference, "readback changed output");
    Reject([&] { (void)device.Submit(*stale); });
    Reject([&] {
      stale->Transition(
          {target.handle, rhi::ResourceState::ShaderRead, rhi::ResourceState::RenderTarget});
    });
  }
  {
    auto invalid = device.CreateCommandList(rhi::QueueType::Graphics);
    invalid->Transition(
        {target.handle, rhi::ResourceState::ShaderRead, rhi::ResourceState::RenderTarget});
    Reject([&] {
      invalid->BeginRendering(
          {target.handle, target.descriptor.width + 1, target.descriptor.height});
    });
  }
  {
    auto stale = record();
    device.DestroyTexture(target.handle);
    Reject([&] { (void)device.Submit(*stale); });
  }
  std::cout << "Vulkan texture snapshots: abandonment/retry, stale submit/readback/handle and "
               "bounded render-area rejection passed\n";
}
void TestGraphFailureReuse(rhi::Device &device, rhi::PipelineHandle pipeline) {
  const auto target = MakeTarget(device, 0);
  {
    auto clear = device.CreateCommandList(rhi::QueueType::Graphics);
    Render(*clear, target, pipeline, false);
    device.WaitForSubmission(device.Submit(*clear));
  }
  const auto sentinel = Read(device, target);
  renderer::RenderGraph graph;
  const auto imported = graph.ImportTexture(target.handle, target.descriptor);
  bool fail = true;
  (void)graph.AddPass({"transactional target",
                       rhi::QueueType::Graphics,
                       {},
                       {{imported, rhi::ResourceState::RenderTarget}},
                       [&](rhi::CommandList &commands, auto) {
                         commands.BeginRendering(
                             {target.handle, target.descriptor.width, target.descriptor.height});
                         commands.BindPipeline(pipeline);
                         commands.Draw(3);
                         commands.EndRendering();
                         if (fail)
                           throw std::runtime_error("injected graphics callback failure");
                       }});
  (void)graph.AddPass({"restore target",
                       rhi::QueueType::Graphics,
                       {{imported, rhi::ResourceState::ShaderRead}},
                       {},
                       [](auto &, auto) {}});
  graph.Compile();
  bool rejected = false;
  try {
    graph.Execute(device);
  } catch (const std::runtime_error &error) {
    rejected = std::string_view(error.what()) == "injected graphics callback failure";
  }
  Require(rejected && graph.GetStatistics().completed_pass_count == 0 &&
              Read(device, target) == sentinel,
          "failed texture graph changed output or swallowed its error");
  fail = false;
  graph.Execute(device);
  const auto reference = Read(device, target);
  Require(reference != sentinel, "failed texture graph could not reuse the imported target");
  graph.Execute(device);
  Require(Read(device, target) == reference, "replayed graphics graph changed its output");
  device.DestroyTexture(target.handle);
}
} // namespace
int main() {
  try {
    const auto *artifact = std::getenv("NEXORA_SLANG_SPIRV_PATH");
    if (!artifact || !*artifact || !rhi::IsBackendAvailable(rhi::Backend::Vulkan)) {
      const auto *required = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
      Require(!(required && *required == '1'), "native Vulkan graphics artifacts/device required");
      std::cout << "SKIP: native Vulkan graphics unavailable\n";
      return 77;
    }
    auto device = rhi::CreateDevice(rhi::Backend::Vulkan);
    const auto layout = rhi::TrianglePipelineLayout().layout_hash;
    const std::array pipelines{
        device->CreatePipeline({layout, 1, rhi::TextureFormat::Rgba8Unorm, "RGBA graphics"}),
        device->CreatePipeline({layout, 1, rhi::TextureFormat::Bgra8Unorm, "BGRA graphics"})};
    TestIndependentGraphics(*device, pipelines);
    TestAbandonedAndStale(*device, pipelines[0]);
    TestGraphFailureReuse(*device, pipelines[0]);
    for (const auto pipeline : pipelines)
      device->DestroyPipeline(pipeline);
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
