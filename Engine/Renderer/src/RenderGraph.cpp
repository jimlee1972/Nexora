#include "Nexora/Renderer/RenderGraph.h"

#include <algorithm>
#include <exception>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <unordered_set>

namespace nexora::renderer {
namespace {
std::uint64_t EdgeKey(std::size_t before, std::size_t after) {
  return (static_cast<std::uint64_t>(before) << 32U) | static_cast<std::uint64_t>(after);
}
} // namespace

GraphBuffer RenderGraph::ImportBuffer(rhi::BufferHandle buffer, rhi::BufferAccess initial_access,
                                      rhi::QueueType initial_queue) {
  if (!buffer.IsValid() || !rhi::IsValidBufferAccess(initial_access) ||
      !rhi::IsValidQueue(initial_queue))
    throw std::invalid_argument("invalid imported buffer");
  if (std::ranges::any_of(buffers_, [&](const auto &record) { return record.imported == buffer; }))
    throw std::invalid_argument("buffer already imported; reuse its graph ID");
  buffers_.push_back({buffer, initial_access, initial_queue});
  compiled_ = false;
  return {static_cast<std::uint32_t>(buffers_.size() - 1)};
}

GraphTexture RenderGraph::ImportTexture(rhi::TextureHandle texture,
                                        const rhi::TextureDescriptor &descriptor) {
  if (!texture.IsValid())
    throw std::invalid_argument("imported texture handle is invalid");
  if (std::ranges::any_of(textures_, [&](const auto &record) {
        return !record.transient && !record.external && record.imported == texture;
      }))
    throw std::invalid_argument("texture already imported; reuse its graph ID");
  textures_.push_back({descriptor, texture, false, 0, 0});
  compiled_ = false;
  return {static_cast<std::uint32_t>(textures_.size() - 1)};
}
GraphTexture RenderGraph::CreateTransientTexture(const rhi::TextureDescriptor &descriptor) {
  if (descriptor.width == 0 || descriptor.height == 0)
    throw std::invalid_argument("invalid transient texture extent");
  textures_.push_back({descriptor, {}, true, 0, 0});
  compiled_ = false;
  return {static_cast<std::uint32_t>(textures_.size() - 1)};
}
GraphTexture RenderGraph::ImportExternalTexture(const rhi::TextureDescriptor &descriptor) {
  if (descriptor.width == 0 || descriptor.height == 0)
    throw std::invalid_argument("invalid external texture extent");
  textures_.push_back({descriptor, {}, false, 0, 0, true});
  compiled_ = false;
  return {static_cast<std::uint32_t>(textures_.size() - 1)};
}
std::size_t RenderGraph::AddExternalPass(std::string name, std::vector<TextureUse> reads,
                                         std::vector<TextureUse> writes,
                                         std::function<void(const ExternalPassContext &)> execute) {
  return AddPass({std::move(name),
                  rhi::QueueType::Graphics,
                  std::move(reads),
                  std::move(writes),
                  {},
                  std::move(execute)});
}
std::size_t RenderGraph::AddPass(PassDescriptor descriptor) {
  if (descriptor.name.empty() ||
      static_cast<bool>(descriptor.execute) == static_cast<bool>(descriptor.external_execute))
    throw std::invalid_argument("invalid render pass");
  if (!rhi::IsValidQueue(descriptor.queue))
    throw std::invalid_argument("invalid pass queue");
  if (descriptor.external_execute &&
      (!descriptor.buffer_reads.empty() || !descriptor.buffer_writes.empty()))
    throw std::invalid_argument("external passes cannot declare RHI buffers");
  std::unordered_set<std::uint32_t> buffer_uses;
  const auto validate_buffer = [&](const BufferUse &use, bool write) {
    if (use.buffer.id >= buffers_.size() || !buffer_uses.insert(use.buffer.id).second)
      throw std::invalid_argument("unknown or duplicate pass buffer");
    if (write ? use.access != rhi::BufferAccess::StorageReadWrite
              : (use.access != rhi::BufferAccess::ShaderRead &&
                 use.access != rhi::BufferAccess::IndirectRead))
      throw std::invalid_argument("invalid pass buffer access");
  };
  for (const auto &use : descriptor.buffer_reads)
    validate_buffer(use, false);
  for (const auto &use : descriptor.buffer_writes)
    validate_buffer(use, true);
  const auto validate = [this, &descriptor](const TextureUse &use) {
    if (use.texture.id >= textures_.size())
      throw std::invalid_argument("pass references unknown texture");
    if (textures_[use.texture.id].external != static_cast<bool>(descriptor.external_execute))
      throw std::invalid_argument("pass execution mode does not match texture ownership");
  };
  if (descriptor.external_execute && descriptor.queue != rhi::QueueType::Graphics)
    throw std::invalid_argument("external passes require the owner graphics queue");
  std::ranges::for_each(descriptor.reads, validate);
  std::ranges::for_each(descriptor.writes, validate);
  std::unordered_set<std::uint32_t> uses;
  for (const auto &read : descriptor.reads) {
    if (!uses.insert(read.texture.id).second)
      throw std::invalid_argument("pass declares a texture more than once");
  }
  for (const auto &write : descriptor.writes) {
    if (!uses.insert(write.texture.id).second)
      throw std::invalid_argument("pass cannot read and write the same texture");
  }
  passes_.push_back(std::move(descriptor));
  explicit_edges_.emplace_back();
  compiled_ = false;
  return passes_.size() - 1;
}
void RenderGraph::AddDependency(std::size_t before, std::size_t after) {
  if (before >= passes_.size() || after >= passes_.size() || before == after) {
    throw std::invalid_argument("invalid render pass dependency");
  }
  explicit_edges_[before].push_back(after);
  compiled_ = false;
}
void RenderGraph::Compile() {
  compiled_ = false;
  std::vector<std::vector<std::size_t>> edges = explicit_edges_;
  std::unordered_set<std::uint64_t> unique_edges;
  for (std::size_t before = 0; before < edges.size(); ++before) {
    for (const auto after : edges[before])
      unique_edges.insert(EdgeKey(before, after));
  }
  // Track all readers since the last write: a queue/state handoff must wait for each.
  struct Reader {
    std::size_t pass;
    std::uint8_t state;
    rhi::QueueType queue;
  };
  struct Dependencies {
    std::optional<std::size_t> writer;
    std::vector<Reader> readers;
  };
  std::vector<Dependencies> texture_dependencies(textures_.size());
  std::vector<Dependencies> buffer_dependencies(buffers_.size());
  const auto track = [&](Dependencies &dependency, std::size_t pass, std::uint8_t state,
                         bool write) {
    if (dependency.writer)
      unique_edges.insert(EdgeKey(*dependency.writer, pass));
    for (const auto &reader : dependency.readers)
      if (write || reader.state != state || reader.queue != passes_[pass].queue)
        unique_edges.insert(EdgeKey(reader.pass, pass));
    if (write) {
      dependency.readers.clear();
      dependency.writer = pass;
    } else {
      dependency.readers.push_back({pass, state, passes_[pass].queue});
    }
  };
  for (std::size_t pass = 0; pass < passes_.size(); ++pass) {
    for (const auto &use : passes_[pass].reads)
      track(texture_dependencies[use.texture.id], pass, static_cast<std::uint8_t>(use.state),
            false);
    for (const auto &use : passes_[pass].writes)
      track(texture_dependencies[use.texture.id], pass, static_cast<std::uint8_t>(use.state), true);
    for (const auto &use : passes_[pass].buffer_reads)
      track(buffer_dependencies[use.buffer.id], pass, static_cast<std::uint8_t>(use.access), false);
    for (const auto &use : passes_[pass].buffer_writes)
      track(buffer_dependencies[use.buffer.id], pass, static_cast<std::uint8_t>(use.access), true);
  }
  edges.assign(passes_.size(), {});
  std::vector<std::size_t> indegree(passes_.size());
  for (const auto key : unique_edges) {
    const auto before = static_cast<std::size_t>(key >> 32U);
    const auto after = static_cast<std::size_t>(key & 0xFFFFFFFFULL);
    if (before >= passes_.size() || after >= passes_.size())
      throw std::logic_error("invalid graph edge");
    edges[before].push_back(after);
    ++indegree[after];
  }
  std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<>> ready;
  for (std::size_t pass = 0; pass < passes_.size(); ++pass)
    if (indegree[pass] == 0)
      ready.push(pass);
  execution_order_.clear();
  while (!ready.empty()) {
    const auto pass = ready.top();
    ready.pop();
    execution_order_.push_back(pass);
    for (const auto dependent : edges[pass])
      if (--indegree[dependent] == 0)
        ready.push(dependent);
  }
  if (execution_order_.size() != passes_.size())
    throw std::logic_error("render graph contains a cycle");

  for (auto &texture : textures_) {
    texture.first_use = std::numeric_limits<std::size_t>::max();
    texture.last_use = 0;
  }
  for (std::size_t order = 0; order < execution_order_.size(); ++order) {
    const auto &pass = passes_[execution_order_[order]];
    const auto mark = [this, order](const TextureUse &use) {
      auto &texture = textures_[use.texture.id];
      texture.first_use = std::min(texture.first_use, order);
      texture.last_use = order;
    };
    std::ranges::for_each(pass.reads, mark);
    std::ranges::for_each(pass.writes, mark);
  }
  predecessors_.assign(passes_.size(), {});
  for (std::size_t before = 0; before < edges.size(); ++before)
    for (const auto after : edges[before])
      predecessors_[after].push_back(before);
  texture_barriers_.assign(passes_.size(), {});
  buffer_barriers_.assign(passes_.size(), {});
  std::vector<rhi::ResourceState> states;
  std::vector<std::optional<rhi::QueueType>> owners;
  for (const auto &texture : textures_) {
    states.push_back(texture.descriptor.initial_state);
    owners.push_back(texture.transient ? std::nullopt : std::optional{rhi::QueueType::Graphics});
  }
  auto buffer_states = buffers_;
  std::vector<bool> buffer_used(buffers_.size());
  for (const auto pass_index : execution_order_) {
    const auto &pass = passes_[pass_index];
    const auto texture_transition = [&](const TextureUse &use) {
      const auto id = use.texture.id;
      if (states[id] != use.state || (owners[id] && *owners[id] != pass.queue))
        texture_barriers_[pass_index].push_back(
            {use.texture, states[id], use.state, owners[id].value_or(pass.queue), pass.queue});
      states[id] = use.state;
      owners[id] = pass.queue;
    };
    std::ranges::for_each(pass.reads, texture_transition);
    std::ranges::for_each(pass.writes, texture_transition);
    const auto buffer_transition = [&](const BufferUse &use) {
      auto &state = buffer_states[use.buffer.id];
      if (!buffer_used[use.buffer.id] || state.initial_access != use.access ||
          state.initial_queue != pass.queue ||
          state.initial_access == rhi::BufferAccess::StorageReadWrite)
        buffer_barriers_[pass_index].push_back(
            {state.imported, state.initial_access, use.access, state.initial_queue, pass.queue});
      buffer_used[use.buffer.id] = true;
      state.initial_access = use.access;
      state.initial_queue = pass.queue;
    };
    std::ranges::for_each(pass.buffer_reads, buffer_transition);
    std::ranges::for_each(pass.buffer_writes, buffer_transition);
  }
  statistics_ = {passes_.size(),
                 static_cast<std::size_t>(std::ranges::count_if(
                     textures_, [](const TextureRecord &t) { return t.transient; })),
                 0, 0};
  compiled_ = true;
}

const char *RenderGraphCancelled::what() const noexcept {
  return "render graph recording cancelled";
}

void RenderGraph::Execute(rhi::Device &device) { Execute(device, {}); }

void RenderGraph::Execute(rhi::Device &device, const ExecutionOptions &options) {
  if (!compiled_)
    throw std::logic_error("render graph must be compiled before execution");
  if (std::ranges::any_of(textures_, [](const auto &texture) { return texture.external; }) ||
      std::ranges::any_of(
          passes_, [](const auto &pass) { return static_cast<bool>(pass.external_execute); }))
    throw std::logic_error("external graph requires its owner executor");
  if (options.recording_mode != RecordingMode::Serial &&
      options.recording_mode != RecordingMode::Parallel)
    throw std::invalid_argument("invalid command recording mode");
  if (options.max_parallel_passes == 0)
    throw std::invalid_argument("parallel batch limit must be nonzero");
  statistics_.completed_pass_count = 0;
  statistics_.parallel_recorded_pass_count = 0;
  statistics_.parallel_batch_count = 0;
  statistics_.serial_recorded_pass_count = 0;
  statistics_.parallel_fallback_pass_count = 0;
  statistics_.max_parallel_batch_size = 0;
  statistics_.barrier_count = 0;
  statistics_.queue_transfer_count = 0;
  statistics_.buffer_barrier_count = 0;
  statistics_.buffer_queue_transfer_count = 0;
  const auto check_cancellation = [&] {
    if (options.cancellation.IsCancellationRequested())
      throw RenderGraphCancelled{};
  };
  // A pre-cancelled call makes no RHI allocation, recording, submission or GPU wait.
  check_cancellation();
  std::vector<rhi::TextureHandle> handles(textures_.size());
  const auto release_transients = [&] {
    for (std::size_t index = 0; index < textures_.size(); ++index) {
      if (textures_[index].transient && handles[index].IsValid())
        device.DestroyTexture(handles[index]);
    }
  };
  try {
    for (std::size_t index = 0; index < textures_.size(); ++index) {
      // An unused transient has no lifetime and must not consume a GPU allocation.
      if (textures_[index].transient &&
          textures_[index].first_use != std::numeric_limits<std::size_t>::max()) {
        handles[index] = device.CreateTexture(textures_[index].descriptor);
      } else if (!textures_[index].transient) {
        handles[index] = textures_[index].imported;
      }
    }
    struct Recorded final {
      std::unique_ptr<rhi::CommandList> commands;
      std::size_t texture_barriers{};
      std::size_t texture_transfers{};
      std::size_t buffer_barriers{};
      std::size_t buffer_transfers{};
      bool succeeded{};
    };
    const auto record = [&](std::size_t index, Recorded &result) {
      for (const auto &barrier : texture_barriers_[index]) {
        result.commands->Transition({handles[barrier.texture.id], barrier.before, barrier.after,
                                     barrier.source_queue, barrier.destination_queue});
        ++result.texture_barriers;
        if (barrier.source_queue != barrier.destination_queue)
          ++result.texture_transfers;
      }
      for (const auto &barrier : buffer_barriers_[index]) {
        result.commands->TransitionBuffer(barrier);
        ++result.buffer_barriers;
        if (barrier.source_queue != barrier.destination_queue)
          ++result.buffer_transfers;
      }
      passes_[index].execute(*result.commands, handles);
      result.succeeded = true;
    };
    const auto account = [&](const Recorded &result) {
      statistics_.barrier_count += result.texture_barriers;
      statistics_.queue_transfer_count += result.texture_transfers;
      statistics_.buffer_barrier_count += result.buffer_barriers;
      statistics_.buffer_queue_transfer_count += result.buffer_transfers;
    };
    const bool requested = options.recording_mode == RecordingMode::Parallel;
    // Snapshot lifecycle-dependent pool metadata on the coordinator. The caller serializes
    // Start/Stop and keeps the pool, device and resources alive until Execute returns.
    const auto worker_limit =
        requested && options.recording_jobs && !options.recording_jobs->IsWorkerThread()
            ? std::min(options.max_parallel_passes, options.recording_jobs->WorkerCount())
            : 0;
    const bool shared_reads =
        worker_limit > 1 && device.Capabilities().shared_buffer_read_recording;
    const auto eligible = [&](std::size_t index) {
      const auto &pass = passes_[index];
      if (worker_limit < 2 || !pass.allow_parallel_recording)
        return false;
      const auto capabilities = device.Capabilities();
      if (pass.queue == rhi::QueueType::Compute)
        return capabilities.independent_compute_recording && pass.reads.empty() &&
               pass.writes.empty() && (!pass.buffer_reads.empty() || !pass.buffer_writes.empty());
      return capabilities.independent_graphics_recording &&
             pass.queue == rhi::QueueType::Graphics && pass.reads.empty() && !pass.writes.empty() &&
             pass.buffer_writes.empty() &&
             std::ranges::all_of(pass.writes, [](const TextureUse &use) {
               return use.state == rhi::ResourceState::RenderTarget;
             });
    };
    for (std::size_t order = 0; order < execution_order_.size();) {
      check_cancellation();
      std::size_t end = order;
      std::unordered_set<std::uint32_t> batch_buffers;
      std::unordered_set<std::uint32_t> batch_buffer_writes;
      std::unordered_set<std::uint32_t> batch_textures;
      while (end < execution_order_.size() && end - order < worker_limit &&
             eligible(execution_order_[end])) {
        const auto index = execution_order_[end];
        const auto &pass = passes_[index];
        const auto overlaps = [&](const BufferUse &use) {
          return batch_buffers.contains(use.buffer.id);
        };
        const auto read_overlaps = [&](const BufferUse &use) {
          return shared_reads ? batch_buffer_writes.contains(use.buffer.id)
                              : batch_buffers.contains(use.buffer.id);
        };
        const auto texture_overlaps = [&](const TextureUse &use) {
          return batch_textures.contains(use.texture.id);
        };
        bool dependent = false;
        for (auto earlier = order; earlier < end; ++earlier)
          if (std::ranges::find(predecessors_[index], execution_order_[earlier]) !=
              predecessors_[index].end())
            dependent = true;
        if (dependent || std::ranges::any_of(pass.buffer_reads, read_overlaps) ||
            std::ranges::any_of(pass.buffer_writes, overlaps) ||
            std::ranges::any_of(pass.reads, texture_overlaps) ||
            std::ranges::any_of(pass.writes, texture_overlaps))
          break;
        for (const auto &use : pass.buffer_reads)
          batch_buffers.insert(use.buffer.id);
        for (const auto &use : pass.buffer_writes) {
          batch_buffers.insert(use.buffer.id);
          batch_buffer_writes.insert(use.buffer.id);
        }
        for (const auto &use : pass.reads)
          batch_textures.insert(use.texture.id);
        for (const auto &use : pass.writes)
          batch_textures.insert(use.texture.id);
        ++end;
      }
      if (end - order < 2) {
        const auto index = execution_order_[order];
        Recorded result;
        result.commands = device.CreateCommandList(passes_[index].queue);
        try {
          record(index, result);
        } catch (...) {
          account(result);
          throw;
        }
        account(result);
        ++statistics_.serial_recorded_pass_count;
        if (requested)
          ++statistics_.parallel_fallback_pass_count;
        check_cancellation();
        try {
          const auto completion = device.Submit(*result.commands);
          device.WaitForSubmission(completion);
        } catch (...) {
          device.WaitIdle();
          throw;
        }
        ++statistics_.completed_pass_count;
        ++order;
        continue;
      }
      const auto count = end - order;
      std::vector<Recorded> recorded(count);
      std::vector<core::JobHandle> jobs;
      jobs.reserve(count); // No allocation may lose a handle after its job starts.
      // Allocate all command allocators/lists on the coordinator, then give each job one owner.
      for (std::size_t offset = 0; offset < count; ++offset)
        recorded[offset].commands =
            device.CreateCommandList(passes_[execution_order_[order + offset]].queue);
      ++statistics_.parallel_batch_count;
      statistics_.max_parallel_batch_size = std::max(statistics_.max_parallel_batch_size, count);
      std::exception_ptr failure;
      try {
        for (std::size_t offset = 0; offset < count; ++offset) {
          const auto index = execution_order_[order + offset];
          jobs.push_back(
              options.recording_jobs->Submit({[&, offset, index](const core::CancellationToken &) {
                                                record(index, recorded[offset]);
                                              },
                                              core::JobPriority::Normal,
                                              {},
                                              passes_[index].name}));
        }
      } catch (...) {
        failure = std::current_exception();
      }
      // A callback or Submit failure cannot release resources while any started job still uses
      // them. Wait in graph order; choose the earliest callback error ahead of a later scheduling
      // error.
      std::exception_ptr callback_failure;
      for (const auto &job : jobs) {
        try {
          options.recording_jobs->Wait(job);
        } catch (...) {
          if (!callback_failure)
            callback_failure = std::current_exception();
        }
      }
      for (const auto &result : recorded) {
        account(result);
        if (result.succeeded)
          ++statistics_.parallel_recorded_pass_count;
      }
      if (callback_failure)
        std::rethrow_exception(callback_failure);
      if (failure)
        std::rethrow_exception(failure);
      // Recording jobs are never themselves cancelled: every started callback joins, and
      // callback/scheduler failures take precedence over a cancellation request.
      check_cancellation();
      // No worker submits. Keep lists/pools alive through completion, then destroy on the
      // coordinator.
      try {
        // Once accepted, finish this entire batch; observe cancellation before the next one.
        for (auto &result : recorded) {
          const auto completion = device.Submit(*result.commands);
          device.WaitForSubmission(completion);
          ++statistics_.completed_pass_count;
        }
      } catch (...) {
        // A submission can fail after native work was accepted. Drain before freeing allocators.
        device.WaitIdle();
        throw;
      }
      order = end;
    }
    device.WaitIdle();
    release_transients();
  } catch (...) {
    device.WaitIdle();
    release_transients();
    throw;
  }
}
void RenderGraph::ExecuteExternal() {
  if (!compiled_)
    throw std::logic_error("render graph must be compiled before execution");
  if (!buffers_.empty() ||
      std::ranges::any_of(textures_, [](const auto &texture) { return !texture.external; }) ||
      std::ranges::any_of(passes_, [](const auto &pass) { return !pass.external_execute; }))
    throw std::logic_error("external executor cannot own RHI textures or passes");
  statistics_.barrier_count = 0; // GPU barrier emission belongs to the external owner.
  statistics_.queue_transfer_count = 0;
  statistics_.external_transition_count = 0;
  statistics_.completed_pass_count = 0;
  for (const auto index : execution_order_) {
    const auto &pass = passes_[index];
    std::vector<ExternalTextureTransition> transitions;
    for (const auto &barrier : texture_barriers_[index])
      transitions.push_back({barrier.texture, barrier.before, barrier.after});
    pass.external_execute({pass.name, pass.reads, pass.writes, transitions});
    statistics_.external_transition_count += transitions.size();
    ++statistics_.completed_pass_count;
  }
}

} // namespace nexora::renderer
