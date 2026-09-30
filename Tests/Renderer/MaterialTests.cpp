#include "Nexora/Renderer/Material.h"

#include <cstdlib>
#include <iostream>
#include <unordered_map>

namespace {
using namespace nexora::renderer;

void Require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
  }
}

class Resolver final : public MaterialResourceResolver {
public:
  std::unordered_map<std::string, std::uint32_t> resources;

  std::optional<std::uint32_t> ResolveTexture(std::string_view id) const override {
    const auto iterator = resources.find(std::string(id));
    return iterator == resources.end() ? std::nullopt
                                       : std::optional<std::uint32_t>(iterator->second);
  }
  std::uint32_t FallbackTexture(std::string_view semantic) const override {
    return semantic == "Normal" ? 2U : 1U;
  }
};

MaterialSchema MakeMaterial(ShadingModel model = ShadingModel::PBR) {
  return {.shader_id = "surface",
          .shading_model = model,
          .surface_mode = SurfaceMode::Opaque,
          .features = FeatureBit(MaterialFeature::IBL) | FeatureBit(MaterialFeature::ForwardPlus) |
                      FeatureBit(MaterialFeature::Instancing),
          .shader_profile = "desktop",
          .parameters = {{"Roughness", 0.5F},
                         {"BaseColor", std::array<float, 4>{1.0F, 0.5F, 0.25F, 1.0F}}},
          .textures = {{"BaseColor", "textures/brick"}, {"Normal", "textures/missing"}}};
}

void TestSchemaAndShadingModels() {
  for (const auto model : {ShadingModel::PBR, ShadingModel::StylizedPBR, ShadingModel::Anime,
                           ShadingModel::Vegetation, ShadingModel::Water, ShadingModel::Unlit}) {
    Require(ValidateMaterial(MakeMaterial(model)).valid, "every shading model must validate");
  }
  auto masked = MakeMaterial();
  masked.surface_mode = SurfaceMode::Masked;
  Require(!ValidateMaterial(masked).valid, "masked material must require alpha test");
  masked.features |= FeatureBit(MaterialFeature::AlphaTest);
  Require(ValidateMaterial(masked).valid, "masked alpha-test material must validate");
}

void TestVariantsAndStripping() {
  auto pbr = MakeMaterial();
  auto duplicate = pbr;
  auto anime = MakeMaterial(ShadingModel::Anime);
  const std::array materials{pbr, duplicate, anime};
  const auto variants = StripMaterialVariants(materials);
  Require(variants.size() == 2U, "only used unique variants must survive stripping");
  Require(HashMaterialVariant(SelectMaterialVariant(pbr)) !=
              HashMaterialVariant(SelectMaterialVariant(anime)),
          "shading model must participate in variant identity");
}

void TestBindingFallbackAndReflection() {
  Resolver resolver;
  resolver.resources.emplace("textures/brick", 42U);
  const auto material = MakeMaterial();
  const auto binding = BindMaterialResources(material, resolver);
  Require(binding.textures.size() == 2U, "all texture semantics must be bound");
  Require(binding.textures[0].resource_index == 42U && !binding.textures[0].used_fallback,
          "resident texture must resolve through the resource registry");
  Require(binding.textures[1].resource_index == 2U && binding.textures[1].used_fallback,
          "missing normal texture must use semantic fallback");

  const auto reflection = ReflectMaterial(material);
  Require(reflection.parameters.size() == 2U && reflection.texture_semantics.size() == 2U,
          "inspector reflection must expose parameters and textures");
  Require(reflection.parameters[1].byte_offset == 16U,
          "float4 inspector field must preserve constant-buffer alignment");
  Require(reflection.layout_hash != 0U && binding.layout_hash != 0U,
          "reflection and binding layouts must have stable hashes");
}

void TestTransactionalHotReload() {
  MaterialRegistry registry;
  const auto original = registry.Publish("materials/hero", MakeMaterial());
  Require(original.has_value() && registry.Get(*original) != nullptr,
          "valid material must publish");

  auto invalid = MakeMaterial();
  invalid.shader_id.clear();
  Require(!registry.Reload("materials/hero", std::move(invalid)).has_value(),
          "invalid reload must be rejected");
  Require(registry.Get(*original) != nullptr, "failed reload must retain the published generation");

  const auto reloaded = registry.Reload("materials/hero", MakeMaterial(ShadingModel::Anime));
  Require(reloaded.has_value() && reloaded->generation != original->generation,
          "successful hot reload must publish a new generation");
  Require(registry.Get(*original) == nullptr &&
              registry.Get(*reloaded)->shading_model == ShadingModel::Anime,
          "old handles must become stale after publication");
}
} // namespace

int main() {
  TestSchemaAndShadingModels();
  TestVariantsAndStripping();
  TestBindingFallbackAndReflection();
  TestTransactionalHotReload();
  return 0;
}
