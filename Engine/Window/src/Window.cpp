#include "Nexora/Window/Window.h"
namespace Nexora::Window {
static_assert(sizeof(WindowHandle) == sizeof(std::uint64_t));
#if !defined(_WIN32) && !defined(__linux__) && !defined(__APPLE__)
std::unique_ptr<IWindowSystem> CreateWindowSystem() { return {}; }
#endif
} // namespace Nexora::Window
