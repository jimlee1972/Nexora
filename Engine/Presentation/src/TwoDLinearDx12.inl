// Included inside Dx12Surface; each offscreen owner follows the existing swapchain-slot fence.
private:
struct TwoDLinearFrame {
  std::array<ComPtr<ID3D12Resource>, 2> colors;
  ComPtr<ID3D12DescriptorHeap> rtvs, srvs;
  ComPtr<ID3D12Resource> upload;
  std::size_t capacity{};
  std::uint32_t width{}, height{};
  std::uint64_t allocationBytes{};
  D3D12_RESOURCE_STATES colorState{D3D12_RESOURCE_STATE_RENDER_TARGET};
};
std::array<TwoDLinearFrame, kMaximumFrames> twoDLinearFrames_;
std::array<ComPtr<ID3D12PipelineState>, 5> twoDLinearPipelines_;
std::array<ComPtr<ID3D12RootSignature>, 2> twoDLinearRoots_;
bool CreateTwoDLinearRoots() {
  if (twoDLinearRoots_[0] && twoDLinearRoots_[1])
    return true;
  D3D12_DESCRIPTOR_RANGE range{};
  range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  range.NumDescriptors = 1;
  range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
  D3D12_ROOT_PARAMETER parameters[3]{};
  parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[0].Constants.Num32BitValues = sizeof(TwoDShaderConstants) / 4;
  parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[1].DescriptorTable = {1, &range};
  parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  parameters[2].Descriptor.ShaderRegister = 1;
  parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  D3D12_STATIC_SAMPLER_DESC sampler{};
  sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  sampler.MaxLOD = D3D12_FLOAT32_MAX;
  D3D12_ROOT_SIGNATURE_DESC root{};
  root.NumParameters = 3;
  root.pParameters = parameters;
  root.NumStaticSamplers = 1;
  root.pStaticSamplers = &sampler;
  root.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  for (unsigned i = 0; i < 2; ++i) {
    sampler.Filter = i ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    ComPtr<ID3DBlob> signature, error;
    if (FAILED(
            D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) ||
        FAILED(device_->CreateRootSignature(0, signature->GetBufferPointer(),
                                            signature->GetBufferSize(),
                                            IID_PPV_ARGS(&twoDLinearRoots_[i])))) {
      for (auto &r : twoDLinearRoots_)
        r.Reset();
      return false;
    }
  }
  return true;
}
bool CreateTwoDLinearProgram(bool instanced, bool composite, bool nearest,
                             ComPtr<ID3D12PipelineState> &output) {
  ComPtr<ID3DBlob> vertex, pixel, errors;
  const auto *vs = instanced ? twod_linear_hlsl_instance : twod_linear_hlsl_vert;
  const auto *ps = composite ? twod_linear_hlsl_composite : twod_linear_hlsl_frag;
  if (FAILED(D3DCompile(vs, std::strlen(vs), "NexoraLinear2D", nullptr, nullptr,
                        instanced ? "twoDInstanceMain" : "twoDVertexMain", "vs_5_0", 0, 0, &vertex,
                        &errors)) ||
      FAILED(D3DCompile(ps, std::strlen(ps), "NexoraLinear2D", nullptr, nullptr,
                        composite ? "twoDCompositeMain" : "twoDFragmentMain", "ps_5_0", 0, 0,
                        &pixel, &errors))) {
    if (errors)
      std::fprintf(stderr, "linear 2D shader compile: %s\n",
                   static_cast<const char *>(errors->GetBufferPointer()));
    return false;
  }
  const D3D12_INPUT_ELEMENT_DESC inputs[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(UiVertex, position),
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(UiVertex, uv),
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(UiVertex, color),
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
  const D3D12_INPUT_ELEMENT_DESC instanceInputs[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SpriteGpuInstance, origin),
       D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SpriteGpuInstance, axisX),
       D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
      {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SpriteGpuInstance, axisY),
       D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
      {"TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(SpriteGpuInstance, uv),
       D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
      {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(SpriteGpuInstance, color),
       D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1}};
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};
  pipeline.pRootSignature = twoDLinearRoots_[nearest ? 1 : 0].Get();
  pipeline.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()};
  pipeline.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
  pipeline.BlendState.RenderTarget[0].BlendEnable = TRUE;
  pipeline.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  pipeline.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  pipeline.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipeline.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  pipeline.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  pipeline.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  pipeline.SampleMask = UINT_MAX;
  pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeline.RasterizerState.DepthClipEnable = TRUE;
  pipeline.InputLayout =
      instanced
          ? D3D12_INPUT_LAYOUT_DESC{instanceInputs, static_cast<UINT>(std::size(instanceInputs))}
          : D3D12_INPUT_LAYOUT_DESC{inputs, static_cast<UINT>(std::size(inputs))};
  pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeline.BlendState.IndependentBlendEnable = TRUE;
  pipeline.BlendState.RenderTarget[1] = pipeline.BlendState.RenderTarget[0];
  pipeline.NumRenderTargets = composite ? 1 : 2;
  if (composite)
    pipeline.BlendState.RenderTarget[0].BlendEnable = FALSE;
  pipeline.RTVFormats[0] =
      composite ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R16G16B16A16_FLOAT;
  if (!composite)
    pipeline.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
  pipeline.SampleDesc.Count = 1;
  return SUCCEEDED(device_->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&output)));
}
bool CreateTwoDLinearPipelines() {
  if (!CreateTwoDLinearRoots())
    return false;
  if (std::all_of(twoDLinearPipelines_.begin(), twoDLinearPipelines_.end(),
                  [](const auto &p) { return p.Get() != nullptr; }))
    return true;
  for (unsigned i = 0; i < 5; ++i)
    if (!CreateTwoDLinearProgram(i == 2 || i == 3, i == 4, i == 1 || i == 3,
                                 twoDLinearPipelines_[i])) {
      for (auto &p : twoDLinearPipelines_)
        p.Reset();
      return false;
    }
  return true;
}
void ResetTwoDLinearFrame(TwoDLinearFrame &f) {
  diagnostics_.twoDLinearTargetBytes -= f.allocationBytes;
  f = {};
}
void ResetTwoDLinearFrames() {
  for (auto &f : twoDLinearFrames_)
    ResetTwoDLinearFrame(f);
}
bool CreateTwoDLinearTargets(TwoDLinearFrame &f) {
  if (f.width == width_ && f.height == height_ && f.colors[0] && f.colors[1] && f.rtvs && f.srvs)
    return true;
  ResetTwoDLinearFrame(f);
  f.width = width_;
  f.height = height_;
  D3D12_DESCRIPTOR_HEAP_DESC rtv{};
  rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtv.NumDescriptors = 2;
  if (FAILED(device_->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&f.rtvs))))
    return false;
  D3D12_DESCRIPTOR_HEAP_DESC srv{};
  srv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srv.NumDescriptors = 16;
  srv.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  if (FAILED(device_->CreateDescriptorHeap(&srv, IID_PPV_ARGS(&f.srvs))))
    return false;
  D3D12_HEAP_PROPERTIES heap{};
  heap.Type = D3D12_HEAP_TYPE_DEFAULT;
  heap.CreationNodeMask = heap.VisibleNodeMask = 1;
  D3D12_RESOURCE_DESC desc{};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  desc.Width = width_;
  desc.Height = height_;
  desc.DepthOrArraySize = desc.MipLevels = 1;
  desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
  desc.SampleDesc.Count = 1;
  desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  D3D12_CLEAR_VALUE clear{};
  clear.Format = desc.Format;
  clear.Color[3] = 1;
  for (unsigned i = 0; i < 2; ++i) {
    if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                D3D12_RESOURCE_STATE_RENDER_TARGET, &clear,
                                                IID_PPV_ARGS(&f.colors[i]))))
      return false;
    const auto allocation = device_->GetResourceAllocationInfo(0, 1, &desc);
    if (allocation.SizeInBytes == UINT64_MAX)
      return false;
    f.allocationBytes += allocation.SizeInBytes;
    diagnostics_.twoDLinearTargetBytes += allocation.SizeInBytes;
    auto r = f.rtvs->GetCPUDescriptorHandleForHeapStart();
    r.ptr += SIZE_T(i) * increment_;
    device_->CreateRenderTargetView(f.colors[i].Get(), nullptr, r);
    auto v = f.srvs->GetCPUDescriptorHandleForHeapStart();
    v.ptr += SIZE_T(i) * uiDescriptorIncrement_;
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = desc.Format;
    view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels = 1;
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    device_->CreateShaderResourceView(f.colors[i].Get(), &view, v);
  }
  ++diagnostics_.twoDLinearTargetAllocations;
  return true;
}

