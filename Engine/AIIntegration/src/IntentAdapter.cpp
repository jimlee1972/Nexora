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
  // Finite inputs can still overflow: a large speed times a large or non-unit direction yields
  // +/-inf, and the motor's hypot()/scale step then turns that into NaN on the character. Reject
  // any projection whose components or planar magnitude are not finite.
  const runtime::CharacterIntent projected{direction.x * intent.desired_speed,
                                           direction.z * intent.desired_speed};
  if (!std::isfinite(projected.requested_x) || !std::isfinite(projected.requested_z) ||
      !std::isfinite(std::hypot(projected.requested_x, projected.requested_z))) {
    return std::nullopt;
  }
  return projected;
}

} // namespace nexora::ai::integration
