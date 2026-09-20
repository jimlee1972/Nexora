#include "Nexora/Foundation/BuildInfo.h"

extern "C" std::uint32_t NexoraPluginAbiVersion() noexcept {
  return nexora::foundation::kEngineAbiVersion;
}
