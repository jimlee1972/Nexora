#include "Nexora/Renderer/GPUDrivenPipeline.h"

#include <array>
#include <barrier>
#include <bit>
#include <cstdlib>
#include <exception>
#include <future>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace nexora;
using namespace nexora::renderer;

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

struct Workload {
  std::array<rhi::BufferHandle, 4> buffers;
  GPUDrivenResult reference;
};

Workload MakeWorkload(rhi::Device &device, std::uint32_t identity) {
  GPUSceneReferenceObject object;
  object.object = {identity, 1};
  object.descriptor.world_bounds = {{0, 0, 0.25F}, 0.05F};
  object.descriptor.mesh_resource_index = identity + 7;
  object.descriptor.material_resource_index = identity + 11;
  object.descriptor.lod.lod_count = 1;
  GPUSceneReferenceSnapshot scene;
  scene.objects.push_back(object);
  GPUDrivenView view;
  view.maximum_distance = 10;
  // Canonical packed GPU-driven input: one visible object with a unique identity and bin.
  std::array<std::uint32_t, 56> packed{};
  packed[0] = 1;
  packed[4] = std::bit_cast<std::uint32_t>(view.maximum_distance);
  packed[6] = static_cast<std::uint32_t>(HiZPolicy::Invalidated);
  for (std::size_t index = 0; index < view.view_projection.values.size(); ++index)
    packed[20 + index] = std::bit_cast<std::uint32_t>(view.view_projection.values[index]);
  packed[46] = std::bit_cast<std::uint32_t>(object.descriptor.world_bounds.center.z);
  packed[47] = std::bit_cast<std::uint32_t>(object.descriptor.world_bounds.radius);
  packed[48] = identity;
  packed[49] = 1;
  packed[50] = object.descriptor.mesh_resource_index;
  packed[51] = object.descriptor.material_resource_index;
  packed[52] = object.descriptor.visibility_flags;
  packed[53] = 1;
  Workload result{{device.CreateBuffer({sizeof(packed), "isolated input"}),
                   device.CreateBuffer({5 * sizeof(std::uint32_t), "isolated visible"}),
                   device.CreateBuffer({rhi::GPUDrivenIndirectCommandStride, "isolated indirect"}),
                   device.CreateBuffer({8 * sizeof(std::uint32_t), "isolated counts"})},
                  BuildGPUDrivenCommands(scene, view)};
  device.WriteBuffer(result.buffers[0], 0, std::as_bytes(std::span{packed}));
  Require(result.reference.instances.size() == 1 && result.reference.commands.size() == 1,
          "descriptor isolation fixture must produce a nonempty reference");
  return result;
}

void Bind(rhi::CommandList &commands, const Workload &workload) {
  for (std::uint32_t slot = 0; slot < workload.buffers.size(); ++slot)
    commands.BindStorageBuffer(slot, workload.buffers[slot]);
}

void Compare(rhi::Device &device, const Workload &workload) {
  std::array<std::uint32_t, 5> visible{};
  std::array<std::uint32_t, rhi::GPUDrivenIndirectCommandWords> indirect{};
  std::array<std::uint32_t, 8> counts{};
  device.ReadBufferForTesting(workload.buffers[1], 0, std::as_writable_bytes(std::span{visible}));
  device.ReadBufferForTesting(workload.buffers[2], 0, std::as_writable_bytes(std::span{indirect}));
  device.ReadBufferForTesting(workload.buffers[3], 0, std::as_writable_bytes(std::span{counts}));
  Require(counts[4] == 1 && counts[5] == 1,
          "each descriptor snapshot must execute its own workload");
  GPUDrivenResult output;
  output.statistics = {counts[0], counts[1], counts[2], counts[3]};
  output.instances.push_back({{visible[0], visible[1]}, visible[2], visible[3], visible[4]});
  output.commands.push_back({indirect[4], indirect[5], indirect[6], indirect[7], indirect[8]});
  Require(CompareGPUDrivenResults(workload.reference, output).matches,
          "isolated dispatch output must match its unique CPU reference");
  Require(indirect[0] == 3 && indirect[1] == 1 && indirect[2] == 0 && indirect[3] == 0,
          "isolated dispatch must preserve the native indirect draw prefix");
}

