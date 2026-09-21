#pragma once

#include "Nexora/Runtime/Api.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nexora::runtime {

struct NEXORA_RUNTIME_API AssetUuid final {
  std::uint64_t high{};
  std::uint64_t low{};
  [[nodiscard]] static std::optional<AssetUuid> Parse(std::string_view text) noexcept;
  [[nodiscard]] std::string ToString() const;
  friend bool operator==(const AssetUuid &, const AssetUuid &) = default;
};
struct NEXORA_RUNTIME_API AssetUuidHash final {
  std::size_t operator()(const AssetUuid &id) const noexcept;
};

using ByteBuffer = std::vector<std::byte>;
struct SourceAsset final {
  AssetUuid id;
  std::string type;
  std::string source_path;
  ByteBuffer bytes;
};
struct CanonicalAsset final {
  AssetUuid id;
  std::string type;
  std::vector<AssetUuid> dependencies;
  ByteBuffer payload;
};
struct RuntimeBlob final {
  static constexpr std::uint32_t kSchemaVersion = 1;
  AssetUuid id;
  std::string type;
  std::uint32_t schema_version{kSchemaVersion};
  std::vector<AssetUuid> dependencies;
  ByteBuffer payload;
  std::string content_hash;
};

class NEXORA_RUNTIME_API ImporterRegistry final {
public:
  using Importer = std::function<std::optional<CanonicalAsset>(const SourceAsset &)>;
  bool Register(std::string extension, Importer importer);
  [[nodiscard]] std::optional<CanonicalAsset> Import(const SourceAsset &source) const;

private:
  std::unordered_map<std::string, Importer> importers_;
};

class NEXORA_RUNTIME_API DerivedDataCache final {
public:
  void Store(std::string key, RuntimeBlob blob);
  [[nodiscard]] const RuntimeBlob *Find(std::string_view key) const;
  [[nodiscard]] std::size_t Size() const noexcept { return entries_.size(); }

private:
  std::unordered_map<std::string, RuntimeBlob> entries_;
};

class NEXORA_RUNTIME_API AssetCooker final {
public:
  [[nodiscard]] static std::string CacheKey(const CanonicalAsset &asset, std::string_view platform,
                                            std::string_view settings);
  [[nodiscard]] std::optional<RuntimeBlob> Cook(const CanonicalAsset &asset,
                                                std::string_view platform,
                                                std::string_view settings,
                                                DerivedDataCache &cache) const;
  [[nodiscard]] static ByteBuffer Serialize(const RuntimeBlob &blob);
  [[nodiscard]] static std::optional<RuntimeBlob> Deserialize(std::span<const std::byte> bytes);
};

struct BundleAsset final {
  AssetUuid id;
  std::string hash;
  std::uint64_t offset{};
  std::uint64_t size{};
};
struct BundleManifest final {
  std::string name;
  std::uint64_t generation{};
  std::vector<std::string> dependencies;
  std::vector<BundleAsset> assets;
};
struct Bundle final {
  BundleManifest manifest;
  ByteBuffer data;
};
class NEXORA_RUNTIME_API BundleBuilder final {
public:
  [[nodiscard]] static std::optional<Bundle> Build(std::string name, std::uint64_t generation,
                                                   std::vector<std::string> dependencies,
                                                   const std::vector<RuntimeBlob> &assets);
  [[nodiscard]] static bool Verify(const Bundle &bundle);
  [[nodiscard]] static bool ValidateDependencyDag(const std::vector<BundleManifest> &manifests,
                                                  std::vector<std::string> *cycle = nullptr);
};

class NEXORA_RUNTIME_API AssetGenerationStore final {
public:
  bool Stage(std::vector<Bundle> bundles);
  bool ActivateStaged();
  bool Rollback();
  bool Pin(std::uint64_t generation);
  bool Unpin(std::uint64_t generation);
  [[nodiscard]] const RuntimeBlob *Load(AssetUuid id, std::uint64_t generation = 0);
  void Release(AssetUuid id, std::uint64_t generation = 0);
  [[nodiscard]] bool IsResident(AssetUuid id, std::uint64_t generation = 0) const;
  [[nodiscard]] std::size_t ResidentBytes() const noexcept { return resident_bytes_; }
  [[nodiscard]] std::uint64_t ActiveGeneration() const noexcept { return active_; }
  [[nodiscard]] bool IsPinned(std::uint64_t generation) const;

private:
  struct Generation {
    std::vector<Bundle> bundles;
    std::unordered_map<AssetUuid, RuntimeBlob, AssetUuidHash> assets;
  };
  struct ResidentKey {
    AssetUuid id;
    std::uint64_t generation{};
    friend bool operator==(const ResidentKey &, const ResidentKey &) = default;
  };
  struct ResidentKeyHash {
    std::size_t operator()(const ResidentKey &key) const noexcept;
  };
  struct Resident {
    std::size_t references{};
    std::size_t bytes{};
  };
  void Collect();
  std::unordered_map<std::uint64_t, Generation> generations_;
  std::unordered_map<std::uint64_t, std::size_t> pins_;
  std::unordered_map<ResidentKey, Resident, ResidentKeyHash> residents_;
  std::uint64_t active_{}, previous_{}, staged_{};
  std::size_t resident_bytes_{};
};

struct DataTableRow final {
  std::string key;
  std::unordered_map<std::string, std::string> values;
};
class NEXORA_RUNTIME_API DataTable final {
public:
  bool Build(std::vector<DataTableRow> rows);
  [[nodiscard]] const DataTableRow *Find(std::string_view key) const;
  [[nodiscard]] std::size_t Size() const noexcept { return rows_.size(); }

private:
  std::vector<DataTableRow> rows_;
  std::unordered_map<std::string, std::size_t> index_;
};

[[nodiscard]] NEXORA_RUNTIME_API bool AssetPipelineEnabled() noexcept;
} // namespace nexora::runtime
