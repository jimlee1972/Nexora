#include "Nexora/Editor/PrefabPropertyPlan.h"
#include <charconv>
#include <map>
#include <sstream>

namespace nexora::editor {
std::optional<std::string> BuildPrefabPropertySnapshot(const PrefabAsset &current,
                                                       const PrefabAsset &source) {
  if (!PrefabAssets::Validate(current) || !PrefabAssets::Validate(source) ||
      current.nodes.size() != source.nodes.size())
    return {};
  using Identity = std::pair<std::uint64_t, std::uint64_t>;
  std::map<Identity, runtime::Id> targets;
  for (const auto &node : current.nodes)
    targets.emplace(Identity{node.id.high, node.id.low}, node.serialized_node);
  std::map<runtime::Id, runtime::Id> ids{{0, 0}};
  for (const auto &node : source.nodes) {
    const auto found = targets.find({node.id.high, node.id.low});
    if (found == targets.end())
      return {};
    ids.emplace(node.serialized_node, found->second);
  }
  runtime::World current_world, source_world, candidate_world;
  const auto current_scene = current_world.LoadScene("Current property source");
  const auto source_scene = source_world.LoadScene("Revert property source");
  const auto candidate_scene = candidate_world.LoadScene("Candidate");
  SceneDocument current_document(current_world, current_scene),
      source_document(source_world, source_scene), candidate(candidate_world, candidate_scene);
  if (!current_document.ReloadBytes(current.scene_bytes, PrefabAssets::kMaximumNodes) ||
      !source_document.ReloadBytes(source.scene_bytes, PrefabAssets::kMaximumNodes))
    return {};
  const auto *current_value = current_world.FindScene(current_scene);
  const auto *source_value = source_world.FindScene(source_scene);
  if (!current_value || !source_value || current_value->entities.size() != current.nodes.size() ||
      source_value->entities.size() != source.nodes.size())
    return {};
  const auto prepared = source_document.PrepareSave();
  const auto current_runtime =
      current_world.SaveScene(current_scene, PrefabAssets::kMaximumSceneBytes);
  if (!prepared || prepared->Bytes().size() > PrefabAssets::kMaximumSceneBytes || !current_runtime)
    return {};
  std::string result;
  const auto append = [&](std::string_view value) {
    if (value.size() > PrefabAssets::kMaximumSceneBytes - result.size())
      return false;
    result += value;
    return true;
  };
  const auto remap = [&](std::string_view line, std::size_t count) -> std::optional<std::string> {
    std::string value;
    std::size_t begin{};
    for (std::size_t i = 0; i < count; ++i) {
      const auto end = line.find(' ', begin);
      if (end == std::string_view::npos)
        return {};
      runtime::Id id{};
      const auto parsed = std::from_chars(line.data() + begin, line.data() + end, id);
      const auto target = ids.find(id);
      if (parsed.ec != std::errc{} || parsed.ptr != line.data() + end || target == ids.end())
        return {};
      if (i)
        value += ' ';
      value += std::to_string(target->second);
      begin = end + 1;
    }
    value += ' ';
    value += line.substr(begin);
    return value;
  };
  std::istringstream input(prepared->Bytes());
  std::string line;
  if (!std::getline(input, line) || !append(line) || !append("\n"))
    return {};
  while (std::getline(input, line) && line != "world") {
    const auto prefix = line.find(' ');
    if (prefix == std::string::npos)
      return {};
    const auto kind = std::string_view(line).substr(0, prefix);
    if (kind != "node" && kind != "euler" && kind != "opaque")
      return {};
    const auto mapped = remap(std::string_view(line).substr(prefix + 1), kind == "node" ? 2 : 1);
    if (!mapped || !append(kind) || !append(" ") || !append(*mapped) || !append("\n"))
      return {};
  }
  if (line != "world" || !append("world\n") || !std::getline(input, line))
    return {};
  // Preserve the target's scene identity while source entity storage retains authored sibling
  // order.
  const auto header_end = current_runtime->find('\n');
  if (header_end == std::string::npos ||
      !append(std::string_view(*current_runtime).substr(0, header_end + 1)))
    return {};
  while (std::getline(input, line)) {
    const auto mapped = remap(line, 2);
    if (!mapped || !append(*mapped) || !append("\n"))
      return {};
  }
  if (!candidate.ReloadBytes(result, PrefabAssets::kMaximumNodes))
    return {};
  const auto canonical = candidate.PrepareSave();
  if (!canonical || canonical->Bytes().size() > PrefabAssets::kMaximumSceneBytes)
    return {};
  return canonical->Bytes();
}
} // namespace nexora::editor
