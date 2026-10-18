#include "Nexora/RHI/Device.h"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora::rhi;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
void TestRejection() {
  Require(DeviceCreationOptions{}.d3d12_adapter_policy == D3D12AdapterPolicy::Default,
          "adapter policy changed the default");
  for (auto backend : {Backend::Null, Backend::Vulkan, Backend::Metal}) {
    for (auto policy : {D3D12AdapterPolicy::Software, D3D12AdapterPolicy::PreferHardware,
                        D3D12AdapterPolicy::RequireHardware}) {
      DeviceCreationOptions options;
      options.d3d12_adapter_policy = policy;
      bool rejected = false;
      try {
        (void)CreateDeviceWithOptions(backend, options);
      } catch (const std::invalid_argument &) {
        rejected = true;
      }
      Require(rejected, "non-D3D12 backend ignored explicit adapter policy");
    }
  }
  for (auto backend : {Backend::Null, Backend::Direct3D12, Backend::Vulkan, Backend::Metal}) {
    DeviceCreationOptions options;
    options.d3d12_adapter_policy = static_cast<D3D12AdapterPolicy>(255);
    bool rejected = false;
    try {
      (void)CreateDeviceWithOptions(backend, options);
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    Require(rejected, "invalid adapter policy reached backend creation");
  }
}
#if defined(NEXORA_TEST_D3D12_ADAPTER_NATIVE)
constexpr bool preview =
#if defined(NEXORA_TEST_D3D12_ADAPTER_PREVIEW)
    true;
#else
    false;
#endif
std::unique_ptr<Device> Create(D3D12AdapterPolicy policy) {
  std::cout << "Creating adapter policy=" << static_cast<unsigned>(policy) << '\n';
  DeviceCreationOptions options;
  options.enable_d3d12_preview_runtime = preview;
  options.d3d12_adapter_policy = policy;
  return CreateDeviceWithOptions(Backend::Direct3D12, options);
}
GraphicsDeviceCapabilities Exercise(Device &device) {
  const auto caps = device.Capabilities();
  Require(device.GetBackend() == Backend::Direct3D12 && caps.backend == Backend::Direct3D12 &&
              !caps.validation_only && caps.vendor_id != 0,
          "adapter snapshot is missing native identity");
  const std::array<std::byte, 16> expected{std::byte{1}, std::byte{9}, std::byte{31},
                                           std::byte{255}};
  const auto buffer = device.CreateBuffer({expected.size(), "adapter policy witness"});
  device.WriteBuffer(buffer, 0, expected);
  auto commands = device.CreateCommandList(QueueType::Graphics);
  device.WaitForSubmission(device.Submit(*commands));
  commands.reset();
  std::array<std::byte, expected.size()> actual{};
  device.ReadBufferForTesting(buffer, 0, actual);
  device.DestroyBuffer(buffer);
  Require(actual == expected, "selected native adapter corrupted the buffer witness");
  std::cout << "preview=" << preview << " software=" << caps.software_device
            << " vendor=" << caps.vendor_id << " device=" << caps.device_id << '\n';
  return caps;
}
void TestNative() {
  auto baseline = Create(D3D12AdapterPolicy::Default);
  const auto original = Exercise(*baseline);
  {
    auto software = Create(D3D12AdapterPolicy::Software);
    Require(Exercise(*software).software_device, "explicit software policy did not select WARP");
  }
  const auto preferred = [&] {
    auto device = Create(D3D12AdapterPolicy::PreferHardware);
    return Exercise(*device);
  }();
  if (preview)
    Require(original.software_device, "preview default stopped selecting WARP");
  else
    Require(original.software_device == preferred.software_device &&
                original.vendor_id == preferred.vendor_id &&
                original.device_id == preferred.device_id,
            "ordinary default stopped preferring the same hardware with WARP fallback");
  try {
    auto hardware = Create(D3D12AdapterPolicy::RequireHardware);
    const auto caps = Exercise(*hardware);
    Require(!caps.software_device && !preferred.software_device &&
                caps.vendor_id == preferred.vendor_id && caps.device_id == preferred.device_id,
            "required hardware silently selected software or differs from hardware preference");
    std::cout << "RequireHardware selected a nonsoftware DXGI adapter\n";
  } catch (const D3D12HardwareUnavailableError &) {
    Require(preferred.software_device, "hardware discovery disagrees between required/preferred");
    std::cout
        << "RequireHardware correctly reported no compatible hardware; WARP fallback verified\n";
  }
  // Private factory destruction must not invalidate a still-live independently created device.
  const auto retained = Exercise(*baseline);
  Require(retained.vendor_id == original.vendor_id && retained.device_id == original.device_id &&
              retained.software_device == original.software_device,
          "another factory changed the retained device");
}
#endif
} // namespace
int main() {
  try {
    TestRejection();
#if defined(NEXORA_TEST_D3D12_ADAPTER_NATIVE)
    TestNative();
#endif
    std::cout << "D3D12 adapter policy contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
