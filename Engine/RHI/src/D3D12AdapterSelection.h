#pragma once

// Included after Device.h, DXGI, D3D12 and WRL. The callback owns the ordinary/private factory
// route.
namespace nexora::rhi::detail {
inline bool IsD3D12SoftwareAdapter(const DXGI_ADAPTER_DESC1 &description) noexcept {
  return (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 ||
         (description.VendorId == 0x1414 && description.DeviceId == 0x008c);
}
template <class Create>
void SelectD3D12Adapter(D3D12AdapterPolicy policy, bool preview, Create create,
                        Microsoft::WRL::ComPtr<ID3D12Device> &device,
                        Microsoft::WRL::ComPtr<IDXGIAdapter1> &adapter) {
  const auto check = [](HRESULT result, const char *operation) {
    if (FAILED(result))
      throw std::runtime_error(std::string(operation) + " failed (HRESULT " +
                               std::to_string(static_cast<unsigned long>(result)) + ")");
  };
  const bool preserve_ordinary_default = policy == D3D12AdapterPolicy::Default && !preview;
  if (policy == D3D12AdapterPolicy::Default)
    policy = preview ? D3D12AdapterPolicy::Software : D3D12AdapterPolicy::PreferHardware;
  Microsoft::WRL::ComPtr<IDXGIFactory6> dxgi;
  check(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgi)), "CreateDXGIFactory2(adapter selection)");
  if (policy != D3D12AdapterPolicy::Software) {
    // Fail instead of wrapping an index or silently falling back on an enumeration failure.
    constexpr UINT maximum_adapters = 64;
    bool exhausted = false;
    for (UINT index = 0; index < maximum_adapters; ++index) {
      Microsoft::WRL::ComPtr<IDXGIAdapter1> candidate;
      const auto result = dxgi->EnumAdapterByGpuPreference(
          index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&candidate));
      if (result == DXGI_ERROR_NOT_FOUND) {
        exhausted = true;
        break;
      }
      check(result, "EnumAdapterByGpuPreference");
      DXGI_ADAPTER_DESC1 description{};
      check(candidate->GetDesc1(&description), "IDXGIAdapter1::GetDesc1");
      // Hosted DXGI can enumerate Microsoft Basic Render Driver/WARP (1414:008c) without
      // SOFTWARE, while EnumWarpAdapter reports the same identity with SOFTWARE. Explicit
      // hardware requests exclude that known software identity. Preserve the ordinary Default
      // route's existing enumeration behavior for compatibility.
      if (preserve_ordinary_default ? (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0
                                    : IsD3D12SoftwareAdapter(description))
        continue;
      Microsoft::WRL::ComPtr<ID3D12Device> created;
      const auto creation = create(candidate.Get(), created.GetAddressOf());
      if (SUCCEEDED(creation)) {
        device = std::move(created);
        adapter = std::move(candidate);
        return;
      }
      // Only a feature-level/interface incompatibility permits trying the next candidate.
      // Resource exhaustion, removed devices and other runtime faults must remain visible.
      if (creation != DXGI_ERROR_UNSUPPORTED && creation != E_INVALIDARG &&
          creation != E_NOINTERFACE)
        check(creation, "CreateDevice(hardware)");
    }
    if (!exhausted)
      throw std::runtime_error("D3D12 adapter enumeration exceeded the 64-adapter limit");
    if (policy == D3D12AdapterPolicy::RequireHardware)
      throw D3D12HardwareUnavailableError("no compatible D3D12 hardware adapter");
  }
  Microsoft::WRL::ComPtr<IDXGIAdapter1> software;
  check(dxgi->EnumWarpAdapter(IID_PPV_ARGS(&software)), "EnumWarpAdapter");
  Microsoft::WRL::ComPtr<ID3D12Device> created;
  check(create(software.Get(), created.GetAddressOf()), "CreateDevice(WARP)");
  device = std::move(created);
  adapter = std::move(software);
}
} // namespace nexora::rhi::detail
