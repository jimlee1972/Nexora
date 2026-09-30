#include "Nexora/AIIntegration/IntentAdapter.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
} // namespace

int main() {
  using namespace nexora;
  const ai::CharacterIntent intent{
      {0.6, 0.0, 0.8}, 4.0, {}, ai::CharacterMovementMode::Grounded, 0};
  const auto projected = ai::integration::IntentAdapter::Project(intent);
  Require(projected.has_value() && projected->requested_x == 2.4 && projected->requested_z == 3.2,
          "AI direction and speed were not projected to horizontal locomotion");
  const auto disabled = ai::integration::IntentAdapter::Project(
      {{1.0, 0.0, 0.0}, 3.0, {}, ai::CharacterMovementMode::Disabled, 0});
  Require(disabled.has_value() && disabled->requested_x == 0.0 && disabled->requested_z == 0.0,
          "disabled AI movement was not projected to a hold intent");
  Require(!ai::integration::IntentAdapter::Project(
               {{std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0},
                1.0,
                {},
                ai::CharacterMovementMode::Grounded,
                0})
               .has_value(),
          "non-finite AI movement was accepted");
  Require(!ai::integration::IntentAdapter::Project(
               {{1.0, 0.0, 0.0}, -1.0, {}, ai::CharacterMovementMode::Grounded, 0})
               .has_value(),
          "negative AI movement speed was accepted");
  const auto motion = runtime::CharacterMotor{}.Simulate(*projected, 3.0, true);
  Require(std::abs(motion.actual_x - 1.8) < 1e-12 && std::abs(motion.actual_z - 2.4) < 1e-12 &&
              motion.grounded,
          "projected AI intent did not pass through Runtime motor resolution");
  std::cout << "AI CharacterIntent adapter tests passed\n";
}
