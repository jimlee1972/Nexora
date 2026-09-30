#include "Nexora/AIIntegration/IntentAdapter.h"

#include <cmath>

namespace nexora::ai::integration {

std::optional<runtime::CharacterIntent> IntentAdapter::Project(const CharacterIntent &intent) {
  const auto &direction = intent.desired_move_direction;
  if (!std::isfinite(direction.x) || !std::isfinite(direction.z) ||
      !std::isfinite(intent.desired_speed) || intent.desired_speed < 0.0) {
    return std::nullopt;
  }

  if (intent.movement_mode == CharacterMovementMode::Disabled) {
    return runtime::CharacterIntent{};
  }
  return runtime::CharacterIntent{direction.x * intent.desired_speed,
                                  direction.z * intent.desired_speed};
}

} // namespace nexora::ai::integration
