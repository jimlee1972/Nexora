#pragma once
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/TwoD/Assets.h"
#include <iosfwd>
#include <optional>

namespace nexora::twod::validation {
// Tool policy only; no engine/public ABI or original benchmark workload changes.
struct IntegratedOptions {
  bool native{};
  unsigned frames{48}, seconds{}, reloadEvery{600};
  Nexora::Presentation::SurfaceBackend backend{Nexora::Presentation::SurfaceBackend::Automatic};
  unsigned objects{10000};
};
void IntegratedStress(std::ostream &report, std::ostream &trace, const IntegratedOptions &options,
                      const std::optional<Atlas> &atlas, const std::optional<Clip> &clip);
} // namespace nexora::twod::validation
