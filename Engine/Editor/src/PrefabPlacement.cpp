#include "PrefabPlacementInternal.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <unordered_set>

namespace nexora::editor {
namespace {
auto Identity(foundation::Uuid id) { return std::array{id.high, id.low}; }
} // namespace
bool SceneDocument::ValidatePrefabPlacements(std::span<const PrefabPlacement> placements,
                                             std::span<const runtime::Id> live) {
  if (placements.size() > kMaximumPrefabPlacements)
    return false;
  std::set<std::array<std::uint64_t, 2>> instances;
  std::unordered_set<runtime::Id> ids(live.begin(), live.end()), mapped;
  std::size_t count{};
  for (const auto &placement : placements) {
    if (placement.instance.IsNil() || placement.source.IsNil() || !placement.revision ||
        placement.nodes.empty() || !instances.insert(Identity(placement.instance)).second ||
        placement.nodes.size() > kMaximumPrefabPlacementNodes - count)
      return false;
    count += placement.nodes.size();
    std::set<std::vector<std::uint64_t>> paths;
    for (const auto &node : placement.nodes) {
      if (node.source_node.IsNil() || !node.target || !ids.contains(node.target) ||
          !mapped.insert(node.target).second || node.scope.size() > kMaximumPrefabPlacementDepth)
        return false;
      std::vector<std::uint64_t> path;
      path.reserve(node.scope.size() * 2 + 2);
      for (const auto id : node.scope) {
        if (id.IsNil())
          return false;
        path.push_back(id.high);
        path.push_back(id.low);
      }
      path.push_back(node.source_node.high);
      path.push_back(node.source_node.low);
      if (!paths.insert(std::move(path)).second)
        return false;
    }
  }
  return detail::EncodePrefabPlacements(placements).has_value();
}
std::span<const SceneDocument::PrefabPlacement> SceneDocument::PrefabPlacements() const noexcept {
  return prefab_placements_ ? std::span<const PrefabPlacement>(*prefab_placements_)
                            : std::span<const PrefabPlacement>{};
}
std::optional<std::string> SceneDocument::PrefabPlacementRecords() const {
  if (PrefabPlacements().empty())
    return std::string{};
  std::vector<runtime::Id> ids;
  ids.reserve(nodes_.size());
  const auto *scene = world_.FindScene(scene_);
  if (!scene)
    return {};
  std::unordered_set<runtime::Id> live;
  for (const auto &entity : scene->entities)
    live.insert(entity.id);
  for (const auto &node : nodes_)
    if (live.contains(node.id))
      ids.push_back(node.id);
  if (!ValidatePrefabPlacements(PrefabPlacements(), ids))
    return {};
  return detail::EncodePrefabPlacements(PrefabPlacements());
}
std::optional<std::vector<SceneDocument::ImportedForestNode>>
SceneDocument::ImportPrefabForest(const PreparedSave &expected, std::string_view bytes,
                                  PrefabPlacement placement, bool authorized) {
  if (!authorized || !MatchesPreparedSave(expected) ||
      PrefabPlacements().size() >= kMaximumPrefabPlacements)
    return {};
  runtime::World probe_world;
  SceneDocument probe(probe_world, probe_world.LoadScene("Bound prefab import"));
  if (!probe.ReloadBytes(bytes, kMaximumImportedForestNodes) || !probe.PrefabPlacements().empty() ||
      placement.nodes.size() != probe.nodes_.size())
    return {};
  std::vector<runtime::Id> ids;
  for (const auto &node : probe.nodes_)
    ids.push_back(node.id);
  if (!ValidatePrefabPlacements(std::span{&placement, 1}, ids))
    return {};
  auto prepared = std::make_shared<std::vector<PrefabPlacement>>(PrefabPlacements().begin(),
                                                                 PrefabPlacements().end());
  if (std::ranges::any_of(*prepared,
                          [&](const auto &old) { return old.instance == placement.instance; }))
    return {};
  std::size_t count = placement.nodes.size();
  for (const auto &old : *prepared) {
    if (old.nodes.size() > kMaximumPrefabPlacementNodes - count)
      return {};
    count += old.nodes.size();
  }
  prepared->push_back(std::move(placement));
  const auto records = detail::EncodePrefabPlacements(*prepared);
  const auto normalized = probe.PrepareSave();
  // Account conservatively for remapped 20-digit IDs, repeated opaque IDs and record framing.
  if (!records || !normalized ||
      prepared->back().nodes.size() * 20 > kMaximumPrefabPlacementBytes - records->size() ||
      expected.Bytes().size() + normalized->Bytes().size() + records->size() + count * 256 +
              UnknownComponentStore::kMaximumComponents * 32 >
          kMaximumRuntimeCaptureBytes)
    return {};
  std::map<runtime::Id, std::size_t> positions;
  for (std::size_t i = 0; i < prepared->back().nodes.size(); ++i)
    positions.emplace(prepared->back().nodes[i].target, i);
  auto imported = ImportForestBytes(expected, normalized->Bytes(), authorized);
  if (!imported)
    return {};
  // All metadata and indices exist before the live import. Import's complete identity map
  // guarantees every lookup; translating IDs and sharing immutable ownership cannot allocate.
  for (const auto &entry : *imported)
    prepared->back().nodes[positions.at(entry.source)].target = entry.target.id;
  prefab_placements_ = std::move(prepared);
  return imported;
}
namespace detail {
std::optional<std::string>
EncodePrefabPlacements(std::span<const SceneDocument::PrefabPlacement> placements) {
  std::string result;
  const auto append = [&](std::string line) {
    if (line.size() > SceneDocument::kMaximumPrefabPlacementBytes - result.size())
      return false;
    result += line;
    return true;
  };
  for (const auto &placement : placements) {
    if (!append("prefab-placement " + placement.instance.ToString() + ' ' +
                placement.source.ToString() + ' ' + std::to_string(placement.revision) + '\n'))
      return {};
    for (const auto &node : placement.nodes) {
      std::string line =
          "prefab-node " + placement.instance.ToString() + ' ' + std::to_string(node.scope.size());
      for (const auto id : node.scope)
        line += ' ' + id.ToString();
      line += ' ' + node.source_node.ToString() + ' ' + std::to_string(node.target) + '\n';
      if (!append(std::move(line)))
        return {};
    }
  }
  return result;
}
bool ReadPrefabPlacementLine(std::string_view line,
                             std::vector<SceneDocument::PrefabPlacement> &placements) {
  std::istringstream input{std::string(line)};
  input.imbue(std::locale::classic());
  std::string kind, instance_text, source_text;
  const auto number = [&](auto &value) {
    std::string token;
    if (!(input >> token) || token.empty() ||
        std::ranges::any_of(token, [](char c) { return c < '0' || c > '9'; }))
      return false;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size();
  };
  if (!(input >> kind >> instance_text))
    return false;
  const auto instance = foundation::Uuid::Parse(instance_text);
  if (!instance || instance.Value().IsNil())
    return false;
  if (kind == "prefab-placement") {
    std::uint64_t revision{};
    if (placements.size() >= SceneDocument::kMaximumPrefabPlacements || !(input >> source_text) ||
        !number(revision) || !revision || std::ranges::any_of(placements, [&](const auto &old) {
          return old.instance == instance.Value();
        }))
      return false;
    const auto source = foundation::Uuid::Parse(source_text);
    if (!source || source.Value().IsNil())
      return false;
    placements.push_back({instance.Value(), source.Value(), revision, {}});
  } else if (kind == "prefab-node") {
    std::size_t count{};
    std::size_t total{};
    for (const auto &placement : placements)
      total += placement.nodes.size();
    if (placements.empty() || placements.back().instance != instance.Value() || !number(count) ||
        count > SceneDocument::kMaximumPrefabPlacementDepth ||
        total >= SceneDocument::kMaximumPrefabPlacementNodes ||
        placements.back().nodes.size() >= SceneDocument::kMaximumPrefabPlacementNodes)
      return false;
    SceneDocument::PrefabPlacementNode node;
    node.scope.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      std::string scope_text;
      if (!(input >> scope_text))
        return false;
      const auto scope = foundation::Uuid::Parse(scope_text);
      if (!scope || scope.Value().IsNil())
        return false;
      node.scope.push_back(scope.Value());
    }
    if (!(input >> source_text) || !number(node.target) || !node.target)
      return false;
    const auto source = foundation::Uuid::Parse(source_text);
    if (!source || source.Value().IsNil())
      return false;
    node.source_node = source.Value();
    placements.back().nodes.push_back(std::move(node));
  } else
    return false;
  input >> std::ws;
  return input.eof();
}
} // namespace detail
} // namespace nexora::editor
