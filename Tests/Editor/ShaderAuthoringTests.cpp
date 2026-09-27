#include "Nexora/Editor/ShaderAuthoring.h"

#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
}

int main() {
  try {
    using namespace nexora;
    const auto layout_hash = rhi::TrianglePipelineLayout().layout_hash;
    runtime::ShaderArtifactSlot slot{rhi::Backend::Vulkan, layout_hash};
    std::string error;
    auto artifact = rhi::ShaderModuleArtifact{};
    artifact.shader_id = "triangle";
    artifact.format = rhi::ShaderBinaryFormat::SpirV;
    artifact.entry_point = "vertexMain";
    artifact.binary = {std::byte{0x01}};
    artifact.reflection = rhi::TrianglePipelineLayout();
    editor::ShaderCompileResult first{true, artifact, {}};
    if (slot.BuildMode() == runtime::ShaderBuildMode::Development) {
      Require(editor::ApplyShaderCompileResult(first, slot, error) && slot.Generation() == 1,
              "successful compile result was not published");
    } else {
      Require(!editor::ApplyShaderCompileResult(first, slot, error) && slot.Generation() == 0,
              "Shipping Runtime accepted an Editor compile result");
      Require(slot.Stage(artifact, runtime::ShaderArtifactSource::Cooked, error) &&
                  slot.Commit(error),
              "Shipping Runtime rejected the cooked test artifact");
    }

    auto failed = first;
    failed.succeeded = false;
    failed.diagnostics.push_back({editor::ShaderDiagnosticSeverity::Error,
                                  "Shaders/Triangle.slang", 9, 4, "unknown identifier"});
    const auto active_generation = slot.Generation();
    const auto active_binary = slot.Active()->binary;
    Require(!editor::ApplyShaderCompileResult(failed, slot, error) &&
                slot.Generation() == active_generation &&
                slot.Active()->binary == active_binary,
            "failed compile result replaced the active artifact");

    auto invalid_success = first;
    invalid_success.artifact.reflection.layout_hash++;
    Require(!editor::ApplyShaderCompileResult(invalid_success, slot, error) &&
                slot.Generation() == active_generation,
            "incompatible compiler output was published");
    std::cout << "Editor shader authoring contracts passed\n";
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << exception.what() << '\n';
    return 1;
  }
}
