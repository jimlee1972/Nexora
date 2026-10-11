#include "Nexora/Editor/MaterialImport.h"
#include "Nexora/Editor/ScalarMaterialTool.h"
#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Foundation/EditorToolAbi.h"
#include "Nexora/Foundation/PluginAbi.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
std::uint32_t Read32(const std::uint8_t *p) {
  return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8U) | (std::uint32_t(p[2]) << 16U) |
         (std::uint32_t(p[3]) << 24U);
}
std::int32_t Invoke(void *, std::uint32_t operation, const std::uint8_t *input, std::uint32_t bytes,
                    std::uint8_t *output, std::uint32_t capacity, std::uint32_t *written) noexcept {
  using namespace nexora::editor;
  if (!written)
    return NEXORA_EDITOR_TOOL_REJECTED;
  *written = 0;
  if (bytes > 65536 || (bytes && !input) || (capacity && !output))
    return NEXORA_EDITOR_TOOL_REJECTED;
  if (operation != NEXORA_EDITOR_TOOL_INSPECT && operation != NEXORA_EDITOR_TOOL_EDIT &&
      operation != NEXORA_EDITOR_TOOL_SERIALIZE)
    return NEXORA_EDITOR_TOOL_UNAVAILABLE;
  try {
    std::uint32_t lane{};
    float scalar{};
    if (operation == NEXORA_EDITOR_TOOL_EDIT) {
      if (bytes < kScalarMaterialEditPrefixBytes || std::memcmp(input, "NXM1", 4) != 0)
        return NEXORA_EDITOR_TOOL_REJECTED;
      lane = Read32(input + 4);
      scalar = std::bit_cast<float>(Read32(input + 8));
      if (lane > 8 || !std::isfinite(scalar) || scalar < 0 || scalar > (lane >= 6 ? 65504.F : 1.F))
        return NEXORA_EDITOR_TOOL_REJECTED;
      input += kScalarMaterialEditPrefixBytes;
      bytes -= kScalarMaterialEditPrefixBytes;
    }
    if (!bytes)
      return NEXORA_EDITOR_TOOL_REJECTED;
    auto imported = ImportMaterial({reinterpret_cast<const char *>(input), bytes});
    if (!imported.material)
      return NEXORA_EDITOR_TOOL_REJECTED;
    auto &material = *imported.material;
    if (operation == NEXORA_EDITOR_TOOL_EDIT) {
      if (lane < 3)
        material.base_color[lane] = scalar;
      else if (lane == 3)
        material.metallic = scalar;
      else if (lane == 4)
        material.roughness = scalar;
      else if (lane == 5)
        material.occlusion = scalar;
      else
        material.emission[lane - 6] = scalar;
      // Derive the exact public Renderer reflection from canonical scalar values.
      material.schema.parameters[0].value = material.base_color;
      material.schema.parameters[1].value = material.metallic;
      material.schema.parameters[2].value = material.roughness;
      material.schema.parameters[3].value = material.occlusion;
      material.schema.parameters[4].value = material.emission;
      material.schema.features =
          std::ranges::any_of(material.emission, [](float value) { return value != 0; })
              ? nexora::renderer::FeatureBit(nexora::renderer::MaterialFeature::Emission)
              : 0;
    }
    auto exported = ExportMaterial(material);
    if (!exported.source)
      return NEXORA_EDITOR_TOOL_FAILED;
    if (exported.source->size() > capacity)
      return NEXORA_EDITOR_TOOL_REJECTED;
    std::memcpy(output, exported.source->data(), exported.source->size());
    *written = static_cast<std::uint32_t>(exported.source->size());
    return NEXORA_EDITOR_TOOL_SUCCESS;
  } catch (...) {
    return NEXORA_EDITOR_TOOL_FAILED;
  }
}
const NexoraEditorToolServiceV1 table{sizeof(NexoraEditorToolServiceV1),
                                      NEXORA_EDITOR_TOOL_SCHEMA_V1,
                                      NEXORA_EDITOR_TOOL_INTERFACE_V1,
                                      NEXORA_EDITOR_TOOL_INSPECT | NEXORA_EDITOR_TOOL_EDIT |
                                          NEXORA_EDITOR_TOOL_SERIALIZE,
                                      65536,
                                      1024,
                                      nullptr,
                                      Invoke};
// Stateless synchronous service: owner drains calls before shutdown; no workers/resources remain.
// Qualified host visibility revokes per admission, without global state affecting another host.
std::int32_t Shutdown(void *) noexcept { return 0; }
std::int32_t Quiescent(void *) noexcept { return 1; }
} // namespace
NEXORA_PLUGIN_ABI_EXPORT std::uint32_t NexoraPluginAbiVersion() noexcept {
  return nexora::foundation::kEngineAbiVersion;
}
NEXORA_PLUGIN_ABI_EXPORT void NexoraPluginRegister(void *context,
                                                   NexoraServiceRegisterCallback register_service) {
  register_service(context, nexora::editor::kScalarMaterialToolService.data(),
                   const_cast<NexoraEditorToolServiceV1 *>(&table));
}
NEXORA_PLUGIN_ABI_EXPORT std::int32_t
NexoraPluginGetLifecycleV1(std::uint32_t requested, NexoraPluginLifecycleV1 *lifecycle) noexcept {
  if (!lifecycle || lifecycle->struct_size < sizeof(*lifecycle) ||
      requested != NEXORA_PLUGIN_LIFECYCLE_SCHEMA_V1)
    return -1;
  *lifecycle = {sizeof(*lifecycle), NEXORA_PLUGIN_LIFECYCLE_SCHEMA_V1, nullptr, Shutdown,
                Quiescent};
  return 0;
}
