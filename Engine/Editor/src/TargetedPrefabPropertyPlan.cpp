#include "Nexora/Editor/PrefabPropertyPlan.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <locale>
#include <map>
#include <set>
#include <sstream>

namespace nexora::editor {
namespace {
using Identity = std::pair<std::uint64_t, std::uint64_t>;
Identity Key(foundation::Uuid id) { return {id.high, id.low}; }
struct Metadata final {
  struct Node final {
    runtime::Id parent{};
    std::string name;
  };
  std::string header;
  std::vector<runtime::Id> order;
  std::map<runtime::Id, Node> nodes;
  std::map<runtime::Id, std::string> eulers;
  std::map<std::pair<runtime::Id, std::uint64_t>, std::string> opaque;
};
bool Number(std::string_view line, std::size_t &cursor, std::uint64_t &value) {
  const auto end = line.find(' ', cursor);
  if (end == std::string_view::npos)
    return false;
  const auto result = std::from_chars(line.data() + cursor, line.data() + end, value);
  if (result.ec != std::errc{} || result.ptr != line.data() + end)
    return false;
  cursor = end + 1;
  return true;
}
std::optional<Metadata> ReadMetadata(const std::string &bytes) {
  Metadata result;
  std::istringstream input(bytes);
  if (!std::getline(input, result.header))
    return {};
  std::string line;
  while (std::getline(input, line) && line != "world") {
    const auto end = line.find(' ');
    if (end == std::string::npos)
      return {};
    const auto kind = std::string_view(line).substr(0, end);
    std::size_t cursor = end + 1;
    runtime::Id id{};
    if (!Number(line, cursor, id))
      return {};
    if (kind == "node") {
      runtime::Id parent{};
      if (!Number(line, cursor, parent) ||
          !result.nodes.emplace(id, Metadata::Node{parent, line.substr(cursor)}).second)
        return {};
      result.order.push_back(id);
    } else if (kind == "euler") {
      if (!result.eulers.emplace(id, line).second)
        return {};
    } else if (kind == "opaque") {
      std::uint64_t type{};
      if (!Number(line, cursor, type) || !result.opaque.emplace(std::pair{id, type}, line).second)
        return {};
    } else
      return {};
  }
  return line == "world" ? std::optional(std::move(result)) : std::nullopt;
}
using Record = std::array<std::string, 21>;
struct RuntimeRecords final {
  std::string header;
  std::vector<runtime::Id> order;
  std::map<runtime::Id, Record> rows;
};
std::optional<RuntimeRecords> ReadRuntime(const std::string &bytes) {
  RuntimeRecords result;
  std::istringstream input(bytes);
  input.imbue(std::locale::classic());
  if (!std::getline(input, result.header))
    return {};
  std::string line;
  while (std::getline(input, line)) {
    std::istringstream fields(line);
    fields.imbue(std::locale::classic());
    Record record;
    for (auto &value : record)
      if (!(fields >> value))
        return {};
    fields >> std::ws;
    runtime::Id id{};
    const auto parsed = std::from_chars(record[0].data(), record[0].data() + record[0].size(), id);
    if (!fields.eof() || parsed.ec != std::errc{} ||
        parsed.ptr != record[0].data() + record[0].size() ||
        !result.rows.emplace(id, std::move(record)).second)
      return {};
    result.order.push_back(id);
  }
  return result;
}
} // namespace

std::optional<std::string>
BuildPrefabPropertySnapshot(const PrefabAsset &current, const PrefabAsset &source,
                            std::span<const PrefabPropertySelection> selected) {
  if (selected.size() > PrefabAssets::kMaximumProperties)
    return {};
  const auto complete = BuildPrefabPropertySnapshot(current, source);
  if (!complete)
    return {};
  std::map<Identity, const PrefabNodeIdentity *> current_nodes, source_nodes;
  for (const auto &node : current.nodes)
    current_nodes.emplace(Key(node.id), &node);
  for (const auto &node : source.nodes)
    source_nodes.emplace(Key(node.id), &node);
  std::map<runtime::Id, std::set<std::string>> fields;
  std::set<std::pair<Identity, Identity>> unique;
  for (const auto &selection : selected) {
    const auto target = current_nodes.find(Key(selection.node));
    const auto origin = source_nodes.find(Key(selection.node));
    if (selection.node.IsNil() || selection.field.IsNil() || target == current_nodes.end() ||
        origin == source_nodes.end() ||
        !unique.emplace(Key(selection.node), Key(selection.field)).second)
      return {};
    const auto find = [&](const PrefabNodeIdentity &node) {
      return std::ranges::find(node.properties, selection.field, &PrefabPropertyIdentity::id);
    };
    const auto local = find(*target->second), remote = find(*origin->second);
    if (local == target->second->properties.end() && remote == origin->second->properties.end())
      return {};
    const auto &field = local != target->second->properties.end() ? local->field : remote->field;
    for (const auto *node : {target->second, origin->second}) {
      const auto other = std::ranges::find(node->properties, field, &PrefabPropertyIdentity::field);
      if (other != node->properties.end() && other->id != selection.field)
        return {};
    }
    if (local != target->second->properties.end() && remote != origin->second->properties.end() &&
        local->field != remote->field)
      return {};
    if (field != "name" && field != "parent" && field != "transform.position" &&
        field != "transform.rotation" && field != "transform.scale" && field != "camera" &&
        field != "light" && field != "mesh" && !field.starts_with("opaque/"))
      return {};
    fields[target->second->serialized_node].insert(field);
  }
  runtime::World current_world, source_world, result_world;
  const auto current_scene = current_world.LoadScene("Targeted current");
  const auto source_scene = source_world.LoadScene("Targeted source");
  const auto result_scene = result_world.LoadScene("Targeted candidate");
  SceneDocument current_document(current_world, current_scene),
      source_document(source_world, source_scene), result_document(result_world, result_scene);
  if (!current_document.ReloadBytes(current.scene_bytes, PrefabAssets::kMaximumNodes) ||
      !source_document.ReloadBytes(*complete, PrefabAssets::kMaximumNodes))
    return {};
  const auto prepared = current_document.PrepareSave();
  const auto source_prepared = source_document.PrepareSave();
  if (!prepared || !source_prepared)
    return {};
  if (selected.empty())
    return prepared->Bytes().size() <= PrefabAssets::kMaximumSceneBytes
               ? std::optional(prepared->Bytes())
               : std::nullopt;
  const auto local_metadata = ReadMetadata(prepared->Bytes());
  const auto remote_metadata = ReadMetadata(source_prepared->Bytes());
  if (!local_metadata || !remote_metadata)
    return {};
  runtime::WorldCommandBuffer hierarchy;
  for (const auto &[id, names] : fields)
    if (names.contains("parent"))
      hierarchy.SetParent(id, 0, false);
  for (const auto &[id, names] : fields)
    if (names.contains("parent")) {
      const auto *entity = source_world.FindEntity(id);
      if (!entity)
        return {};
      hierarchy.SetParent(id, entity->parent, false);
    }
  // Apply hierarchy together before ordering. Incompatible selected subsets reject without a
  // result, rather than forcing unrelated parent edits or changing local TRS.
  if (hierarchy.Size() && !hierarchy.Apply(current_world))
    return {};
  runtime::WorldCommandBuffer ordering;
  const auto *source_value = source_world.FindScene(source_scene);
  if (!source_value)
    return {};
  for (const auto &entity : source_value->entities)
    if (const auto it = fields.find(entity.id);
        it != fields.end() && it->second.contains("parent")) {
      const auto index = source_world.SiblingIndex(entity.id);
      if (!index)
        return {};
      ordering.SetSiblingIndex(entity.id, *index);
    }
  if (ordering.Size() && !ordering.Apply(current_world))
    return {};
  const auto local_runtime =
      current_world.SaveScene(current_scene, PrefabAssets::kMaximumSceneBytes);
  const auto remote_runtime =
      source_world.SaveScene(source_scene, PrefabAssets::kMaximumSceneBytes);
  if (!local_runtime || !remote_runtime)
    return {};
  auto local_records = ReadRuntime(*local_runtime);
  const auto remote_records = ReadRuntime(*remote_runtime);
  if (!local_records || !remote_records)
    return {};
  std::string bytes;
  const auto append = [&](std::string_view value) {
    if (value.size() > PrefabAssets::kMaximumSceneBytes - bytes.size())
      return false;
    bytes += value;
    return true;
  };
  if (!append(local_metadata->header) || !append("\n"))
    return {};
  for (const auto id : local_metadata->order) {
    const auto &node = local_metadata->nodes.at(id);
    const auto &names = fields[id];
    const auto *entity = current_world.FindEntity(id);
    const auto remote_node = remote_metadata->nodes.find(id);
    if (!entity || remote_node == remote_metadata->nodes.end())
      return {};
    const auto &name = names.contains("name") ? remote_node->second.name : node.name;
    if (!append("node " + std::to_string(id) + " " + std::to_string(entity->parent) + " " + name +
                "\n"))
      return {};
    const auto &eulers =
        names.contains("transform.rotation") ? remote_metadata->eulers : local_metadata->eulers;
    if (const auto it = eulers.find(id); it != eulers.end())
      if (!append(it->second) || !append("\n"))
        return {};
  }
  auto opaque = local_metadata->opaque;
  for (const auto &[id, names] : fields)
    for (const auto &name : names)
      if (name.starts_with("opaque/")) {
        std::uint64_t type{};
        const auto parsed = std::from_chars(name.data() + 7, name.data() + name.size(), type);
        if (parsed.ec != std::errc{} || parsed.ptr != name.data() + name.size())
          return {};
        const auto key = std::pair{id, type};
        if (const auto it = remote_metadata->opaque.find(key); it != remote_metadata->opaque.end())
          opaque[key] = it->second;
        else
          opaque.erase(key);
      }
  for (const auto &[key, line] : opaque) {
    static_cast<void>(key);
    if (!append(line) || !append("\n"))
      return {};
  }
  if (!append("world\n") || !append(local_records->header) || !append("\n"))
    return {};
  for (const auto id : local_records->order) {
    auto &record = local_records->rows.at(id);
    const auto remote = remote_records->rows.find(id);
    if (remote == remote_records->rows.end())
      return {};
    const auto &names = fields[id];
    const auto copy = [&](std::size_t first, std::size_t end) {
      for (auto index = first; index < end; ++index)
        record[index] = remote->second[index];
    };
    if (names.contains("transform.position"))
      copy(2, 5);
    if (names.contains("transform.rotation"))
      copy(5, 9);
    if (names.contains("transform.scale"))
      copy(9, 12);
    if (names.contains("camera")) {
      copy(12, 13);
      copy(15, 18);
    }
    if (names.contains("light")) {
      copy(13, 14);
      copy(18, 19);
    }
    if (names.contains("mesh")) {
      copy(14, 15);
      copy(19, 21);
    }
    for (std::size_t i = 0; i < record.size(); ++i)
      if ((i && !append(" ")) || !append(record[i]))
        return {};
    if (!append("\n"))
      return {};
  }
  if (!result_document.ReloadBytes(bytes, PrefabAssets::kMaximumNodes))
    return {};
  const auto result = result_document.PrepareSave();
  if (!result || result->Bytes().size() > PrefabAssets::kMaximumSceneBytes)
    return {};
  return result->Bytes();
}
} // namespace nexora::editor
