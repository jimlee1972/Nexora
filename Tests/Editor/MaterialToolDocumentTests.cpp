#include "Nexora/Editor/MaterialToolDocument.h"
#include "Nexora/Editor/NativeTool.h"
#include "Nexora/Editor/ScalarMaterialTool.h"
#include "Nexora/Foundation/BuildInfo.h"
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
using namespace editor;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::vector<std::byte> Bytes(std::string_view source) {
  const auto bytes = std::as_bytes(std::span(source.data(), source.size()));
  return {bytes.begin(), bytes.end()};
}
#if defined(NEXORA_TEST_SCALAR_MATERIAL)
std::vector<std::byte> Edit(std::string_view source, float roughness) {
  auto result = Bytes("NXM1");
  for (const auto value :
       {std::uint32_t(ScalarMaterialLane::Roughness), std::bit_cast<std::uint32_t>(roughness)})
    for (unsigned i = 0; i < 4; ++i)
      result.push_back(std::byte(value >> (i * 8)));
  const auto source_bytes = Bytes(source);
  result.insert(result.end(), source_bytes.begin(), source_bytes.end());
  return result;
}
#endif
MaterialToolSnapshot Read(const MaterialToolDocument &document) {
  auto snapshot = document.Snapshot();
  Require(snapshot.has_value(), "Owning document snapshot unavailable");
  return *snapshot;
}
void Conserved(const MaterialToolDocument &document, const MaterialToolSnapshot &expected) {
  const auto actual = Read(document);
  Require(actual.scope == expected.scope && actual.serial == expected.serial &&
              actual.source == expected.source && actual.saved_source == expected.saved_source &&
              actual.dirty == expected.dirty && actual.can_undo == expected.can_undo &&
              actual.can_redo == expected.can_redo &&
              std::bit_cast<std::uint32_t>(actual.material.base_color[0]) ==
                  std::bit_cast<std::uint32_t>(expected.material.base_color[0]),
          "Rejected/no-op operation changed source, history, baseline or exact scalar bits");
}
void Run(int argc, char **argv) {
  MaterialToolDocument document;
  const MaterialToolScope scope{{1, 2}, {3, 4}, 5};
  std::string source = "NEXORA_MATERIAL 1\nbase_color -0 .3 .4\nmetallic .5\nroughness .6\n"
                       "occlusion 1\nemission 0 0 0\n";
  source.resize(kMaximumMaterialSourceBytes, ' ');
  Require(!document.Snapshot() && document.Open(scope, source), "Exact source budget open failed");
  const auto original = Read(document);
  Require(!original.dirty && !original.can_undo && !original.can_redo &&
              original.saved_source == source && original.source.size() <= 1024 &&
              std::bit_cast<std::uint32_t>(original.material.base_color[0]) == 0x80000000U,
          "Opening canonicalized the saved bytes or lost negative zero");
  Require(!document.Open(scope, source + ' ') && !document.Open({{}, scope.asset, 5}, source) &&
              !document.Open({scope.project, {}, 5}, source) &&
              !document.Open({scope.project, scope.asset, 0}, source),
          "Invalid scope/source budget accepted");
  Conserved(document, original);
#if defined(NEXORA_TEST_SCALAR_MATERIAL)
  runtime::ServiceRegistry registry;
  runtime::PluginHost host(foundation::kEngineAbiVersion);
  NativeToolInvoker invoker;
  std::uint64_t admission{};
  Require(argc == 2, "Actual material module path missing");
  const auto loaded = host.Load(argv[1], &registry);
  Require(loaded.loaded && loaded.cooperative, "Actual material module failed load");
  admission = loaded.id;
#else
  static_cast<void>(argc);
  static_cast<void>(argv);
#endif
  const auto changed = [&](float value) {
    const auto current = Read(document);
#if defined(NEXORA_TEST_SCALAR_MATERIAL)
    auto result = invoker.Invoke(host, registry, admission, kScalarMaterialToolService,
                                 NativeToolOperation::Edit, Edit(current.source, value));
    Require(result.state == NativeToolState::Success && result.callback_result == 0,
            "Actual native material edit failed");
    return result.bytes;
#else
    auto material = current.material;
    material.roughness = value;
    material.schema.parameters[2].value = value;
    const auto exported = ExportMaterial(material);
    Require(exported.source.has_value(), "Public material transformation failed");
    return Bytes(*exported.source);
#endif
  };
  auto result = changed(.75F);
  Require(document.Apply(scope, original.serial, result, true), "Actual owning output not applied");
  auto edited = Read(document);
  Require(edited.dirty && edited.can_undo && !edited.can_redo &&
              edited.material.roughness == .75F && edited.saved_source == source &&
              edited.material.base_color == original.material.base_color,
          "Material edit changed unrelated values or baseline");
  Require(!document.Close(scope, edited.serial) && !document.Open(scope, source) &&
              !document.Open(scope, "NEXORA_MATERIAL 2", true),
          "Dirty discard/future schema failure changed document");
  Conserved(document, edited);
  for (unsigned cycle = 0; cycle < 40; ++cycle) {
    auto current = Read(document);
    Require(document.Undo(scope, current.serial, true), "Repeated Undo failed");
    current = Read(document);
    Require(current.source == original.source && !current.dirty && current.can_redo,
            "Undo did not recover exact canonical material");
    const auto no_op = Bytes(current.source + "  \n");
    Require(document.Apply(scope, current.serial, no_op, true), "Semantic no-op rejected");
    Conserved(document, current);
    auto stale = scope;
    ++stale.generation;
    Require(!document.Apply(stale, current.serial, result, true) &&
                !document.Apply(scope, current.serial - 1, result, true) &&
                !document.Apply(scope, current.serial, result, false) &&
                !document.Apply(scope, current.serial, Bytes("future schema"), true) &&
                !document.Apply(scope, current.serial, {}, true) &&
                !document.Apply(scope, current.serial,
                                std::vector<std::byte>(kMaximumMaterialSourceBytes + 1), true) &&
                !document.Undo(scope, current.serial, false) &&
                !document.Redo(scope, current.serial, false) &&
                !document.AcknowledgeSave(scope, current.serial, "stale", current.source) &&
                !document.AcknowledgeSave(scope, current.serial, current.saved_source,
                                          "NEXORA_MATERIAL 2"),
            "Invalid/read-only/stale operation accepted");
    Conserved(document, current);
    Require(document.Redo(scope, current.serial, true), "Pending Redo lost after failures/no-op");
    current = Read(document);
    Require(current.source == edited.source && current.dirty &&
                std::bit_cast<std::uint32_t>(current.material.base_color[0]) == 0x80000000U,
            "Redo lost exact material or negative zero");
  }
  auto before = Read(document);
  bool rejected{}, observed{};
  std::thread other([&] {
    observed = document.Snapshot().has_value();
    rejected = !document.Open(scope, source, true) &&
               !document.Apply(scope, before.serial, result, true) &&
               !document.Undo(scope, before.serial, true) &&
               !document.Redo(scope, before.serial, true) &&
               !document.AcknowledgeSave(scope, before.serial, source, before.source) &&
               !document.Close(scope, before.serial, true);
  });
  other.join();
  Require(rejected && !observed, "Foreign thread accessed material owner state");
  Conserved(document, before);
  for (unsigned i = 0; i < 70; ++i) {
    const auto current = Read(document);
    auto output = changed(float((i % 8) + 1) / 8);
    Require(document.Apply(scope, current.serial, output, true), "Bounded history edit failed");
  }
  const auto last = Read(document);
  unsigned undone{};
  while (Read(document).can_undo) {
    const auto current = Read(document);
    Require(document.Undo(scope, current.serial, true), "Bounded history Undo failed");
    ++undone;
  }
  Require(undone == MaterialToolDocument::kMaximumHistory &&
              Read(document).material.roughness == .75F,
          "History exceeded budget or retained wrong oldest state");
  unsigned redone{};
  while (Read(document).can_redo) {
    const auto current = Read(document);
    Require(document.Redo(scope, current.serial, true), "Bounded history Redo failed");
    ++redone;
  }
  Require(redone == undone && Read(document).source == last.source,
          "Bounded history failed exact restoration");
  before = Read(document);
  Require(!document.AcknowledgeSave(scope, before.serial, before.saved_source, original.source),
          "Different published document was acknowledged");
  Conserved(document, before);
  Require(document.AcknowledgeSave(scope, before.serial, before.saved_source, before.source),
          "Exact caller-confirmed save acknowledgement failed");
  auto saved = Read(document);
  Require(!saved.dirty && saved.saved_source == saved.source && saved.can_undo &&
              document.Undo(scope, saved.serial, true),
          "Save acknowledgement reset Undo or retained dirty baseline");
  saved = Read(document);
  Require(saved.dirty && document.Redo(scope, saved.serial, true) && !Read(document).dirty,
          "Undo/Redo failed acknowledged baseline conservation");
#if defined(NEXORA_TEST_SCALAR_MATERIAL)
  Require(host.RequestUnload(admission) == runtime::PluginState::Unloaded,
          "Actual material module failed unload");
  const auto unavailable =
      invoker.Invoke(host, registry, admission, kScalarMaterialToolService,
                     NativeToolOperation::Inspect, Bytes(Read(document).source));
  saved = Read(document);
  Require(unavailable.state == NativeToolState::Unavailable && unavailable.bytes.empty() &&
              !document.Apply(scope, saved.serial, unavailable.bytes, true),
          "Missing backend changed material payload");
  Conserved(document, saved);
#endif
  saved = Read(document);
  const auto retained = saved;
  Require(document.Close(scope, saved.serial) && !document.Snapshot() &&
              !document.Apply(scope, saved.serial, result, true) && document.Open(scope, source),
          "Close/reopen did not invalidate old observation");
  const auto reopened = Read(document);
  Require(reopened.serial > retained.serial && retained.source == last.source &&
              retained.material.roughness == .75F && reopened.source == original.source &&
              reopened.saved_source == source,
          "Owning snapshots expired or reopen changed original bytes");
#if defined(NEXORA_TEST_SCALAR_MATERIAL)
  std::cout << "Owning material document, exact native outcomes and bounded history verified.\n";
#else
  std::cout << "Owning material document and bounded history verified with native tool disabled.\n";
#endif
}
} // namespace
int main(int argc, char **argv) {
  try {
    Run(argc, argv);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
