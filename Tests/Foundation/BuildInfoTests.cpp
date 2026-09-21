#include <cstring>

#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Foundation/Types.h"
#include "Nexora/Math/Math.h"

int main() {
  const auto info = nexora::foundation::GetBuildInfo();
  if (info.engine_version == nullptr || std::strlen(info.engine_version) == 0)
    return 1;
  if (info.abi_version != nexora::foundation::kEngineAbiVersion)
    return 2;
  if (info.build_configuration == nullptr || info.link_mode == nullptr)
    return 3;
  const char *build_id = nexora::foundation::GetBuildId();
  if (build_id == nullptr || std::strlen(build_id) == 0)
    return 4;
  using namespace nexora;
  if (!foundation::IsValidUtf8("Nexora \xE2\x9C\x93") || foundation::IsValidUtf8("\xC0\x80"))
    return 5;
  const auto number = foundation::ParseNumber<int>("42");
  if (!number || number.Value() != 42 || foundation::ParseNumber<int>("42x"))
    return 6;
  const auto cross = math::Cross({1, 0, 0}, {0, 1, 0});
  if (!math::NearlyEqual(cross.z, 1.0F) ||
      math::Length(math::NormalizeSafe(math::Vector3{})) != 0.0F)
    return 7;
  const auto matrix = math::Compose({{1, 2, 3}, {}, {1, 1, 1}});
  if (!math::NearlyEqual(matrix(0, 3), 1.0F) || !math::NearlyEqual(matrix(2, 3), 3.0F))
    return 8;
  return 0;
}
