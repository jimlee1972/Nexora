#include "Nexora/Editor/MaterialImport.h"
#include "Nexora/Editor/NativeTool.h"
#include "Nexora/Editor/ScalarMaterialTool.h"
#include "Nexora/Foundation/BuildInfo.h"

#include <bit>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void Require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}
std::vector<std::byte> Bytes(std::string_view source) {
  const auto b = std::as_bytes(std::span(source.data(), source.size()));
  return {b.begin(), b.end()};
}
std::string Text(const std::vector<std::byte> &bytes) {
  return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}
std::vector<std::byte> Edit(std::string_view source, std::uint32_t lane, float value) {
  auto result = Bytes("NXM1");
  for (auto integer : {lane, std::bit_cast<std::uint32_t>(value)})
    for (unsigned shift = 0; shift < 32; shift += 8)
      result.push_back(static_cast<std::byte>((integer >> shift) & 255U));
  auto text = Bytes(source);
  result.insert(result.end(), text.begin(), text.end());
  return result;
}
} // namespace
int main(int argc, char **argv) {
  using namespace nexora::editor;
  using namespace nexora::runtime;
  Require(argc == 2, "Actual production material plugin path required");
  const std::string source = "NEXORA_MATERIAL 1\nbase_color .2 .3 .4\nmetallic .6\n"
                             "roughness .7\nocclusion .8\nemission 0 0 0\n";
  const auto input = Bytes(source);
  auto material = ImportMaterial(source);
  Require(material.material.has_value(), "Production source did not import");
  auto canonical = ExportMaterial(*material.material);
  Require(canonical.source && canonical.source->size() <= kMaximumCanonicalMaterialBytes,
          "Public canonical material serialization failed");
  auto roundtrip = ImportMaterial(*canonical.source);
  Require(roundtrip.material && roundtrip.material->base_color == material.material->base_color &&
              roundtrip.material->metallic == material.material->metallic &&
              roundtrip.material->roughness == material.material->roughness &&
              roundtrip.material->occlusion == material.material->occlusion &&
              roundtrip.material->emission == material.material->emission &&
              ExportMaterial(*roundtrip.material).source == canonical.source,
          "Canonical exact scalar roundtrip or idempotence failed");
  auto invalid = *material.material;
  invalid.roughness = std::numeric_limits<float>::quiet_NaN();
  Require(!ExportMaterial(invalid).source, "Invalid canonical material serialized");
  invalid = *material.material;
  invalid.schema.parameters[0].name = "wrong";
  Require(!ExportMaterial(invalid).source, "Divergent Renderer schema serialized");
  for (float scalar : {0.F, -0.F, std::numeric_limits<float>::denorm_min(),
                       std::numeric_limits<float>::min(), std::nextafter(1.F, 0.F), 1.F}) {
    auto edge = *material.material;
    edge.roughness = scalar;
    edge.schema.parameters[2].value = scalar;
    const auto encoded = ExportMaterial(edge);
    Require(encoded.source.has_value(), "Valid boundary scalar did not serialize");
    const auto decoded = ImportMaterial(*encoded.source);
    Require(decoded.material && std::bit_cast<std::uint32_t>(decoded.material->roughness) ==
                                    std::bit_cast<std::uint32_t>(scalar),
            "Canonical boundary float did not roundtrip exact bits");
  }

  PluginHost host(nexora::foundation::kEngineAbiVersion);
  ServiceRegistry registry;
  NativeToolInvoker invoker;
  auto result = invoker.Invoke(host, registry, 1, kScalarMaterialToolService,
                               NativeToolOperation::Inspect, input);
  Require(result.state == NativeToolState::Unavailable && input == Bytes(source),
          "Absent backend consumed the owning source");
  const auto admission = host.Load(argv[1], &registry);
  Require(admission.loaded && admission.registered && admission.cooperative,
          "Actual production material plugin did not load");
  const ServiceRegistry copy = registry;
  result = invoker.Invoke(host, registry, admission.id, kScalarMaterialToolService,
                          NativeToolOperation::Inspect, input);
  Require(result.state == NativeToolState::Success && Text(result.bytes) == *canonical.source,
          "Native production inspection did not use public material backend");
  const auto retained = result;
  for (std::uint32_t lane = 0; lane < 9; ++lane) {
    const float value = lane >= 6 ? 65504.F : .125F;
    result = invoker.Invoke(host, copy, admission.id, kScalarMaterialToolService,
                            NativeToolOperation::Edit, Edit(source, lane, value));
    Require(result.state == NativeToolState::Success, "Actual native scalar edit rejected");
    const auto changed = ImportMaterial(Text(result.bytes));
    Require(changed.material && ValidateMaterialAsset(*changed.material).valid,
            "Native candidate did not validate against production Renderer");
    auto expected = *material.material;
    if (lane < 3)
      expected.base_color[lane] = value;
    else if (lane == 3)
      expected.metallic = value;
    else if (lane == 4)
      expected.roughness = value;
    else if (lane == 5)
      expected.occlusion = value;
    else
      expected.emission[lane - 6] = value;
    Require(changed.material->base_color == expected.base_color &&
                changed.material->metallic == expected.metallic &&
                changed.material->roughness == expected.roughness &&
                changed.material->occlusion == expected.occlusion &&
                changed.material->emission == expected.emission && input == Bytes(source),
            "Native edit changed an unselected scalar or original host bytes");
    const auto serialized = invoker.Invoke(host, registry, admission.id, kScalarMaterialToolService,
                                           NativeToolOperation::Serialize, result.bytes);
    Require(serialized.state == NativeToolState::Success && serialized.bytes == result.bytes,
            "Native edited candidate serialization was not idempotent");
  }
  result = invoker.Invoke(host, registry, admission.id, kScalarMaterialToolService,
                          NativeToolOperation::Preview, input);
  Require(result.state == NativeToolState::Unavailable && !result.callback_result,
          "Unimplemented GPU preview falsely became available");
  auto corrupt = Edit(source, 4, .5F);
  corrupt[0] = std::byte{0};
  const std::vector<std::vector<std::byte>> rejected{
      {},
      Bytes("NXM1"),
      corrupt,
      Edit(source, 9, .5F),
      Edit(source, 4, -1),
      Edit(source, 4, 1.001F),
      Edit(source, 7, 65505.F),
      Edit(source, 4, std::numeric_limits<float>::infinity()),
      Edit(source, 4, std::numeric_limits<float>::quiet_NaN()),
      Edit("corrupt", 4, .5F)};
  for (const auto &request : rejected) {
    result = invoker.Invoke(host, registry, admission.id, kScalarMaterialToolService,
                            NativeToolOperation::Edit, request);
    Require(result.state == NativeToolState::Rejected && result.bytes.empty() &&
                retained.bytes == Bytes(*canonical.source) && input == Bytes(source),
            "Rejected actual material edit leaked output or mutated owning inputs");
  }
  std::string exact = source;
  exact.resize(65536 - kScalarMaterialEditPrefixBytes, ' ');
  result = invoker.Invoke(host, registry, admission.id, kScalarMaterialToolService,
                          NativeToolOperation::Edit, Edit(exact, 4, .5F));
  Require(result.state == NativeToolState::Success, "Exact 64 KiB edit request rejected");
  exact.push_back(' ');
  result = invoker.Invoke(host, registry, admission.id, kScalarMaterialToolService,
                          NativeToolOperation::Edit, Edit(exact, 4, .5F));
  Require(result.state == NativeToolState::Invalid && !result.callback_result,
          "Oversized edit reached the production plugin");
  // Exercise the actual C callback's output admission without modifying its immutable table.
  const auto *table = static_cast<const NexoraEditorToolServiceV1 *>(
      host.FindService(admission.id, registry, kScalarMaterialToolService));
  Require(table != nullptr, "Actual production table missing");
  std::uint8_t output = 77;
  std::uint32_t written = 99;
  Require(table->invoke(table->context, NEXORA_EDITOR_TOOL_SERIALIZE,
                        reinterpret_cast<const std::uint8_t *>(input.data()), input.size(), &output,
                        1, &written) == NEXORA_EDITOR_TOOL_REJECTED &&
              output == 77 && written == 0,
          "Insufficient actual native output capacity changed caller storage");
  table = nullptr;
  PluginHost other_host(nexora::foundation::kEngineAbiVersion);
  ServiceRegistry other_registry;
  const auto other_admission = other_host.Load(argv[1], &other_registry);
  Require(other_admission.loaded && other_admission.id == admission.id,
          "Same-module second host did not load with the same numeric ID");
  Require(host.RequestUnload(admission.id) == PluginState::Unloaded,
          "Production tool did not cooperatively unload");
  result = invoker.Invoke(host, copy, admission.id, kScalarMaterialToolService,
                          NativeToolOperation::Inspect, input);
  Require(result.state == NativeToolState::Unavailable &&
              retained.bytes == Bytes(*canonical.source),
          "Unloaded material provider consumed source or invalidated owned inspection");
  result = invoker.Invoke(other_host, other_registry, other_admission.id,
                          kScalarMaterialToolService, NativeToolOperation::Inspect, input);
  Require(result.state == NativeToolState::Success && result.bytes == retained.bytes &&
              other_host.RequestUnload(other_admission.id) == PluginState::Unloaded,
          "One host shutdown disabled another host's stateless native material provider");
  ServiceRegistry reloaded_registry;
  const auto reload = host.Load(argv[1], &reloaded_registry);
  Require(reload.loaded && reload.id != admission.id, "Production tool did not reload");
  result = invoker.Invoke(host, reloaded_registry, admission.id, kScalarMaterialToolService,
                          NativeToolOperation::Inspect, input);
  Require(result.state == NativeToolState::Unavailable, "Retired material admission revived");
  result = invoker.Invoke(host, reloaded_registry, reload.id, kScalarMaterialToolService,
                          NativeToolOperation::Inspect, input);
  Require(result.state == NativeToolState::Success && result.bytes == retained.bytes &&
              host.RequestUnload(reload.id) == PluginState::Unloaded,
          "Reloaded material tool failed");
}
