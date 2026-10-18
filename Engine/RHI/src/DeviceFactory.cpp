#include "Nexora/RHI/Device.h"

#include <stdexcept>

namespace nexora::rhi {
#if defined(_WIN32) && defined(NEXORA_ENABLE_NATIVE_BACKENDS)
std::unique_ptr<Device> CreateDirect3D12Device(const DeviceCreationOptions &);
#else
std::unique_ptr<Device> CreateDirect3D12Device(const DeviceCreationOptions &) {
  throw std::runtime_error("Direct3D12 backend is only available on Windows");
}
#endif

#if defined(__APPLE__) && defined(NEXORA_ENABLE_NATIVE_BACKENDS)
std::unique_ptr<Device> CreateMetalDevice(const DeviceCreationOptions &);
#else
std::unique_ptr<Device> CreateMetalDevice(const DeviceCreationOptions &) {
  throw std::runtime_error("Metal backend is only available on Apple platforms");
}
#endif

#if (defined(_WIN32) || (defined(__unix__) && !defined(__APPLE__))) &&                             \
    defined(NEXORA_ENABLE_NATIVE_BACKENDS) && defined(NEXORA_ENABLE_VULKAN_BACKEND)
std::unique_ptr<Device> CreateVulkanDevice(const DeviceCreationOptions &);
#else
std::unique_ptr<Device> CreateVulkanDevice(const DeviceCreationOptions &) {
  throw std::runtime_error("Vulkan backend is not built for this platform");
}
#endif
std::unique_ptr<Device> CreateDevice(Backend backend) {
  return CreateDeviceWithOptions(backend, {});
}
std::unique_ptr<Device> CreateDeviceWithOptions(Backend backend,
                                                const DeviceCreationOptions &options) {
  switch (options.d3d12_adapter_policy) {
  case D3D12AdapterPolicy::Default:
    break;
  case D3D12AdapterPolicy::Software:
  case D3D12AdapterPolicy::PreferHardware:
  case D3D12AdapterPolicy::RequireHardware:
    if (backend != Backend::Direct3D12)
      throw std::invalid_argument("adapter policy is D3D12-only");
    break;
  default:
    throw std::invalid_argument("unknown D3D12 adapter policy");
  }
  if (options.enable_d3d12_preview_runtime && backend != Backend::Direct3D12)
    throw std::invalid_argument("preview runtime is D3D12-only");
  switch (backend) {
  case Backend::Null:
    return CreateValidationDevice();
  case Backend::Direct3D12:
    return CreateDirect3D12Device(options);
  case Backend::Vulkan:
    return CreateVulkanDevice(options);
  case Backend::Metal:
    return CreateMetalDevice(options);
  }
  throw std::invalid_argument("unknown RHI backend");
}

bool IsBackendAvailable(Backend backend) noexcept {
  try {
    auto device = CreateDevice(backend);
    return device != nullptr;
  } catch (...) {
    return false;
  }
}
} // namespace nexora::rhi
