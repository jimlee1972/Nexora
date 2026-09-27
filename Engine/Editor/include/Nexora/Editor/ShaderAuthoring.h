#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/Runtime/ShaderRuntime.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nexora::editor {
enum class ShaderDiagnosticSeverity : std::uint8_t { Info, Warning, Error };
struct ShaderCompileDiagnostic final {
  ShaderDiagnosticSeverity severity{ShaderDiagnosticSeverity::Info};
  std::string file;
  std::uint32_t line{};
  std::uint32_t column{};
  std::string message;
};
struct ShaderCompileResult final {
  bool succeeded{};
  rhi::ShaderModuleArtifact artifact;
  std::vector<ShaderCompileDiagnostic> diagnostics;
};

// Applies a successful compiler result to Runtime as one generation; failed results only carry
// diagnostics and leave the active shader untouched.
[[nodiscard]] NEXORA_EDITOR_API bool ApplyShaderCompileResult(
    const ShaderCompileResult &result, runtime::ShaderArtifactSlot &slot, std::string &error);
} // namespace nexora::editor
