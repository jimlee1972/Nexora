#include "Nexora/Editor/ShaderAuthoring.h"

#include <array>
#include <filesystem>
#include <fstream>
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

    const auto diagnostics = editor::ParseShaderDiagnostics(
        "Shaders/Test.slang:12:7: warning: slow path\nShaders/Test.slang:14:3: error: bad type\n");
    Require(diagnostics.size() == 2 && diagnostics[0].severity == editor::ShaderDiagnosticSeverity::Warning &&
                diagnostics[1].severity == editor::ShaderDiagnosticSeverity::Error &&
                diagnostics[1].line == 14,
            "Slang diagnostics were not parsed with source positions");

    const auto source_path = std::filesystem::temp_directory_path() / "nexora-hot-reload-test.slang";
    const auto output_path = std::filesystem::temp_directory_path() / "nexora-hot-reload-test.spv";
    { std::ofstream source(source_path); source << "shader"; }
    editor::ShaderCompileRequest request;
    request.shader_id = "triangle";
    request.source_path = source_path;
    request.output_path = output_path;
    request.entry_points = {"vertexMain"};
    request.format = rhi::ShaderBinaryFormat::SpirV;
    request.reflection = rhi::TrianglePipelineLayout();
    const auto compiler_result = editor::CompileSlang(
        request, {"slangc"},
        [&request](const std::vector<std::string> &arguments, std::string &output) {
          Require(arguments.size() >= 2 && arguments.back() == request.output_path.string(),
                  "Slang command did not preserve the requested output path");
          std::ofstream compiled(request.output_path, std::ios::binary);
          const std::array<std::byte, 2> bytes{std::byte{0x11}, std::byte{0x22}};
          compiled.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
          output = "Shaders/Test.slang:2:3: warning: test warning\n";
          return 0;
        });
    Require(compiler_result.succeeded && compiler_result.artifact.binary.size() == 2 &&
                compiler_result.diagnostics.size() == 1 &&
                compiler_result.diagnostics[0].severity == editor::ShaderDiagnosticSeverity::Warning,
            "Slang compiler integration did not produce a validated result");
    auto fake_compile = [artifact](const editor::ShaderCompileRequest &compile_request) mutable {
      editor::ShaderCompileResult result{true, artifact, {}};
      result.artifact.binary[0] = static_cast<std::byte>(compile_request.retire_fence + 1);
      return result;
    };
    editor::ShaderHotReloadController hot_reload{fake_compile};
    runtime::ShaderArtifactSlot hot_slot{rhi::Backend::Vulkan, layout_hash};
    request.retire_fence = 3;
    if (hot_slot.BuildMode() == runtime::ShaderBuildMode::Development) {
      Require(hot_reload.ReloadIfChanged(request, hot_slot, error) == editor::ShaderReloadStatus::Committed &&
                  hot_slot.Generation() == 1,
              "initial hot reload did not commit");
      Require(hot_reload.ReloadIfChanged(request, hot_slot, error) == editor::ShaderReloadStatus::NoChange,
              "unchanged shader source triggered a redundant compile");
      { std::ofstream source(source_path, std::ios::app); source << " changed"; }
      Require(hot_reload.ReloadIfChanged(request, hot_slot, error) == editor::ShaderReloadStatus::Committed &&
                  hot_slot.Generation() == 2 && hot_slot.RetiredCount() == 1,
              "changed shader source did not publish a fenced replacement");
    } else {
      Require(hot_reload.ReloadIfChanged(request, hot_slot, error) == editor::ShaderReloadStatus::Failed &&
                  hot_slot.Generation() == 0,
              "Shipping Runtime accepted dynamic hot reload");
    }
    hot_reload.Forget(source_path);
    std::error_code ignored;
    std::filesystem::remove(source_path, ignored);
    std::filesystem::remove(output_path, ignored);
    std::cout << "Editor shader authoring contracts passed\n";
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << exception.what() << '\n';
    return 1;
  }
}
