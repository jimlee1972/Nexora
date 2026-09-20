#include <cstring>

#include "Nexora/Foundation/BuildInfo.h"

int main() {
  const auto info = nexora::foundation::GetBuildInfo();
  if (info.engine_version == nullptr || std::strlen(info.engine_version) == 0)
    return 1;
  if (info.abi_version != nexora::foundation::kEngineAbiVersion)
    return 2;
  if (info.build_configuration == nullptr || info.link_mode == nullptr)
    return 3;
  return 0;
}
