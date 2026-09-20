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

GraphTexture RenderGraph::ImportTexture(rhi::TextureHandle texture,
                                        const rhi::TextureDescriptor &descriptor) {
  if (!texture.IsValid())
    throw std::invalid_argument("imported texture handle is invalid");
  textures_.push_back({descriptor, texture, false, 0, 0});
  compiled_ = false;
  return {static_cast<std::uint32_t>(textures_.size() - 1)};
}
GraphTexture RenderGraph::CreateTransientTexture(const rhi::TextureDescriptor &descriptor) {
  textures_.push_back({descriptor, {}, true, 0, 0});
  compiled_ = false;
  return {static_cast<std::uint32_t>(textures_.size() - 1)};
}
std::size_t RenderGraph::AddPass(PassDescriptor descriptor) {
  if (descriptor.name.empty() || !descriptor.execute)
    throw std::invalid_argument("invalid render pass");
  const auto validate = [this](const TextureUse &use) {
    if (use.texture.id >= textures_.size())
      throw std::invalid_argument("pass references unknown texture");
  };
  std::ranges::for_each(descriptor.reads, validate);
  std::ranges::for_each(descriptor.writes, validate);
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
  std::vector<std::vector<std::size_t>> edges = explicit_edges_;
  std::unordered_set<std::uint64_t> unique_edges;
  for (std::size_t before = 0; before < edges.size(); ++before) {
    for (const auto after : edges[before])
      unique_edges.insert(EdgeKey(before, after));
  }
  std::vector<std::optional<std::size_t>> last_writer(textures_.size());
  std::vector<std::vector<std::size_t>> readers(textures_.size());
  for (std::size_t pass = 0; pass < passes_.size(); ++pass) {
    for (const auto &read : passes_[pass].reads) {
      const auto writer = last_writer[read.texture.id].value_or(passes_.size());
      if (writer != passes_.size())
        unique_edges.insert(EdgeKey(writer, pass));
      readers[read.texture.id].push_back(pass);
    }
    for (const auto &write : passes_[pass].writes) {
      const auto writer = last_writer[write.texture.id].value_or(passes_.size());
      if (writer != passes_.size())
        unique_edges.insert(EdgeKey(writer, pass));
      for (const auto reader : readers[write.texture.id])
        unique_edges.insert(EdgeKey(reader, pass));
      readers[write.texture.id].clear();
      last_writer[write.texture.id] = pass;
    }
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
  statistics_ = {passes_.size(),
                 static_cast<std::size_t>(std::ranges::count_if(
                     textures_, [](const TextureRecord &t) { return t.transient; })),
                 0};
  compiled_ = true;
}

void RenderGraph::Execute(rhi::Device &device) {
  if (!compiled_)
    throw std::logic_error("render graph must be compiled before execution");
  std::vector<rhi::TextureHandle> handles(textures_.size());
  std::vector<rhi::ResourceState> states(textures_.size());
  for (std::size_t index = 0; index < textures_.size(); ++index) {
    handles[index] = textures_[index].transient ? device.CreateTexture(textures_[index].descriptor)
                                                : textures_[index].imported;
    states[index] = textures_[index].descriptor.initial_state;
  }
  statistics_.barrier_count = 0;
  const auto release_transients = [&] {
    for (std::size_t index = 0; index < textures_.size(); ++index) {
      if (textures_[index].transient && handles[index].IsValid())
        device.DestroyTexture(handles[index]);
    }
  };
  try {
    for (const auto pass_index : execution_order_) {
      const auto &pass = passes_[pass_index];
      auto commands = device.CreateCommandList(pass.queue);
      const auto transition = [&](const TextureUse &use) {
        if (states[use.texture.id] != use.state) {
          commands->Transition({handles[use.texture.id], states[use.texture.id], use.state});
          states[use.texture.id] = use.state;
          ++statistics_.barrier_count;
        }
      };
      std::ranges::for_each(pass.reads, transition);
      std::ranges::for_each(pass.writes, transition);
      pass.execute(*commands, handles);
      device.Submit(*commands);
    }
    device.WaitIdle();
    release_transients();
  } catch (...) {
    device.WaitIdle();
    release_transients();
    throw;
  }
}
} // namespace nexora::renderer