void TestIsolation(rhi::Device &device, rhi::PipelineHandle pipeline, bool parallel) {
  constexpr std::size_t workers = 4;
  // Cross the 32-set pool boundary and rebind all slots after every dispatch.
  constexpr std::size_t dispatches_per_worker = 40;
  std::vector<Workload> workloads;
  for (std::uint32_t index = 0; index < workers * dispatches_per_worker; ++index)
    workloads.push_back(MakeWorkload(device, index + 1));
  std::array<std::unique_ptr<rhi::CommandList>, workers> lists;
  for (auto &list : lists)
    list = device.CreateCommandList(rhi::QueueType::Compute);
  const auto record = [&](std::size_t worker) {
    auto &list = *lists[worker];
    list.BindPipeline(pipeline);
    for (std::size_t index = 0; index < dispatches_per_worker; ++index) {
      Bind(list, workloads[worker * dispatches_per_worker + index]);
      list.Dispatch(1);
    }
    // Binding without dispatch must not retroactively change the final recorded dispatch.
    Bind(list, workloads[(worker * dispatches_per_worker + 1) % workloads.size()]);
  };
  const auto before = device.Diagnostics();
  if (parallel) {
    std::barrier start{static_cast<std::ptrdiff_t>(workers)};
    std::array<std::future<void>, workers> jobs;
    std::size_t launched = 0;
    try {
      for (; launched < workers; ++launched) {
        const auto worker = launched;
        jobs[worker] = std::async(std::launch::async, [&, worker] {
          start.arrive_and_wait();
          record(worker);
        });
      }
    } catch (...) {
      // A failed thread launch must not leave already launched workers at the barrier.
      for (auto missing = launched; missing < workers; ++missing)
        start.arrive_and_drop();
      for (auto &job : jobs)
        if (job.valid())
          job.wait();
      throw;
    }
    // Drain every worker before a failure can release resources captured by other workers.
    std::exception_ptr failure;
    for (auto &job : jobs) {
      try {
        job.get();
      } catch (...) {
        failure = std::current_exception();
      }
    }
    if (failure)
      std::rethrow_exception(failure);
  } else {
    for (std::size_t worker = 0; worker < workers; ++worker)
      record(worker);
  }
  // No list is submitted until all lists have finished recording. Reverse submission order
  // makes the result independent of both recording order and another list's descriptor writes.
  Require(device.Diagnostics().submitted_command_lists == before.submitted_command_lists,
          "recording workers must not submit work");
  Require(device.Diagnostics().readbacks == before.readbacks, "recording must not read back");
  for (std::size_t index = workers; index-- > 0;) {
    const auto completion = device.Submit(*lists[index]);
    device.WaitForSubmission(completion);
    Require(device.CompletedSubmissionValue() >= completion,
            "submission must complete before retirement");
    lists[index].reset();
  }
  Require(device.Diagnostics().compute_dispatches - before.compute_dispatches == workloads.size(),
          "every isolated dispatch must execute exactly once");
  for (const auto &workload : workloads)
    Compare(device, workload);
  for (const auto &workload : workloads)
    for (const auto buffer : workload.buffers)
      device.DestroyBuffer(buffer);
}

void TestRejectedBindings(rhi::Device &device, rhi::PipelineHandle pipeline) {
  const auto rejects = [](auto &&call) {
    try {
      call();
    } catch (const std::logic_error &) {
      return true;
    }
    return false;
  };
  auto list = device.CreateCommandList(rhi::QueueType::Compute);
  list->BindPipeline(pipeline);
  Require(rejects([&] { list->Dispatch(1); }), "dispatch must reject missing storage bindings");
  auto workload = MakeWorkload(device, 999);
  Bind(*list, workload);
  device.DestroyBuffer(workload.buffers[0]);
  Require(rejects([&] { list->Dispatch(1); }), "dispatch must revalidate a stale bound buffer");
  list.reset();
  for (std::size_t slot = 1; slot < workload.buffers.size(); ++slot)
    device.DestroyBuffer(workload.buffers[slot]);
  Require(device.Diagnostics().compute_dispatches == 0, "rejected dispatches must not execute");
}
} // namespace

int main() {
  try {
    const auto *graphics_artifact = std::getenv("NEXORA_SLANG_SPIRV_PATH");
    const auto *compute_artifact = std::getenv("NEXORA_SLANG_COMPUTE_SPIRV_PATH");
    if (!graphics_artifact || !*graphics_artifact || !compute_artifact || !*compute_artifact ||
        !rhi::IsBackendAvailable(rhi::Backend::Vulkan)) {
      const auto *required = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
      Require(!(required && *required == '1'), "native Vulkan required but unavailable");
      std::cout << "SKIP: native Vulkan artifacts/device unavailable\n";
      return 77;
    }
    auto device = rhi::CreateDevice(rhi::Backend::Vulkan);
    const auto pipeline = device->CreatePipeline(
        {1, 2, rhi::TextureFormat::Rgba8Unorm, "isolation", rhi::PipelineType::Compute});
    TestRejectedBindings(*device, pipeline);
    TestIsolation(*device, pipeline, false);
    TestIsolation(*device, pipeline, true);
    device->DestroyPipeline(pipeline);
    std::cout << "Vulkan descriptor isolation: serial and 4-worker recording, 320 exact dispatch "
                 "comparisons passed\n";
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
