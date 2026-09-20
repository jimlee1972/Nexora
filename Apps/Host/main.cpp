#include <iostream>

#include "Nexora/Core/Engine.h"
#include "Nexora/Foundation/BuildInfo.h"

int main() {
  const auto info = nexora::foundation::GetBuildInfo();
  std::cout << "Nexora " << info.engine_version << " (ABI " << info.abi_version << ", "
            << info.build_configuration << ", " << info.link_mode << ")\n";
  nexora::core::Engine engine;
  engine.Initialize();
  engine.BeginFrame();
  engine.Shutdown();
  return 0;
}