public:
bool SupportsTwoDLinear() const noexcept override {
  if (!valid_ || destroyed_ || !srgbUiOutput_ || !device_)
    return false;
  D3D12_FEATURE_DATA_FORMAT_SUPPORT format{};
  format.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
  const auto need = D3D12_FORMAT_SUPPORT1_RENDER_TARGET | D3D12_FORMAT_SUPPORT1_BLENDABLE |
                    D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE;
  return SUCCEEDED(
             device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &format, sizeof(format))) &&
         (format.Support1 & need) == need;
}
SurfaceStatus DrawTwoDLinear(const TwoDLinearDrawData &data) override {
  if (!OnThread())
    return SurfaceStatus::WrongThread;
  if (!SupportsTwoDLinear())
    return SurfaceStatus::Unsupported;
  if (!acquired_ || sceneDrawn_ || uiDrawn_ || compositeDrawn_ ||
      !ValidateTwoDLinearDraws(data, width_, height_))
    return SurfaceStatus::InvalidDescriptor;
  for (const auto *packet : {&data.world, &data.overlay})
    for (const auto &upload : packet->textureUploads) {
      const auto old = uiTextures_.find(upload.textureId);
      if (old != uiTextures_.end() && old->second.immutable &&
          (!upload.immutable || old->second.width != upload.width ||
           old->second.height != upload.height || old->second.nearest != upload.nearest))
        return SurfaceStatus::InvalidDescriptor;
    }
  for (const auto *packet : {&data.world, &data.overlay})
    for (const auto &command : packet->commands) {
      bool known = uiTextures_.contains(command.textureId);
      for (const auto *source : {&data.world, &data.overlay})
        for (const auto &upload : source->textureUploads)
          known = known || upload.textureId == command.textureId;
      if (!known)
        return SurfaceStatus::InvalidDescriptor;
    }
  if (!CreateTwoDLinearPipelines())
    return SurfaceStatus::Unsupported;
  auto &f = twoDLinearFrames_[frame_];
  if (!CreateTwoDLinearTargets(f))
    return SurfaceStatus::DeviceLost;
  for (const auto *packet : {&data.world, &data.overlay})
    for (const auto &upload : packet->textureUploads)
      if (!UploadUiTexture(upload))
        return SurfaceStatus::DeviceLost;
  for (const auto *packet : {&data.world, &data.overlay})
    for (const auto &c : packet->commands)
      if (!uiTextures_.contains(c.textureId))
        return SurfaceStatus::InvalidDescriptor;
  const auto vertices = std::as_bytes(data.world.vertices),
             instances = std::as_bytes(data.world.spriteInstances);
  const auto instanceOffset = (vertices.size() + data.world.indices.size() + 3U) & ~std::size_t(3);
  const auto quadOffset = (instanceOffset + instances.size() + 3U) & ~std::size_t(3);
  const UiVertex quad[] = {{{0, 0}, {0, 0}, 0xffffffff},
                           {{float(width_), 0}, {1, 0}, 0xffffffff},
                           {{float(width_), float(height_)}, {1, 1}, 0xffffffff},
                           {{0, float(height_)}, {0, 1}, 0xffffffff}};
  const std::uint32_t indices[] = {0, 1, 2, 0, 2, 3};
  const auto uniformOffset =
      (quadOffset + sizeof(quad) + sizeof(indices) + 255U) & ~std::size_t(255);
  const auto required = uniformOffset + sizeof(TwoDLightingUniform);
  if (!CreateUploadAllocation(f.upload, f.capacity, required))
    return SurfaceStatus::DeviceLost;
  void *mapped{};
  D3D12_RANGE noRead{0, 0};
  if (FAILED(f.upload->Map(0, &noRead, &mapped)))
    return SurfaceStatus::DeviceLost;
  auto *bytes = static_cast<std::byte *>(mapped);
  std::memcpy(bytes, vertices.data(), vertices.size());
  std::memcpy(bytes + vertices.size(), data.world.indices.data(), data.world.indices.size());
  if (!instances.empty())
    std::memcpy(bytes + instanceOffset, instances.data(), instances.size());
  std::memcpy(bytes + quadOffset, quad, sizeof(quad));
  std::memcpy(bytes + quadOffset + sizeof(quad), indices, sizeof(indices));
  const TwoDLightingUniform fallback{};
  std::memcpy(bytes + uniformOffset, data.lighting ? &data.lighting->uniforms : &fallback,
              sizeof(fallback));
  f.upload->Unmap(0, nullptr);
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition = {f.colors[0].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, f.colorState,
                        D3D12_RESOURCE_STATE_RENDER_TARGET};
  if (f.colorState != D3D12_RESOURCE_STATE_RENDER_TARGET)
    commands_->ResourceBarrier(1, &barrier);
  f.colorState = D3D12_RESOURCE_STATE_RENDER_TARGET;
  const float black[] = {0, 0, 0, 1};
  auto target = f.rtvs->GetCPUDescriptorHandleForHeapStart();
  commands_->ClearRenderTargetView(target, black, 0, nullptr);
  auto emission = target;
  emission.ptr += increment_;
  commands_->ClearRenderTargetView(emission, black, 0, nullptr);
  commands_->OMSetRenderTargets(2, &target, TRUE, nullptr);
  ID3D12DescriptorHeap *heaps[] = {uiDescriptors_.Get()};
  commands_->SetDescriptorHeaps(1, heaps);
  TwoDShaderConstants transform{{2.F / width_, -2.F / height_, -1, 1}};
  const D3D12_VIEWPORT viewport{0, 0, float(width_), float(height_), 0, 1};
  commands_->RSSetViewports(1, &viewport);
  commands_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  const D3D12_VERTEX_BUFFER_VIEW vertexView{f.upload->GetGPUVirtualAddress(),
                                            static_cast<UINT>(vertices.size()), sizeof(UiVertex)};
  const D3D12_VERTEX_BUFFER_VIEW instanceView{f.upload->GetGPUVirtualAddress() + instanceOffset,
                                              static_cast<UINT>(instances.size()),
                                              sizeof(SpriteGpuInstance)};
  const D3D12_INDEX_BUFFER_VIEW indexView{f.upload->GetGPUVirtualAddress() + vertices.size(),
                                          static_cast<UINT>(data.world.indices.size()),
                                          data.world.indices32Bit ? DXGI_FORMAT_R32_UINT
                                                                  : DXGI_FORMAT_R16_UINT};
  commands_->IASetIndexBuffer(&indexView);
  std::size_t commandIndex{};
  for (const auto &c : data.world.commands) {
    transform.style = data.lighting ? data.lighting->styles[commandIndex]
                                    : TwoDLightingStyle{UINT32_MAX, 0, 0, 0};
    ++commandIndex;
    const auto clipped = TwoDLinearClip(c, data.viewport, width_, height_);
    if (!clipped[2] || !clipped[3])
      continue;
    const auto &texture = uiTextures_.at(c.textureId);
    commands_->SetGraphicsRootSignature(twoDLinearRoots_[texture.nearest ? 1 : 0].Get());
    commands_->SetPipelineState(
        twoDLinearPipelines_[(c.instanceCount ? 2 : 0) + (texture.nearest ? 1 : 0)].Get());
    commands_->SetGraphicsRoot32BitConstants(0, sizeof(transform) / 4, &transform, 0);
    commands_->SetGraphicsRootConstantBufferView(2,
                                                 f.upload->GetGPUVirtualAddress() + uniformOffset);
    auto descriptor = uiDescriptors_->GetGPUDescriptorHandleForHeapStart();
    descriptor.ptr += UINT64(texture.descriptor) * uiDescriptorIncrement_;
    commands_->SetGraphicsRootDescriptorTable(1, descriptor);
    const D3D12_RECT clip{LONG(clipped[0]), LONG(clipped[1]), LONG(clipped[0] + clipped[2]),
                          LONG(clipped[1] + clipped[3])};
    commands_->RSSetScissorRects(1, &clip);
    commands_->IASetVertexBuffers(0, 1, c.instanceCount ? &instanceView : &vertexView);
    if (c.instanceCount) {
      commands_->DrawInstanced(6, c.instanceCount, 0, c.firstInstance);
      ++diagnostics_.spriteInstancedDrawCalls;
      diagnostics_.spriteGpuInstances += c.instanceCount;
    } else
      commands_->DrawIndexedInstanced(c.elementCount, 1, c.indexOffset, c.vertexOffset, 0);
    ++diagnostics_.nativeUiDrawCalls;
  }
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  commands_->ResourceBarrier(1, &barrier);
  f.colorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  target = heap_->GetCPUDescriptorHandleForHeapStart();
  target.ptr += SIZE_T(frame_) * increment_;
  commands_->OMSetRenderTargets(1, &target, FALSE, nullptr);
  commands_->ClearRenderTargetView(target, black, 0, nullptr);
  heaps[0] = f.srvs.Get();
  commands_->SetDescriptorHeaps(1, heaps);
  commands_->SetGraphicsRootSignature(twoDLinearRoots_[0].Get());
  commands_->SetPipelineState(twoDLinearPipelines_[4].Get());
  commands_->SetGraphicsRoot32BitConstants(0, sizeof(transform) / 4, &transform, 0);
  commands_->SetGraphicsRootConstantBufferView(2, f.upload->GetGPUVirtualAddress() + uniformOffset);
  commands_->SetGraphicsRootDescriptorTable(1, f.srvs->GetGPUDescriptorHandleForHeapStart());
  const auto &v = data.viewport;
  const D3D12_RECT clip =
      v == std::array<std::uint32_t, 4>{}
          ? D3D12_RECT{0, 0, LONG(width_), LONG(height_)}
          : D3D12_RECT{LONG(v[0]), LONG(v[1]), LONG(v[0] + v[2]), LONG(v[1] + v[3])};
  commands_->RSSetScissorRects(1, &clip);
  const D3D12_VERTEX_BUFFER_VIEW quadView{f.upload->GetGPUVirtualAddress() + quadOffset,
                                          sizeof(quad), sizeof(UiVertex)};
  const D3D12_INDEX_BUFFER_VIEW quadIndices{f.upload->GetGPUVirtualAddress() + quadOffset +
                                                sizeof(quad),
                                            sizeof(indices), DXGI_FORMAT_R32_UINT};
  commands_->IASetVertexBuffers(0, 1, &quadView);
  commands_->IASetIndexBuffer(&quadIndices);
  commands_->DrawIndexedInstanced(6, 1, 0, 0, 0);
  sceneDrawn_ = true;
  ++diagnostics_.nativeUiDrawCalls;
  diagnostics_.spriteInstanceUploadBytes += instances.size();
  if (!data.overlay.commands.empty()) {
    const auto result = RenderUi(data.overlay);
    if (result != SurfaceStatus::Ready)
      return result;
  }
  ++diagnostics_.twoDLinearFrames;
  return SurfaceStatus::Ready;
}
