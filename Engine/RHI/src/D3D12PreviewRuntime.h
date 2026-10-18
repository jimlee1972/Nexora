#pragma once

// Included after the Windows SDK and WRL headers. Default builds never reference preview APIs.
namespace nexora::rhi::detail {
#if NEXORA_ENABLE_D3D12_PREVIEW
class D3D12PreviewRuntime final {
public:
  void Create(Microsoft::WRL::ComPtr<ID3D12Device> &device,
              Microsoft::WRL::ComPtr<IDXGIAdapter1> &adapter, D3D12AdapterPolicy policy) {
    const auto check = [](HRESULT result, const char *operation) {
      if (FAILED(result))
        throw std::runtime_error(std::string(operation) + " failed (HRESULT " +
                                 std::to_string(static_cast<unsigned long>(result)) + ")");
    };
    const auto *path = std::getenv("NEXORA_D3D12_PREVIEW_SDK_PATH");
    if (!path || !*path)
      throw std::invalid_argument("preview runtime requires NEXORA_D3D12_PREVIEW_SDK_PATH");
    Microsoft::WRL::ComPtr<ID3D12SDKConfiguration1> configuration;
    check(D3D12GetInterface(CLSID_D3D12SDKConfiguration, IID_PPV_ARGS(&configuration)),
          "D3D12GetInterface(SDKConfiguration1)");
    check(configuration->CreateDeviceFactory(721, path, IID_PPV_ARGS(&factory_)),
          "CreateDeviceFactory(preview 721)");
    check(factory_->SetFlags(D3D12_DEVICE_FACTORY_FLAG_DISALLOW_STORING_NEW_DEVICE_AS_SINGLETON),
          "SetFlags(private preview factory)");
    const GUID shader_models = D3D12ExperimentalShaderModels;
    check(factory_->EnableExperimentalFeatures(1, &shader_models, nullptr, nullptr),
          "EnableExperimentalFeatures(private shader models)");
    SelectD3D12Adapter(
        policy, true,
        [this](IDXGIAdapter1 *candidate, ID3D12Device **created) {
          return factory_->CreateDevice(candidate, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(created));
        },
        device, adapter);
  }

private:
  Microsoft::WRL::ComPtr<ID3D12DeviceFactory> factory_;
};
#else
class D3D12PreviewRuntime final {};
#endif
} // namespace nexora::rhi::detail
