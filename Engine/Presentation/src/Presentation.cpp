#include "Nexora/Presentation/Surface.h"
namespace Nexora::Presentation {
std::unique_ptr<ISurface> CreateVulkanSurface(const SurfaceDescriptor &, Window::IWindowSystem &);
std::unique_ptr<ISurface> CreateMetalSurface(const SurfaceDescriptor &, Window::IWindowSystem &);
static_assert(sizeof(SurfaceDescriptor::width) == sizeof(std::uint32_t));
#if !defined(_WIN32)
std::unique_ptr<ISurface> CreateSurface(const SurfaceDescriptor &descriptor,
                                        Window::IWindowSystem &windows) {
#if defined(NEXORA_HAS_METAL_PRESENTATION)
  if (descriptor.backend == SurfaceBackend::Automatic ||
      descriptor.backend == SurfaceBackend::Metal)
    return CreateMetalSurface(descriptor, windows);
#endif
#if defined(NEXORA_HAS_VULKAN_PRESENTATION)
  if (descriptor.backend == SurfaceBackend::Automatic ||
      descriptor.backend == SurfaceBackend::Vulkan)
    return CreateVulkanSurface(descriptor, windows);
#endif
  (void)descriptor;
  (void)windows;
  return {};
}
#endif
} // namespace Nexora::Presentation
