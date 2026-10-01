#include "Nexora/Editor/ShaderAuthoring.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
} // namespace

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
    failed.diagnostics.push_back({editor::ShaderDiagnosticSeverity::Error, "Shaders/Triangle.slang",
                                  9, 4, "unknown identifier"});
    const auto active_generation = slot.Generation();
    const auto active_binary = slot.Active()->binary;
    Require(!editor::ApplyShaderCompileResult(failed, slot, error) &&
                slot.Generation() == active_generation && slot.Active()->binary == active_binary,
            "failed compile result replaced the active artifact");

    auto invalid_success = first;
    invalid_success.artifact.reflection.layout_hash++;
    Require(!editor::ApplyShaderCompileResult(invalid_success, slot, error) &&
                slot.Generation() == active_generation,
            "incompatible compiler output was published");

    const auto diagnostics = editor::ParseShaderDiagnostics(
        "Shaders/Test.slang:12:7: warning: slow path\nShaders/Test.slang:14:3: error: bad type\n",
        rhi::Backend::Vulkan, "SKINNED=1");
    Require(diagnostics.size() == 2 &&
                diagnostics[0].severity == editor::ShaderDiagnosticSeverity::Warning &&
                diagnostics[1].severity == editor::ShaderDiagnosticSeverity::Error &&
                diagnostics[1].line == 14 && diagnostics[1].column == 3 &&
                diagnostics[1].backend == rhi::Backend::Vulkan &&
                diagnostics[1].variant == "SKINNED=1",
            "Slang diagnostics were not parsed with source positions");

    // Native layout printed by the pinned slangc 2026.18 (captured verbatim, plus a gutter line
    // that quotes a location with an out-of-range number). Severity must come from the severity
    // token, echoed source must be ignored, and nothing may throw.
    const std::string native_output =
        "error[E30015]: undefined identifier\n"
        " --> Shaders/A.slang:5:15\n"
        "  |\n"
        "5 | float x = undefinedThing; // see f.slang:99999999999999999999999:1: note\n"
        "  |           ^^^^^^^^^^^^^^ undefined identifier 'undefinedThing'.\n"
        "--'\n"
        "warning[E30081]: implicit conversion of errorCount not recommended\n"
        " --> Shaders/A.slang:6:22\n";
    const auto native = editor::ParseShaderDiagnostics(native_output, rhi::Backend::Vulkan, "BASE");
    Require(native.size() == 2 && native[0].severity == editor::ShaderDiagnosticSeverity::Error &&
                native[0].file == "Shaders/A.slang" && native[0].line == 5 &&
                native[0].column == 15 && native[0].message == "undefined identifier" &&
                native[0].variant == "BASE" &&
                native[1].severity == editor::ShaderDiagnosticSeverity::Warning &&
                native[1].line == 6 && native[1].column == 22,
            "native Slang 2026 diagnostics were not parsed");
    const auto classified = editor::ParseShaderDiagnostics(
        "a.slang:3:1: warning: unused variable 'errorCount'\n"
        "b.slang:99999999999999999999:2: ERROR: overflowing line number\n");
    Require(classified.size() == 2 &&
                classified[0].severity == editor::ShaderDiagnosticSeverity::Warning &&
                classified[1].severity == editor::ShaderDiagnosticSeverity::Error &&
                classified[1].line == 0 && classified[1].column == 2,
            "diagnostic severity was inferred from message text or an overflow threw");

    const auto source_path =
        std::filesystem::temp_directory_path() / "nexora-hot-reload-test.slang";
    const auto output_path = std::filesystem::temp_directory_path() / "nexora-hot-reload-test.spv";
    {
      std::ofstream source(source_path);
      source << "shader";
    }
    editor::ShaderCompileRequest request;
    request.shader_id = "triangle";
    request.source_path = source_path;
    request.output_path = output_path;
    request.entry_points = {"vertexMain"};
    request.format = rhi::ShaderBinaryFormat::SpirV;
    request.reflection = rhi::TrianglePipelineLayout();
    request.backend = rhi::Backend::Vulkan;
    request.variant = "SKINNED=1";
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
                compiler_result.diagnostics[0].severity ==
                    editor::ShaderDiagnosticSeverity::Warning,
            "Slang compiler integration did not produce a validated result");

    // The default runner must report a launch failure instead of hiding it.
    const auto missing_compiler = editor::CompileSlang(request, {"/nonexistent/nexora-slangc"});
    Require(!missing_compiler.succeeded && !missing_compiler.diagnostics.empty() &&
                missing_compiler.diagnostics.back().message.find("unable to launch") !=
                    std::string::npos,
            "a compiler launch failure was not reported");
#if !defined(_WIN32)
    // Request fields come from project content and must never reach a shell. `true` ignores its
    // arguments, so the marker can only appear if an argument was interpreted as shell syntax.
    {
      const auto marker = std::filesystem::temp_directory_path() / "nexora-shader-injection-marker";
      std::error_code removed;
      std::filesystem::remove(marker, removed);
      auto hostile = request;
      hostile.include_directories = {"x$(touch " + marker.string() + ")`touch " + marker.string() +
                                     "`"};
      static_cast<void>(editor::CompileSlang(hostile, {"true"}));
      Require(!std::filesystem::exists(marker),
              "shader compiler arguments were interpreted by a shell");
    }
