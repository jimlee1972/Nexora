#include "Nexora/Runtime/AssetPipeline.h"

#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora::runtime;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
ByteBuffer Bytes(std::string_view text) {
  return {reinterpret_cast<const std::byte *>(text.data()),
          reinterpret_cast<const std::byte *>(text.data() + text.size())};
}
AssetUuid Uuid(std::string_view value) {
  auto id = AssetUuid::Parse(value);
  Require(id.has_value(), "test UUID invalid");
  return *id;
}
RuntimeBlob Cook(AssetUuid id, std::string_view payload, DerivedDataCache &cache) {
  CanonicalAsset canonical{id, "mesh", {}, Bytes(payload)};
  auto result = AssetCooker{}.Cook(canonical, "test", "quality=high", cache);
  Require(result.has_value(), "cook failed");
  return *result;
}
int Run() {
  Require(AssetPipelineEnabled(), "pipeline unexpectedly stripped");
  const auto hero = Uuid("01234567-89ab-cdef-0123-456789abcdef");
  Require(hero.ToString() == "01234567-89ab-cdef-0123-456789abcdef" && !AssetUuid::Parse("bad"),
          "UUID contract failed");

  ImporterRegistry importers;
  Require(importers.Register("gltf",
                             [](const SourceAsset &source) -> std::optional<CanonicalAsset> {
                               if (source.bytes.empty())
                                 return std::nullopt;
                               return CanonicalAsset{source.id, "mesh", {}, source.bytes};
                             }),
          "importer registration failed");
  const SourceAsset source{hero, "model", "Assets/Hero.GLTF", Bytes("canonical mesh")};
  const auto canonical = importers.Import(source);
  Require(canonical.has_value() && !importers.Import({hero, "model", "Hero.fbx", Bytes("x")}),
          "import routing failed");

  DerivedDataCache cache;
  AssetCooker cooker;
  auto blob = cooker.Cook(*canonical, "linux-vulkan", "lod=2", cache);
  auto cached = cooker.Cook(*canonical, "linux-vulkan", "lod=2", cache);
  Require(blob && cached && cache.Size() == 1 && blob->content_hash == cached->content_hash,
          "DDC failed");
  auto encoded = AssetCooker::Serialize(*blob);
  auto decoded = AssetCooker::Deserialize(encoded);
  Require(decoded && decoded->payload == blob->payload, "runtime blob round trip failed");
  encoded.back() ^= std::byte{1};
  Require(!AssetCooker::Deserialize(encoded), "corrupt runtime blob accepted");

  auto first = BundleBuilder::Build("base.bundle", 1, {}, {*blob});
  Require(first && BundleBuilder::Verify(*first), "bundle build/verify failed");
  auto damaged = *first;
  damaged.data.back() ^= std::byte{1};
  Require(!BundleBuilder::Verify(damaged), "corrupt bundle accepted");
  std::vector<std::string> cycle;
  Require(
      !BundleBuilder::ValidateDependencyDag({{"a", 1, {"b"}, {}}, {"b", 1, {"a"}, {}}}, &cycle) &&
          cycle.size() == 3,
      "bundle cycle was accepted or not reported");

  AssetGenerationStore store;
  Require(store.Stage({*first}) && store.ActivateStaged() && store.ActiveGeneration() == 1,
          "generation one activation failed");
  Require(store.Pin(1), "generation pin failed");
  auto second_blob = Cook(hero, "new mesh", cache);
  auto second = BundleBuilder::Build("base.bundle", 2, {}, {second_blob});
  Require(second && store.Stage({*second}) && store.ActivateStaged(),
          "generation two activation failed");
  const auto *old = store.Load(hero, 1);
  const auto *current = store.Load(hero);
  Require(old && current && old->content_hash != current->content_hash && store.IsPinned(1),
          "active generation disrupted pinned reader");
  Require(store.ResidentBytes() == old->payload.size() + current->payload.size(),
          "residency accounting failed");
  Require(store.Rollback() && store.ActiveGeneration() == 1 &&
              store.Load(hero)->content_hash == old->content_hash,
          "rollback failed");
  store.Release(hero, 1);
  store.Release(hero, 1);
  store.Release(hero, 2);
  Require(store.ResidentBytes() == 0 && store.Unpin(1), "residency release failed");

  DataTable table;
  Require(table.Build({{"sword", {{"damage", "12"}}}, {"shield", {{"armor", "8"}}}}) &&
              table.Find("sword") && table.Find("sword")->values.at("damage") == "12",
          "data table lookup failed");
  Require(!table.Build({{"duplicate", {}}, {"duplicate", {}}}) && table.Find("sword"),
          "invalid table replaced previous good data");

  const auto started = std::chrono::steady_clock::now();
  for (std::uint64_t i = 1; i <= 10000; ++i) {
    const AssetUuid id{i, i + 1};
    CanonicalAsset item{id, "mesh", {}, Bytes("payload")};
    Require(cooker.Cook(item, "test", "", cache).has_value(), "bulk cook failed");
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started);
  Require(elapsed < std::chrono::seconds(10), "10k cook performance baseline exceeded");
  std::cout << "V1-M5 baseline: 10000 cached runtime blobs in " << elapsed.count() << " ms\n";
  return 0;
}
} // namespace
int main() {
  try {
    return Run();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
