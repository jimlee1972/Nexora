#include "Nexora/Window/Window.h"

namespace Nexora::Window {
static_assert(sizeof(WindowHandle) == sizeof(std::uint64_t));
} // namespace Nexora::Window
