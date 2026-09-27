#include "Nexora/Editor/ShaderAuthoring.h"

#include <algorithm>

namespace nexora::editor {
bool ApplyShaderCompileResult(const ShaderCompileResult &result, runtime::ShaderArtifactSlot &slot,
                              std::string &error) {
  const bool has_error = std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                                     [](const ShaderCompileDiagnostic &diagnostic) {
                                       return diagnostic.severity ==
                                              ShaderDiagnosticSeverity::Error;
                                     });
  if (!result.succeeded || has_error) {
    slot.DiscardStaged();
    error = "shader compilation failed; active artifact was preserved";
    return false;
  }
  if (!slot.Stage(result.artifact, runtime::ShaderArtifactSource::DynamicCompile, error)) {
    slot.DiscardStaged();
    return false;
  }
  return slot.Commit(error);
}
} // namespace nexora::editor