#endif

    editor::DevelopmentShaderCache cache{{1, 0}};
    Require(cache.Store(request, compiler_result, error) && cache.Find(request) != nullptr &&
                cache.Budget().used == 1,
            "development shader cache did not retain a compiled variant");
    auto other_include = request;
    other_include.include_directories = {"Shaders/Other"};
    Require(cache.Find(other_include) == nullptr,
            "a request with different include directories reused a cached artifact");
    // A save that lands while the compile runs must leave the entry stale: the snapshot taken
    // before compiling predates the new write time, so the next Find has to miss.
    {
      editor::DevelopmentShaderCache racy{{2, 0}};
      const auto before_compile = editor::CaptureShaderInputs(request);
      std::filesystem::last_write_time(source_path, std::filesystem::last_write_time(source_path) +
                                                        std::chrono::seconds(2));
      Require(racy.Store(request, compiler_result, before_compile, error) &&
                  racy.Find(request) == nullptr && racy.Budget().used == 0,
              "a result compiled from pre-edit inputs was cached as current");
    }
    auto second_request = request;
    second_request.variant = "SKINNED=0";
    Require(!cache.Store(second_request, compiler_result, error),
            "shader cache exceeded its variant budget");
    Require(cache.InvalidateDependency(source_path) == 1 && cache.Find(request) == nullptr &&
                cache.Budget().used == 0,
            "shader dependency invalidation did not evict dependent variants");
    auto fake_compile = [artifact](const editor::ShaderCompileRequest &compile_request) mutable {
      editor::ShaderCompileResult result{true, artifact, {}};
      result.artifact.binary[0] = static_cast<std::byte>(compile_request.retire_fence + 1);
      return result;
    };
    editor::ShaderHotReloadController hot_reload{fake_compile};
    runtime::ShaderArtifactSlot hot_slot{rhi::Backend::Vulkan, layout_hash};
    request.retire_fence = 3;
    if (hot_slot.BuildMode() == runtime::ShaderBuildMode::Development) {
      Require(hot_reload.ReloadIfChanged(request, hot_slot, error) ==
                      editor::ShaderReloadStatus::Committed &&
                  hot_slot.Generation() == 1,
              "initial hot reload did not commit");
      Require(hot_reload.ReloadIfChanged(request, hot_slot, error) ==
                  editor::ShaderReloadStatus::NoChange,
              "unchanged shader source triggered a redundant compile");
      // A second variant of the same source is tracked independently: observing the source for
      // one request must not suppress the other's build.
      auto other_variant = request;
      other_variant.variant = "SKINNED=0";
      runtime::ShaderArtifactSlot other_slot{rhi::Backend::Vulkan, layout_hash};
      Require(hot_reload.ReloadIfChanged(other_variant, other_slot, error) ==
                      editor::ShaderReloadStatus::Committed &&
                  other_slot.Generation() == 1,
              "a second variant of an already-observed source was never built");
      {
        std::ofstream source(source_path, std::ios::app);
        source << " changed";
      }
      Require(hot_reload.ReloadIfChanged(request, hot_slot, error) ==
                      editor::ShaderReloadStatus::Committed &&
                  hot_slot.Generation() == 2 && hot_slot.RetiredCount() == 1,
              "changed shader source did not publish a fenced replacement");
      Require(hot_reload.ReloadIfChanged(other_variant, other_slot, error) ==
                      editor::ShaderReloadStatus::Committed &&
                  other_slot.Generation() == 2,
              "a source edit was consumed by one variant and never rebuilt the other");

      // Editing a declared dependency (an included file) must trigger a reload too.
      const auto include_path =
          std::filesystem::temp_directory_path() / "nexora-hot-reload-include.slang";
      {
        std::ofstream include(include_path);
        include << "include";
      }
      auto with_include = request;
      with_include.variant = "INCLUDE=1";
      with_include.dependencies = {include_path};
      runtime::ShaderArtifactSlot include_slot{rhi::Backend::Vulkan, layout_hash};
      Require(hot_reload.ReloadIfChanged(with_include, include_slot, error) ==
                      editor::ShaderReloadStatus::Committed &&
                  hot_reload.ReloadIfChanged(with_include, include_slot, error) ==
                      editor::ShaderReloadStatus::NoChange,
              "dependency-tracking request did not settle");
      std::filesystem::last_write_time(
          include_path, std::filesystem::last_write_time(include_path) + std::chrono::seconds(2));
      Require(hot_reload.ReloadIfChanged(with_include, include_slot, error) ==
                      editor::ShaderReloadStatus::Committed &&
                  include_slot.Generation() == 2,
              "editing a declared dependency did not trigger a hot reload");
      std::error_code include_removed;
      std::filesystem::remove(include_path, include_removed);
    } else {
      Require(hot_reload.ReloadIfChanged(request, hot_slot, error) ==
                      editor::ShaderReloadStatus::Failed &&
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
