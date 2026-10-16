#include "Nexora/Renderer/RenderGraph.h"

#include <algorithm>
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

void RenderGraph::Execute(rhi::Device &device) {
  if (!compiled_)
    throw std::logic_error("render graph must be compiled before execution");
  if (std::ranges::any_of(textures_, [](const auto &texture) { return texture.external; }) ||
      std::ranges::any_of(
          passes_, [](const auto &pass) { return static_cast<bool>(pass.external_execute); }))
    throw std::logic_error("external graph requires its owner executor");
  statistics_.completed_pass_count = 0;
  std::vector<rhi::TextureHandle> handles(textures_.size());
  statistics_.barrier_count = 0;
  statistics_.queue_transfer_count = 0;
  statistics_.buffer_barrier_count = 0;
  statistics_.buffer_queue_transfer_count = 0;
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
    for (const auto pass_index : execution_order_) {
      const auto &pass = passes_[pass_index];
      auto commands = device.CreateCommandList(pass.queue);
      for (const auto &barrier : texture_barriers_[pass_index]) {
        commands->Transition({handles[barrier.texture.id], barrier.before, barrier.after,
                              barrier.source_queue, barrier.destination_queue});
        ++statistics_.barrier_count;
        if (barrier.source_queue != barrier.destination_queue)
          ++statistics_.queue_transfer_count;
      }
      for (const auto &barrier : buffer_barriers_[pass_index]) {
        commands->TransitionBuffer(barrier);
        ++statistics_.buffer_barrier_count;
        if (barrier.source_queue != barrier.destination_queue)
          ++statistics_.buffer_queue_transfer_count;
      }
      pass.execute(*commands, handles);
      device.Submit(*commands);
      ++statistics_.completed_pass_count;
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
