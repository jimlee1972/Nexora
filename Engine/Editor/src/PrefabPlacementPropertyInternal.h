#pragma once
#include "Nexora/Editor/SceneComparison.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <map>
#include <set>

namespace nexora::editor::detail {
using PlacementFields = std::map<std::string, std::string, std::less<>>;
using PlacementNodes = std::map<runtime::Id, PlacementFields>;
inline std::optional<PlacementNodes> PlacementSnapshot(std::string_view bytes, std::string *error) {
  const auto comparison = CompareSceneRevisions({}, bytes, {}, error);
  if (!comparison)
    return {};
  PlacementNodes result;
  for (const auto &row : comparison->rows) {
    constexpr std::string_view prefix = "entities/";
    if (!row.stable_path.starts_with(prefix) || !row.local)
      continue;
    const auto path = std::string_view(row.stable_path).substr(prefix.size());
    const auto slash = path.find('/');
    if (slash == path.npos)
      return {};
    runtime::Id id{};
    const auto parsed = std::from_chars(path.data(), path.data() + slash, id);
    if (parsed.ec != std::errc{} || parsed.ptr != path.data() + slash || !id)
      return {};
    const auto field = path.substr(slash + 1);
    // Scene-global identity and absolute sibling positions are not instance properties.
    if (field == "present" || field == "authoring/tracked" || field == "sibling/index")
      continue;
    result[id].emplace(field, *row.local);
  }
  return result;
}
inline std::string PlacementPropertyGroup(std::string_view field) {
  if (field.starts_with("authoring/euler/"))
    return "transform.rotation";
  for (const auto prefix : {"transform/position/", "transform/rotation/", "transform/scale/"})
    if (field.starts_with(prefix)) {
      std::string result(prefix);
      result.pop_back();
      std::ranges::replace(result, '/', '.');
      return result;
    }
  if (field.starts_with("opaque/")) {
    const auto slash = field.find('/', 7);
    return std::string(field.substr(0, slash));
  }
  return std::string(field.substr(0, field.find('/')));
}
inline std::optional<std::string> PlacementValue(const PlacementFields &fields,
                                                 const std::string &name) {
  const auto found = fields.find(name);
  return found == fields.end() ? std::nullopt : std::optional(found->second);
}
inline std::optional<std::map<runtime::Id, std::string>>
PlacementRuntimeProperties(std::string_view snapshot) {
  // Consume only bounded canonical version-3 output produced by World::SaveScene here.
  // ID/parent remain target-owned; the suffix owns every stored property and presence flag.
  if (!snapshot.starts_with("NEXORA_SCENE 3 "))
    return {};
  auto cursor = snapshot.find('\n');
  if (cursor == snapshot.npos)
    return {};
  ++cursor;
  std::map<runtime::Id, std::string> records;
  while (cursor < snapshot.size()) {
    const auto end = snapshot.find('\n', cursor);
    if (end == snapshot.npos)
      return {};
    const auto line = snapshot.substr(cursor, end - cursor);
    const auto first = line.find(' ');
    const auto second = first == line.npos ? line.npos : line.find(' ', first + 1);
    if (second == line.npos || second + 1 == line.size())
      return {};
    runtime::Id id{};
    const auto parsed = std::from_chars(line.data(), line.data() + first, id);
    if (parsed.ec != std::errc{} || parsed.ptr != line.data() + first || !id ||
        !records.emplace(id, line.substr(second + 1)).second)
      return {};
    cursor = end + 1;
  }
  return records;
}
inline std::optional<std::string>
SelectPlacementRuntimeProperties(std::string_view retained, std::string_view local,
                                 const std::set<std::string> &groups) {
  const auto split = [](std::string_view value) -> std::optional<std::array<std::string_view, 19>> {
    std::array<std::string_view, 19> tokens;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
      const auto end = value.find(' ');
      if (value.empty() || (i + 1 == tokens.size()) != (end == value.npos))
        return {};
      tokens[i] = value.substr(0, end);
      if (tokens[i].empty())
        return {};
      if (end != value.npos)
        value.remove_prefix(end + 1);
    }
    return tokens;
  };
  const auto source = split(retained);
  auto destination = split(local);
  if (!source || !destination)
    return {};
  const auto copy = [&](std::initializer_list<std::size_t> indices) {
    for (const auto index : indices)
      (*destination)[index] = (*source)[index];
  };
  if (groups.contains("transform.position"))
    copy({0, 1, 2});
  if (groups.contains("transform.rotation"))
    copy({3, 4, 5, 6});
  if (groups.contains("transform.scale"))
    copy({7, 8, 9});
  if (groups.contains("camera"))
    copy({10, 13, 14, 15});
  if (groups.contains("light"))
    copy({11, 16});
  if (groups.contains("mesh"))
    copy({12, 17, 18});
  std::string result;
  for (const auto token : *destination) {
    if (!result.empty())
      result += ' ';
    result += token;
  }
  return result;
}
} // namespace nexora::editor::detail
