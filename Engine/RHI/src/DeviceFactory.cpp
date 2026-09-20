#include "Nexora/RHI/Device.h"

#include <stdexcept>

namespace nexora::rhi {
#if defined(_WIN32) && defined(NEXORA_ENABLE_NATIVE_BACKENDS)
std::unique_ptr<Device> CreateDirect3D12Device();
#else
std::unique_ptr<Device> CreateDirect3D12Device() {
  throw std::runtime_error("Direct3D12 backend is only available on Windows");
}
#endif

#if defined(__APPLE__) && defined(NEXORA_ENABLE_NATIVE_BACKENDS)
std::unique_ptr<Device> CreateMetalDevice();
#else
std::unique_ptr<Device> CreateMetalDevice() {
  throw std::runtime_error("Metal backend is only available on Apple platforms");
}
#endif

#if (defined(_WIN32) || (defined(__unix__) && !defined(__APPLE__))) && \
    defined(NEXORA_ENABLE_NATIVE_BACKENDS)
std::unique_ptr<Device> CreateVulkanDevice();
#else
std::unique_ptr<Device> CreateVulkanDevice() {
  throw std::runtime_error("Vulkan backend is not built for this platform");
}
#endif
std::unique_ptr<Device> CreateDevice(Backend backend) {
  switch (backend) {
  case Backend::Null:
    return CreateValidationDevice();
  case Backend::Direct3D12:
    return CreateDirect3D12Device();
  case Backend::Vulkan:
    return CreateVulkanDevice();
  case Backend::Metal:
    return CreateMetalDevice();
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
