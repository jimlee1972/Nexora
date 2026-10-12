#include "Nexora/Editor/PrefabComparison.h"
#include <algorithm>
#include <charconv>
#include <map>
#include <set>

namespace nexora::editor {
namespace {
using Fields = std::map<std::string, std::string, std::less<>>;
bool Fail(std::string *error, const char *message) {
  if (error)
    *error = message;
  return false;
}
std::string Property(std::string_view suffix) {
  if (suffix == "name")
    return "name";
  if (suffix == "parent" || suffix == "sibling/index")
    return "parent";
  for (const auto component : {"camera/", "light/", "mesh/"})
    if (suffix.starts_with(component))
      return std::string(component).substr(0, std::string_view(component).size() - 1);
  for (const auto lane : {"position", "rotation", "scale"})
    if (suffix.starts_with(std::string("transform/") + lane + '/'))
      return std::string("transform.") + lane;
  if (suffix.starts_with("authoring/euler/"))
    return "transform.rotation";
  if (suffix.starts_with("opaque/")) {
    const auto end = suffix.find('/', 7);
    if (end != std::string_view::npos)
      return std::string(suffix.substr(0, end));
  }
  return {};
}
std::string Reference(PrefabRevisionReference ref) {
  return ref.asset.ToString() + '@' + std::to_string(ref.revision);
}
std::optional<Fields> Read(const PrefabAsset *asset, std::string *error) {
  if (!asset)
    return Fields{};
  if (!PrefabAssets::Validate(*asset)) {
    Fail(error, "Prefab comparison source has invalid scene or identity metadata.");
    return {};
  }
  const auto raw = CompareSceneRevisions({}, asset->scene_bytes, {}, error);
  if (!raw)
    return {};
  Fields fields;
  std::size_t bytes{};
  const auto put = [&](std::string path, std::string value) {
    if (value.size() > SceneComparison::kMaximumValueBytes ||
        fields.size() >= SceneComparison::kMaximumRows ||
        path.size() > SceneComparison::kMaximumSnapshotBytes - bytes ||
        value.size() > SceneComparison::kMaximumSnapshotBytes - bytes - path.size())
      return false;
    bytes += path.size() + value.size();
    return fields.emplace(std::move(path), std::move(value)).second;
  };
  std::map<runtime::Id, const PrefabNodeIdentity *> nodes;
  for (const auto &node : asset->nodes)
    nodes.emplace(node.serialized_node, &node);
  for (const auto &row : raw->rows) {
    if (!row.local)
      return {};
    std::string path = row.stable_path, value = *row.local;
    if (path.starts_with("entities/")) {
      const auto end = path.find('/', 9);
      if (end == std::string::npos)
        return {};
      runtime::Id id{};
      const auto parsed = std::from_chars(path.data() + 9, path.data() + end, id);
      const auto node = nodes.find(id);
      if (parsed.ec != std::errc{} || parsed.ptr != path.data() + end || node == nodes.end())
        return {};
      const auto suffix = std::string_view(path).substr(end + 1);
      if (suffix == "authoring/tracked")
        continue;
      std::string stable = "nodes/" + node->second->id.ToString() + '/';
      if (suffix != "present") {
        const auto property = Property(suffix);
        const auto field =
            std::ranges::find(node->second->properties, property, &PrefabPropertyIdentity::field);
        if (property.empty() || field == node->second->properties.end())
          return {};
        stable += "fields/" + field->id.ToString() + '/';
      }
      stable += suffix;
      if (suffix == "parent") {
        runtime::Id parent{};
        const auto p = std::from_chars(value.data(), value.data() + value.size(), parent);
        if (p.ec != std::errc{} || p.ptr != value.data() + value.size())
          return {};
        if (parent) {
          const auto found = nodes.find(parent);
          if (found == nodes.end())
            return {};
          value = found->second->id.ToString();
        } else
          value = "root";
      }
      path = std::move(stable);
    }
    if (!put(std::move(path), std::move(value))) {
      Fail(error, "Prefab comparison exceeds the stable snapshot budget.");
      return {};
    }
  }
  if (asset->base && !put("prefab/base", Reference(*asset->base)))
    return {};
  for (std::size_t i = 0; i < asset->nested.size(); ++i) {
    const auto &nested = asset->nested[i];
    const auto prefix = "nested/" + nested.instance.ToString() + '/';
    if (!put(prefix + "present", "true") || !put(prefix + "source", Reference(nested.source)) ||
        !put(prefix + "attachment", nested.attachment.ToString()) ||
        !put(prefix + "index", std::to_string(i)))
      return {};
  }
  return fields;
}
std::optional<std::string> Value(const Fields &fields, const std::string &path) {
  const auto found = fields.find(path);
  return found == fields.end() ? std::nullopt : std::optional(found->second);
}
} // namespace
std::optional<SceneComparison> ComparePrefabRevisions(const PrefabAsset *base_asset,
                                                      const PrefabAsset *local_asset,
                                                      const PrefabAsset *remote_asset,
                                                      std::string *error) {
  const auto base = Read(base_asset, error), local = Read(local_asset, error),
             remote = Read(remote_asset, error);
  if (!base || !local || !remote) {
    if (error && error->empty())
      *error = "Prefab comparison could not construct a stable snapshot.";
    return {};
  }
  std::set<std::string, std::less<>> paths;
  for (const auto *fields : {&*base, &*local, &*remote})
    for (const auto &[path, value] : *fields) {
      static_cast<void>(value);
      paths.insert(path);
    }
  SceneComparison result;
  std::size_t bytes{};
  for (const auto &path : paths) {
    SceneComparisonRow row{path, Value(*base, path), Value(*local, path), Value(*remote, path)};
    if (row.local == row.base && row.remote == row.base)
      continue;
    row.choice = row.local == row.remote  ? SceneComparisonChoice::Shared
                 : row.local == row.base  ? SceneComparisonChoice::Remote
                 : row.remote == row.base ? SceneComparisonChoice::Local
                                          : SceneComparisonChoice::Unresolved;
    const auto count = path.size() + (row.base ? row.base->size() : 0) +
                       (row.local ? row.local->size() : 0) + (row.remote ? row.remote->size() : 0);
    if (result.rows.size() >= SceneComparison::kMaximumRows ||
        count > SceneComparison::kMaximumResultBytes - bytes) {
      Fail(error, "Prefab comparison exceeds the bounded result budget.");
      return {};
    }
    bytes += count;
    if (row.choice == SceneComparisonChoice::Unresolved)
      ++result.conflicts;
    result.rows.push_back(std::move(row));
  }
  if (error)
    error->clear();
  return result;
}
} // namespace nexora::editor
