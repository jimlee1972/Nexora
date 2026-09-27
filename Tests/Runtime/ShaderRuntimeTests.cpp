#include "Nexora/Runtime/ShaderRuntime.h"

#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

nexora::rhi::ShaderModuleArtifact MakeArtifact(nexora::rhi::Backend backend,
                                               std::uint64_t layout_hash) {
  using namespace nexora::rhi;
  ShaderModuleArtifact artifact;
  artifact.shader_id = "triangle";
  artifact.format = backend == Backend::Direct3D12 ? ShaderBinaryFormat::Dxil
                    : backend == Backend::Metal    ? ShaderBinaryFormat::MetalSource
                                                    : ShaderBinaryFormat::SpirV;
  artifact.entry_point = "vertexMain";
  artifact.binary = {std::byte{0x01}, std::byte{0x02}};
  artifact.reflection = TrianglePipelineLayout();
  artifact.reflection.layout_hash = layout_hash;
  return artifact;
}

void Run() {
  using namespace nexora;
  const auto layout_hash = rhi::TrianglePipelineLayout().layout_hash;
  for (const auto backend : {rhi::Backend::Direct3D12, rhi::Backend::Vulkan, rhi::Backend::Metal}) {
    auto artifact = MakeArtifact(backend, layout_hash);
    Require(rhi::IsArtifactCompatible(artifact, backend, layout_hash),
            "target artifact or canonical reflection was rejected");
    auto wrong_target = artifact;
    wrong_target.format = backend == rhi::Backend::Direct3D12 ? rhi::ShaderBinaryFormat::SpirV
                                                              : rhi::ShaderBinaryFormat::Dxil;
    Require(!rhi::IsArtifactCompatible(wrong_target, backend, layout_hash),
            "artifact from another backend was accepted");
  }

  runtime::ShaderArtifactSlot development{rhi::Backend::Vulkan, layout_hash};
  std::string error;
  auto first = MakeArtifact(rhi::Backend::Vulkan, layout_hash);
  Require(development.Stage(first, runtime::ShaderArtifactSource::Cooked, error) &&
              development.Commit(error) && development.Generation() == 1,
          "initial cooked artifact did not commit");
  auto replacement = MakeArtifact(rhi::Backend::Vulkan, layout_hash);
  replacement.binary[0] = std::byte{0x03};
  if (development.BuildMode() == runtime::ShaderBuildMode::Development) {
    Require(development.Stage(replacement, runtime::ShaderArtifactSource::DynamicCompile, error),
            "Development Runtime rejected dynamic Slang output");
    const auto active_before_commit = development.Active()->binary[0];
    Require(active_before_commit == std::byte{0x01} && development.Generation() == 1,
            "staging changed the active artifact before commit");
    Require(development.Commit(error) && development.Generation() == 2 &&
                development.Active()->binary[0] == std::byte{0x03},
            "validated hot reload did not publish atomically");
  } else {
    Require(!development.Stage(replacement, runtime::ShaderArtifactSource::DynamicCompile, error) &&
                development.Active()->binary[0] == std::byte{0x01},
            "Shipping Runtime admitted dynamic Slang output");
  }

  auto invalid = MakeArtifact(rhi::Backend::Vulkan, layout_hash + 1);
  Require(!development.Stage(invalid, runtime::ShaderArtifactSource::Cooked, error) &&
              development.Active()->binary[0] ==
                  (development.BuildMode() == runtime::ShaderBuildMode::Development
                       ? std::byte{0x03}
                       : std::byte{0x01}),
          "incompatible reflection replaced the active artifact");

  runtime::ShaderArtifactSlot fresh{rhi::Backend::Vulkan, layout_hash};
  if (fresh.BuildMode() == runtime::ShaderBuildMode::Shipping) {
    Require(!fresh.Stage(MakeArtifact(rhi::Backend::Vulkan, layout_hash),
                         runtime::ShaderArtifactSource::DynamicCompile, error) &&
                fresh.Active() == nullptr,
            "Shipping Runtime admitted dynamic compilation");
  }
  Require(fresh.Stage(MakeArtifact(rhi::Backend::Vulkan, layout_hash),
                      runtime::ShaderArtifactSource::Cooked, error) &&
              fresh.Commit(error),
          "Runtime rejected cooked artifacts");
}
} // namespace

int main() {
  try {
    Run();
    std::cout << "Shader runtime artifact contracts passed\n";
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << exception.what() << '\n';
    return 1;
  }
}
