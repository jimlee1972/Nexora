#include "Nexora/Runtime/AssetPipeline.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <limits>
#include <unordered_set>

namespace nexora::runtime {
namespace {
std::uint64_t HashBytes(std::span<const std::byte> bytes,
                        std::uint64_t hash = 1469598103934665603ULL) {
  for (const auto byte : bytes) {
    hash ^= std::to_integer<unsigned char>(byte);
    hash *= 1099511628211ULL;
  }
  return hash;
}
std::uint64_t HashText(std::string_view text, std::uint64_t hash = 1469598103934665603ULL) {
  return HashBytes({reinterpret_cast<const std::byte *>(text.data()), text.size()}, hash);
}
std::string Hex(std::uint64_t value) {
  constexpr char digits[] = "0123456789abcdef";
  std::string out(16, '0');
  for (int i = 15; i >= 0; --i) {
    out[static_cast<std::size_t>(i)] = digits[value & 15U];
    value >>= 4U;
  }
  return out;
}
std::string ContentHash(std::span<const std::byte> bytes) { return Hex(HashBytes(bytes)); }
template <class T> void Append(ByteBuffer &out, T value) {
  const auto *begin = reinterpret_cast<const std::byte *>(&value);
  out.insert(out.end(), begin, begin + sizeof(T));
}
void AppendString(ByteBuffer &out, std::string_view text) {
  Append(out, static_cast<std::uint32_t>(text.size()));
  out.insert(out.end(), reinterpret_cast<const std::byte *>(text.data()),
             reinterpret_cast<const std::byte *>(text.data() + text.size()));
}
template <class T> bool Read(std::span<const std::byte> in, std::size_t &at, T &value) {
  if (at > in.size() || in.size() - at < sizeof(T))
    return false;
  std::memcpy(&value, in.data() + at, sizeof(T));
  at += sizeof(T);
  return true;
}
bool ReadString(std::span<const std::byte> in, std::size_t &at, std::string &value) {
  std::uint32_t size{};
  if (!Read(in, at, size) || size > in.size() - at)
    return false;
  value.assign(reinterpret_cast<const char *>(in.data() + at), size);
  at += size;
  return true;
}
std::string Extension(std::string_view path) {
  const auto slash = path.find_last_of("/\\"), dot = path.find_last_of('.');
  if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash))
    return {};
  std::string result(path.substr(dot));
  std::ranges::transform(result, result.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}
} // namespace
std::optional<AssetUuid> AssetUuid::Parse(std::string_view text) noexcept {
  std::array<char, 32> compact{};
  std::size_t count{};
  for (char c : text)
    if (c != '-') {
      if (count == compact.size() || !std::isxdigit(static_cast<unsigned char>(c)))
        return std::nullopt;
      compact[count++] = c;
    }
  if (count != compact.size())
    return std::nullopt;
  AssetUuid id;
  auto parse = [&](std::size_t offset, std::uint64_t &part) {
    const auto result =
        std::from_chars(compact.data() + offset, compact.data() + offset + 16, part, 16);
    return result.ec == std::errc{};
  };
  if (!parse(0, id.high) || !parse(16, id.low) || (id.high == 0 && id.low == 0))
    return std::nullopt;
  return id;
}
std::string AssetUuid::ToString() const {
  const auto raw = Hex(high) + Hex(low);
  return raw.substr(0, 8) + "-" + raw.substr(8, 4) + "-" + raw.substr(12, 4) + "-" +
         raw.substr(16, 4) + "-" + raw.substr(20);
}
std::size_t AssetUuidHash::operator()(const AssetUuid &id) const noexcept {
  return static_cast<std::size_t>(
      id.high ^ (id.low + 0x9e3779b97f4a7c15ULL + (id.high << 6U) + (id.high >> 2U)));
}
bool ImporterRegistry::Register(std::string extension, Importer importer) {
  if (extension.empty() || !importer)
    return false;
  if (extension.front() != '.')
    extension.insert(extension.begin(), '.');
  std::ranges::transform(extension, extension.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return importers_.emplace(std::move(extension), std::move(importer)).second;
}
std::optional<CanonicalAsset> ImporterRegistry::Import(const SourceAsset &source) const {
  const auto found = importers_.find(Extension(source.source_path));
  if (found == importers_.end())
    return std::nullopt;
  auto result = found->second(source);
  if (!result || result->id != source.id || result->type.empty())
    return std::nullopt;
  return result;
}
void DerivedDataCache::Store(std::string key, RuntimeBlob blob) {
  entries_.insert_or_assign(std::move(key), std::move(blob));
}
const RuntimeBlob *DerivedDataCache::Find(std::string_view key) const {
  const auto found = entries_.find(std::string(key));
  return found == entries_.end() ? nullptr : &found->second;
}
std::string AssetCooker::CacheKey(const CanonicalAsset &asset, std::string_view platform,
                                  std::string_view settings) {
  std::uint64_t hash = HashText(asset.id.ToString());
  hash = HashText(asset.type, hash);
  hash = HashText(platform, hash);
  hash = HashText(settings, hash);
  hash = HashBytes(asset.payload, hash);
  for (const auto &dep : asset.dependencies)
    hash = HashText(dep.ToString(), hash);
  return Hex(hash);
}
std::optional<RuntimeBlob> AssetCooker::Cook(const CanonicalAsset &asset, std::string_view platform,
                                             std::string_view settings,
                                             DerivedDataCache &cache) const {
  if (asset.type.empty() || asset.payload.empty() || platform.empty())
    return std::nullopt;
  const auto key = CacheKey(asset, platform, settings);
  if (const auto *cached = cache.Find(key))
    return *cached;
  RuntimeBlob blob{asset.id,           asset.type,    RuntimeBlob::kSchemaVersion,
                   asset.dependencies, asset.payload, {}};
  blob.content_hash = ContentHash(blob.payload);
  cache.Store(key, blob);
  return blob;
}
ByteBuffer AssetCooker::Serialize(const RuntimeBlob &blob) {
  ByteBuffer out;
  for (char c : std::string_view("NXAB"))
    out.push_back(static_cast<std::byte>(c));
  Append(out, blob.schema_version);
  Append(out, blob.id.high);
  Append(out, blob.id.low);
  AppendString(out, blob.type);
  Append(out, static_cast<std::uint32_t>(blob.dependencies.size()));
  for (const auto &dep : blob.dependencies) {
    Append(out, dep.high);
    Append(out, dep.low);
  }
  AppendString(out, blob.content_hash);
  Append(out, static_cast<std::uint64_t>(blob.payload.size()));
  out.insert(out.end(), blob.payload.begin(), blob.payload.end());
  return out;
}
std::optional<RuntimeBlob> AssetCooker::Deserialize(std::span<const std::byte> bytes) {
  if (bytes.size() < 4 || std::memcmp(bytes.data(), "NXAB", 4) != 0)
    return std::nullopt;
  std::size_t at = 4;
  RuntimeBlob blob;
  std::uint32_t deps{};
  std::uint64_t payload_size{};
  if (!Read(bytes, at, blob.schema_version) || blob.schema_version != RuntimeBlob::kSchemaVersion ||
      !Read(bytes, at, blob.id.high) || !Read(bytes, at, blob.id.low) ||
      !ReadString(bytes, at, blob.type) || !Read(bytes, at, deps) || deps > 1000000)
    return std::nullopt;
  blob.dependencies.resize(deps);
  for (auto &dep : blob.dependencies)
    if (!Read(bytes, at, dep.high) || !Read(bytes, at, dep.low))
      return std::nullopt;
  if (!ReadString(bytes, at, blob.content_hash) || !Read(bytes, at, payload_size) ||
      payload_size != bytes.size() - at)
    return std::nullopt;
  blob.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(at), bytes.end());
  if (blob.type.empty() || blob.content_hash != ContentHash(blob.payload))
    return std::nullopt;
  return blob;
}
std::optional<Bundle> BundleBuilder::Build(std::string name, std::uint64_t generation,
                                           std::vector<std::string> dependencies,
                                           const std::vector<RuntimeBlob> &assets) {
  if (name.empty() || generation == 0 || assets.empty())
    return std::nullopt;
  Bundle out{{std::move(name), generation, std::move(dependencies), {}}, {}};
  std::unordered_set<AssetUuid, AssetUuidHash> ids;
  for (const auto &asset : assets) {
    if (!ids.insert(asset.id).second || asset.content_hash != ContentHash(asset.payload))
      return std::nullopt;
    auto bytes = AssetCooker::Serialize(asset);
    out.manifest.assets.push_back({asset.id, ContentHash(bytes), out.data.size(), bytes.size()});
    out.data.insert(out.data.end(), bytes.begin(), bytes.end());
  }
  return out;
}
bool BundleBuilder::Verify(const Bundle &bundle) {
  if (bundle.manifest.name.empty() || bundle.manifest.generation == 0)
    return false;
  std::uint64_t expected{};
  std::unordered_set<AssetUuid, AssetUuidHash> ids;
  for (const auto &asset : bundle.manifest.assets) {
    if (asset.offset != expected ||
        asset.size >
            bundle.data.size() - std::min<std::uint64_t>(asset.offset, bundle.data.size()) ||
        asset.offset > bundle.data.size() || !ids.insert(asset.id).second)
      return false;
    auto bytes =
        std::span(bundle.data)
            .subspan(static_cast<std::size_t>(asset.offset), static_cast<std::size_t>(asset.size));
    auto blob = AssetCooker::Deserialize(bytes);
    if (!blob || blob->id != asset.id || ContentHash(bytes) != asset.hash)
      return false;
    expected = asset.offset + asset.size;
  }
  return expected == bundle.data.size();
}
bool BundleBuilder::ValidateDependencyDag(const std::vector<BundleManifest> &manifests,
                                          std::vector<std::string> *cycle) {
  std::unordered_map<std::string, const BundleManifest *> graph;
  for (const auto &m : manifests)
    if (m.name.empty() || !graph.emplace(m.name, &m).second)
      return false;
  std::unordered_set<std::string> done, visiting;
  std::vector<std::string> path;
  std::function<bool(const std::string &)> visit = [&](const std::string &name) {
    if (done.contains(name))
      return true;
    if (visiting.contains(name)) {
      if (cycle) {
        auto it = std::find(path.begin(), path.end(), name);
        cycle->assign(it, path.end());
        cycle->push_back(name);
      }
      return false;
    }
    auto it = graph.find(name);
    if (it == graph.end())
      return false;
    visiting.insert(name);
    path.push_back(name);
    for (const auto &dep : it->second->dependencies)
      if (!visit(dep))
        return false;
    path.pop_back();
    visiting.erase(name);
    done.insert(name);
    return true;
  };
  for (const auto &[name, _] : graph)
    if (!visit(name))
      return false;
  return true;
}
std::size_t
AssetGenerationStore::ResidentKeyHash::operator()(const ResidentKey &key) const noexcept {
  return AssetUuidHash{}(key.id) ^ static_cast<std::size_t>(key.generation);
}
bool AssetGenerationStore::Stage(std::vector<Bundle> bundles) {
  if (bundles.empty())
    return false;
  const auto generation = bundles.front().manifest.generation;
  std::vector<BundleManifest> manifests;
  for (const auto &b : bundles) {
    if (b.manifest.generation != generation || !BundleBuilder::Verify(b))
      return false;
    manifests.push_back(b.manifest);
  }
  if (!BundleBuilder::ValidateDependencyDag(manifests))
    return false;
  Generation candidate;
  candidate.bundles = std::move(bundles);
  for (const auto &bundle : candidate.bundles)
    for (const auto &entry : bundle.manifest.assets) {
      auto bytes = std::span(bundle.data).subspan(entry.offset, entry.size);
      auto blob = AssetCooker::Deserialize(bytes);
      if (!blob || !candidate.assets.emplace(blob->id, std::move(*blob)).second)
        return false;
    }
  generations_.insert_or_assign(generation, std::move(candidate));
  staged_ = generation;
  return true;
}
bool AssetGenerationStore::ActivateStaged() {
  if (!staged_ || !generations_.contains(staged_))
    return false;
  previous_ = active_;
  active_ = staged_;
  staged_ = 0;
  Collect();
  return true;
}
bool AssetGenerationStore::Rollback() {
  if (!previous_ || !generations_.contains(previous_))
    return false;
  std::swap(active_, previous_);
  Collect();
  return true;
}
bool AssetGenerationStore::Pin(std::uint64_t generation) {
  if (!generations_.contains(generation))
    return false;
  ++pins_[generation];
  return true;
}
bool AssetGenerationStore::Unpin(std::uint64_t generation) {
  auto it = pins_.find(generation);
  if (it == pins_.end())
    return false;
  if (--it->second == 0)
    pins_.erase(it);
  Collect();
  return true;
}
const RuntimeBlob *AssetGenerationStore::Load(AssetUuid id, std::uint64_t generation) {
  if (generation == 0)
    generation = active_;
  auto git = generations_.find(generation);
  if (git == generations_.end())
    return nullptr;
  auto found = git->second.assets.find(id);
  if (found == git->second.assets.end())
    return nullptr;
  auto &resident = residents_[{id, generation}];
  if (resident.references++ == 0) {
    resident.bytes = found->second.payload.size();
    resident_bytes_ += resident.bytes;
  }
  return &found->second;
}
void AssetGenerationStore::Release(AssetUuid id, std::uint64_t generation) {
  if (generation == 0)
    generation = active_;
  auto it = residents_.find({id, generation});
  if (it == residents_.end())
    return;
  if (--it->second.references == 0) {
    resident_bytes_ -= it->second.bytes;
    residents_.erase(it);
  }
  Collect();
}
bool AssetGenerationStore::IsResident(AssetUuid id, std::uint64_t generation) const {
  if (generation == 0)
    generation = active_;
  return residents_.contains({id, generation});
}
bool AssetGenerationStore::IsPinned(std::uint64_t generation) const {
  return pins_.contains(generation);
}
void AssetGenerationStore::Collect() {
  for (auto it = generations_.begin(); it != generations_.end();) {
    const auto g = it->first;
    bool resident =
        std::ranges::any_of(residents_, [g](const auto &e) { return e.first.generation == g; });
    if (g != active_ && g != previous_ && g != staged_ && !pins_.contains(g) && !resident)
      it = generations_.erase(it);
    else
      ++it;
  }
}
bool DataTable::Build(std::vector<DataTableRow> rows) {
  std::unordered_map<std::string, std::size_t> index;
  for (std::size_t i = 0; i < rows.size(); ++i)
    if (rows[i].key.empty() || !index.emplace(rows[i].key, i).second)
      return false;
  rows_ = std::move(rows);
  index_ = std::move(index);
  return true;
}
const DataTableRow *DataTable::Find(std::string_view key) const {
  auto it = index_.find(std::string(key));
  return it == index_.end() ? nullptr : &rows_[it->second];
}
bool AssetPipelineEnabled() noexcept {
#if NEXORA_ASSET_PIPELINE_ENABLED
  return true;
#else
  return false;
#endif
}
} // namespace nexora::runtime
