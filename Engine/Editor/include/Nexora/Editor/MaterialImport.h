#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/Renderer/Material.h"

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace nexora::editor {

struct MaterialAsset final {
  // Scalar fields are canonical. The schema is their exact derived Renderer reflection view;
  // ValidateMaterialAsset rejects divergence and unsupported models/profiles/features.
  renderer::MaterialSchema schema;
  std::array<float, 3> base_color{1, 1, 1};
  std::array<float, 3> emission{};
  float metallic{};
  float roughness{.5F};
  float occlusion{1};
};

struct MaterialImportResult final {
  std::optional<MaterialAsset> material;
  std::string error;
  bool cancelled{};
};

// Schema-1 scalar opaque PBR tokens in fixed order. Owning results, no I/O/publication.
inline constexpr std::size_t kMaximumMaterialSourceBytes = 64 * 1024;
inline constexpr std::size_t kMaximumWorkspaceMaterials = 4096;
[[nodiscard]] NEXORA_EDITOR_API MaterialImportResult
ImportMaterial(std::string_view source, const std::function<bool()> &cancelled = {});
[[nodiscard]] NEXORA_EDITOR_API renderer::MaterialValidation
ValidateMaterialAsset(const MaterialAsset &material);

struct MaterialExportResult final {
  std::optional<std::string> source;
  std::string error;
};
// Canonical schema-1 scalar PBR source, round-tripping exact float values. No IO/publication.
inline constexpr std::size_t kMaximumCanonicalMaterialBytes = 1024;
[[nodiscard]] NEXORA_EDITOR_API MaterialExportResult ExportMaterial(const MaterialAsset &material);

} // namespace nexora::editor
