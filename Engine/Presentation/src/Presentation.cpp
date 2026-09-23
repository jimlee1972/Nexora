#include "Nexora/Presentation/Surface.h"
namespace Nexora::Presentation {
static_assert(sizeof(SurfaceDescriptor::width) == sizeof(std::uint32_t));
#if !defined(_WIN32)
std::unique_ptr<ISurface> CreateSurface(const SurfaceDescriptor &, Window::IWindowSystem &) {
  return {};
}
#endif
} // namespace Nexora::Presentation
