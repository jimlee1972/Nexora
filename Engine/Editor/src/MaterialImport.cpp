#include "Nexora/Editor/MaterialImport.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace nexora::editor {
MaterialExportResult ExportMaterial(const MaterialAsset &material) {
  const auto validation = ValidateMaterialAsset(material);
  if (!validation.valid)
    return {{}, validation.message};
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << std::setprecision(std::numeric_limits<float>::max_digits10)
         << "NEXORA_MATERIAL 1\nbase_color " << material.base_color[0] << ' '
         << material.base_color[1] << ' ' << material.base_color[2] << "\nmetallic "
         << material.metallic << "\nroughness " << material.roughness << "\nocclusion "
         << material.occlusion << "\nemission " << material.emission[0] << ' '
         << material.emission[1] << ' ' << material.emission[2] << '\n';
  if (!output)
    return {{}, "Material serialization failed."};
  auto source = output.str();
  if (source.size() > kMaximumCanonicalMaterialBytes)
    return {{}, "Canonical material exceeds its byte budget."};
  return {std::move(source), {}};
}

renderer::MaterialValidation ValidateMaterialAsset(const MaterialAsset &material) {
  const auto finite_scalar = [](float value, float maximum) {
    return std::isfinite(value) && value >= 0 && value <= maximum;
  };
  if (!std::ranges::all_of(material.base_color,
                           [&](float value) { return finite_scalar(value, 1); }) ||
      !std::ranges::all_of(material.emission,
                           [&](float value) { return finite_scalar(value, 65504); }) ||
      !finite_scalar(material.metallic, 1) || !finite_scalar(material.roughness, 1) ||
      !finite_scalar(material.occlusion, 1))
    return {false, "Scalar PBR material contains an invalid canonical value."};
  const auto &schema = material.schema;
  const auto features =
      std::ranges::any_of(material.emission, [](float value) { return value != 0; })
          ? renderer::FeatureBit(renderer::MaterialFeature::Emission)
          : 0;
  if (schema.shader_id != "nexora.editor.scalar-pbr" || schema.shader_profile != "default" ||
      schema.shading_model != renderer::ShadingModel::PBR ||
      schema.surface_mode != renderer::SurfaceMode::Opaque || !schema.textures.empty() ||
      schema.features != features)
    return {false, "Scalar PBR material has an unsupported Renderer schema."};
  const std::array<renderer::MaterialParameter, 5> expected{{{"base_color", material.base_color},
                                                             {"metallic", material.metallic},
                                                             {"roughness", material.roughness},
                                                             {"occlusion", material.occlusion},
                                                             {"emission", material.emission}}};
  if (schema.parameters.size() != expected.size())
    return {false, "Scalar PBR material reflection diverges from its canonical values."};
  for (std::size_t index = 0; index < expected.size(); ++index)
    if (schema.parameters[index].name != expected[index].name ||
        schema.parameters[index].value != expected[index].value)
      return {false, "Scalar PBR material reflection diverges from its canonical values."};
  return renderer::ValidateMaterial(schema);
}

MaterialImportResult ImportMaterial(std::string_view source,
                                    const std::function<bool()> &cancelled) {
  if (cancelled && cancelled())
    return {{}, {}, true};
  if (source.size() > kMaximumMaterialSourceBytes)
    return {{}, "Material source exceeds the 64 KiB limit.", false};
  std::istringstream input{std::string(source)};
  input.imbue(std::locale::classic());
  bool was_cancelled{};
  const auto check_cancelled = [&] {
    was_cancelled = cancelled && cancelled();
    return was_cancelled;
  };
  const auto token = [&](std::string_view expected) {
    if (check_cancelled())
      return false;
    std::string value;
    return static_cast<bool>(input >> value) && value == expected;
  };
  const auto scalar = [&](float &value, float maximum) {
    if (check_cancelled())
      return false;
    std::string number;
    if (!(input >> number))
      return false;
    std::string_view numeric = number;
    if (numeric.starts_with('+')) {
      numeric.remove_prefix(1);
      if (numeric.empty() || numeric.front() == '+' || numeric.front() == '-')
        return false;
    }
    const auto parsed = std::from_chars(numeric.data(), numeric.data() + numeric.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == numeric.data() + numeric.size() &&
           std::isfinite(value) && value >= 0 && value <= maximum;
  };
  const auto triple = [&](std::array<float, 3> &values, float maximum) {
    return std::ranges::all_of(values, [&](float &value) { return scalar(value, maximum); });
  };
  MaterialAsset material;
  if (!token("NEXORA_MATERIAL") || !token("1") || !token("base_color") ||
      !triple(material.base_color, 1) || !token("metallic") || !scalar(material.metallic, 1) ||
      !token("roughness") || !scalar(material.roughness, 1) || !token("occlusion") ||
      !scalar(material.occlusion, 1) || !token("emission") || !triple(material.emission, 65504))
    return {{},
            was_cancelled ? "" : "Invalid schema-1 scalar PBR material tokens or values.",
            was_cancelled};
  input >> std::ws;
  if (!input.eof())
    return {{}, "Material source contains extra tokens.", false};
  if (check_cancelled())
    return {{}, {}, true};
  material.schema.shader_id = "nexora.editor.scalar-pbr";
  material.schema.parameters = {{"base_color", material.base_color},
                                {"metallic", material.metallic},
                                {"roughness", material.roughness},
                                {"occlusion", material.occlusion},
                                {"emission", material.emission}};
  if (std::ranges::any_of(material.emission, [](float value) { return value != 0; }))
    material.schema.features = renderer::FeatureBit(renderer::MaterialFeature::Emission);
  const auto validation = ValidateMaterialAsset(material);
  if (!validation.valid)
    return {{}, validation.message, false};
  if (check_cancelled())
    return {{}, {}, true};
  return {std::move(material), {}, false};
}
} // namespace nexora::editor
