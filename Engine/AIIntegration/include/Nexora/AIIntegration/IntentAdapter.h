#pragma once

#include "Nexora/AI/AI.h"
#include "Nexora/AIIntegration/Api.h"
#include "Nexora/Runtime/Runtime.h"

#include <optional>

namespace nexora::ai::integration {

// Projects AI locomotion into Runtime's horizontal motor input without transferring ownership.
class NEXORA_AI_INTEGRATION_API IntentAdapter final {
public:
  [[nodiscard]] static std::optional<runtime::CharacterIntent>
  Project(const CharacterIntent &intent);
};

} // namespace nexora::ai::integration
