// Trace actual renderer commands while the production validation device checks state/lifetime.
namespace {
struct DrawTrace {
  std::uint32_t count{}, first{};
  std::int32_t vertex{};
  std::uint64_t vertex_bytes{}, index_bytes{};
};
class TracedCommands final : public nexora::rhi::CommandList {
public:
  std::unique_ptr<nexora::rhi::CommandList> commands;
  std::vector<DrawTrace> &draws;
  std::uint32_t &pipeline_binds, &vertex_binds, &index_binds;
  std::uint64_t vertex_bytes{}, index_bytes{};
  TracedCommands(std::unique_ptr<nexora::rhi::CommandList> inner, std::vector<DrawTrace> &trace,
                 std::uint32_t &pipelines, std::uint32_t &vertices, std::uint32_t &indices)
      : commands(std::move(inner)), draws(trace), pipeline_binds(pipelines), vertex_binds(vertices),
        index_binds(indices) {}
  void Transition(const nexora::rhi::Barrier &b) override { commands->Transition(b); }
  void BeginRendering(const nexora::rhi::RenderingInfo &i) override { commands->BeginRendering(i); }
  void BindPipeline(nexora::rhi::PipelineHandle p) override {
    ++pipeline_binds;
    commands->BindPipeline(p);
  }
  void BindVertexBuffer(nexora::rhi::BufferHandle b, std::uint64_t o) override {
    ++vertex_binds;
    vertex_bytes = o;
    commands->BindVertexBuffer(b, o);
  }
  void BindIndexBuffer(nexora::rhi::BufferHandle b, nexora::rhi::IndexFormat f,
                       std::uint64_t o) override {
    ++index_binds;
    index_bytes = o;
    commands->BindIndexBuffer(b, f, o);
  }
  void BindTexture(std::uint32_t s, nexora::rhi::TextureHandle t) override {
    commands->BindTexture(s, t);
  }
  void SetScissor(const nexora::rhi::ScissorRect &s) override { commands->SetScissor(s); }
  void Draw(std::uint32_t v, std::uint32_t i) override { commands->Draw(v, i); }
  void DrawIndexed(std::uint32_t n, std::uint32_t instances, std::uint32_t first,
                   std::int32_t vertex, std::uint32_t instance) override {
    draws.push_back({n, first, vertex, vertex_bytes, index_bytes});
    commands->DrawIndexed(n, instances, first, vertex, instance);
  }
  void EndRendering() override { commands->EndRendering(); }
};
class TracedDevice final : public nexora::rhi::Device {
public:
  std::unique_ptr<nexora::rhi::Device> device = nexora::rhi::CreateValidationDevice();
  std::vector<DrawTrace> draws;
  mutable std::uint32_t calls{};
  std::uint32_t pipeline_binds{}, vertex_binds{}, index_binds{};
  nexora::rhi::Backend GetBackend() const noexcept override { return device->GetBackend(); }
  nexora::rhi::TextureHandle CreateTexture(const nexora::rhi::TextureDescriptor &d) override {
    ++calls;
    return device->CreateTexture(d);
  }
  void WriteTextureRgba8(nexora::rhi::TextureHandle t, std::span<const std::byte> p,
                         std::uint32_t pitch) override {
    ++calls;
    device->WriteTextureRgba8(t, p, pitch);
  }
  void DestroyTexture(nexora::rhi::TextureHandle t) override {
    ++calls;
    device->DestroyTexture(t);
  }
  nexora::rhi::BufferHandle CreateBuffer(const nexora::rhi::BufferDescriptor &d) override {
    ++calls;
    return device->CreateBuffer(d);
  }
  void WriteBuffer(nexora::rhi::BufferHandle b, std::uint64_t o,
                   std::span<const std::byte> p) override {
    ++calls;
    device->WriteBuffer(b, o, p);
  }
  void DestroyBuffer(nexora::rhi::BufferHandle b) override {
    ++calls;
    device->DestroyBuffer(b);
  }
  nexora::rhi::PipelineHandle CreatePipeline(const nexora::rhi::PipelineDescriptor &d) override {
    ++calls;
    return device->CreatePipeline(d);
  }
  void DestroyPipeline(nexora::rhi::PipelineHandle p) override {
    ++calls;
    device->DestroyPipeline(p);
  }
  std::unique_ptr<nexora::rhi::CommandList> CreateCommandList(nexora::rhi::QueueType q) override {
    ++calls;
    return std::make_unique<TracedCommands>(device->CreateCommandList(q), draws, pipeline_binds,
                                            vertex_binds, index_binds);
  }
  std::uint64_t Submit(nexora::rhi::CommandList &c) override {
    ++calls;
    return device->Submit(*static_cast<TracedCommands &>(c).commands);
  }
  std::uint64_t CompletedSubmissionValue() const noexcept override {
    ++calls;
    return device->CompletedSubmissionValue();
  }
  void WaitForSubmission(std::uint64_t v) override {
    ++calls;
    device->WaitForSubmission(v);
  }
  void Present(nexora::rhi::TextureHandle t) override {
    ++calls;
    device->Present(t);
  }
  void WaitIdle() override {
    ++calls;
    device->WaitIdle();
  }
  nexora::rhi::DeviceDiagnostics Diagnostics() const noexcept override {
    return device->Diagnostics();
  }
};
void CountUnexpectedCallback(const ImDrawList *, const ImDrawCmd *c) {
  ++*static_cast<unsigned *>(c->UserCallbackData);
}
void TestRenderDrawContract() {
  using namespace nexora::editor::imgui;
  EditorImGuiHost host;
  host.SetDisplay(640, 480, 1);
  assert((ImGui::GetIO().BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset) != 0);
  TracedDevice device;
  const auto target =
      device.CreateTexture({640, 480, nexora::rhi::TextureFormat::Rgba8Unorm,
                            nexora::rhi::ResourceState::Undefined, "Draw contract"});
  host.BeginFrame();
  auto *background = ImGui::GetBackgroundDrawList();
  background->AddRectFilled({1, 1}, {5, 5}, IM_COL32_WHITE);
  auto *list = ImGui::GetForegroundDrawList();
  for (unsigned i = 0; i < 17000; ++i)
    list->AddRectFilled({10, 10}, {20, 20}, IM_COL32_WHITE);
  list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
  list->AddRectFilled({30, 10}, {40, 20}, IM_COL32_WHITE);
  const auto frame = host.EndFrame();
  assert(frame.vertices > 65535);
  const auto *data = ImGui::GetDrawData();
  std::vector<DrawTrace> expected;
  std::uint64_t vertex_bytes = 0, index_bytes = 0;
  bool large_offset = false;
  for (const auto *draw_list : data->CmdLists) {
    for (const auto &command : draw_list->CmdBuffer) {
      if (command.UserCallback || !command.ElemCount)
        continue;
      expected.push_back({command.ElemCount, command.IdxOffset,
                          static_cast<std::int32_t>(command.VtxOffset), vertex_bytes, index_bytes});
      large_offset |= command.VtxOffset != 0;
      // Every emitted local index must resolve within this actual source list, including the
      // second range after the 16-bit rollover. This oracle does not use renderer output.
      for (unsigned j = 0; j < command.ElemCount; ++j)
        assert(command.VtxOffset + draw_list->IdxBuffer[command.IdxOffset + j] <
               static_cast<unsigned>(draw_list->VtxBuffer.Size));
    }
    vertex_bytes += draw_list->VtxBuffer.Size * sizeof(ImDrawVert);
    index_bytes += draw_list->IdxBuffer.Size * sizeof(ImDrawIdx);
  }
  assert(large_offset && data->CmdListsCount == 2);
  for (unsigned repeat = 0; repeat < 2; ++repeat) {
    device.draws.clear();
    const auto pipelines = device.pipeline_binds;
    const auto vertices = device.vertex_binds;
    const auto indices = device.index_binds;
    assert(host.Render(device, target, 640, 480,
                       repeat == 0 ? nexora::rhi::ResourceState::Undefined
                                   : nexora::rhi::ResourceState::ShaderRead,
                       false) == expected.size());
    assert(device.draws.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
      const auto &actual = device.draws[i], &e = expected[i];
      assert(actual.count == e.count && actual.first == e.first && actual.vertex == e.vertex &&
             actual.vertex_bytes == e.vertex_bytes && actual.index_bytes == e.index_bytes);
    }
    assert(device.pipeline_binds == pipelines + 2 && device.vertex_binds == vertices + 3 &&
           device.index_binds == indices + 3);
  }
  unsigned callback_calls = 0;
  for (bool callback_only : {false, true}) {
    host.BeginFrame();
    auto *callbacks = ImGui::GetForegroundDrawList();
    if (!callback_only)
      callbacks->AddRectFilled({10, 10}, {20, 20}, IM_COL32_WHITE);
    callbacks->AddCallback(CountUnexpectedCallback, &callback_calls);
    if (!callback_only)
      callbacks->AddRectFilled({30, 10}, {40, 20}, IM_COL32_WHITE);
    static_cast<void>(host.EndFrame());
    const auto calls = device.calls;
    const auto metrics = host.GetRendererMetrics();
    const auto draw_count = device.draws.size();
    assert(host.Render(device, target, 640, 480, nexora::rhi::ResourceState::ShaderRead, false) ==
           0);
    assert(device.calls == calls && device.draws.size() == draw_count && callback_calls == 0);
    const auto after = host.GetRendererMetrics();
    assert(after.frames == metrics.frames && after.draw_calls == metrics.draw_calls &&
           after.buffer_reallocations == metrics.buffer_reallocations &&
           after.font_rebuilds == metrics.font_rebuilds &&
           after.rejected_textures == metrics.rejected_textures &&
           after.completion_waits == metrics.completion_waits);
  }
  assert(device.Diagnostics().validation_errors == 0);
  host.ReleaseRenderer(device);
  device.DestroyTexture(target);
}
} // namespace
