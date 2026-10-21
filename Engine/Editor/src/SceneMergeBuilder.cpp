#include "Nexora/Editor/EditorWorkspace.h"
#include "SceneMergeTestAccess.h"
#include "SceneSemanticSnapshot.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <locale>
#include <new>
#include <set>
#include <sstream>
#include <utility>

namespace nexora::editor::detail {
namespace {
using Id = runtime::Id;
thread_local bool fail_after_load{};
struct Builder final {
  const SceneSemanticFields &fields;
  bool valid{true};
  const std::string &Get(const std::string &path) {
    static const std::string missing;
    const auto found = fields.find(path);
    if (found == fields.end()) {
      valid = false;
      return missing;
    }
    return found->second;
  }
  std::uint64_t Unsigned(std::string_view text) {
    std::uint64_t value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
      valid = false;
    return value;
  }
  std::string Boolean(const std::string &path) {
    const auto &value = Get(path);
    if (value == "true")
      return "1";
    if (value != "false")
      valid = false;
    return "0";
  }
  bool Append(std::string &output, std::string_view text) {
    if (!valid || text.size() > SceneComparison::kMaximumSourceBytes - output.size()) {
      valid = false;
      return false;
    }
    output.append(text);
    return true;
  }
  static std::string Quote(const std::string &text) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::quoted(text);
    return output.str();
  }
  std::set<Id> NumericChildren(const std::string &prefix) {
    std::set<Id> ids;
    for (auto i = fields.lower_bound(prefix); i != fields.end() && i->first.starts_with(prefix);
         ++i) {
      const auto suffix = std::string_view(i->first).substr(prefix.size());
      const auto slash = suffix.find('/');
      if (slash == suffix.npos) {
        valid = false;
        break;
      }
      const auto id = Unsigned(suffix.substr(0, slash));
      if (!id)
        valid = false;
      ids.insert(id);
    }
    return ids;
  }
  bool UnitRotation(const std::string &prefix) {
    double q[4]{};
    std::size_t i{};
    for (const auto axis : {"x", "y", "z", "w"}) {
      const auto &value = Get(prefix + "transform/rotation/" + axis);
      const auto result = std::from_chars(value.data(), value.data() + value.size(), q[i]);
      if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
          !std::isfinite(q[i]))
        return false;
      ++i;
    }
    return std::abs(std::hypot(std::hypot(q[0], q[1], q[2]), q[3]) - 1) <= 1e-8;
  }
};
} // namespace
void SceneMergeTestAccess::FailNextAfterLoad() noexcept { fail_after_load = true; }
std::optional<std::string> BuildMergedScene(const SceneSemanticFields &fields, std::string *error) {
  const auto fail = [&]() -> std::optional<std::string> {
    if (error)
      *error = "Chosen fields do not form a complete coherent supported scene candidate.";
    return {};
  };
  Builder b{fields};
  if (b.Get("scene/present") != "true")
    return fail();
  const auto entities = b.NumericChildren("entities/");
  if (!b.valid || entities.size() > SceneComparison::kMaximumEntities)
    return fail();
  std::map<Id, std::vector<std::pair<std::uint64_t, Id>>> children;
  for (const auto id : entities) {
    const auto prefix = "entities/" + std::to_string(id) + '/';
    if (b.Get(prefix + "present") != "true" || !b.UnitRotation(prefix))
      return fail();
    const auto parent = b.Unsigned(b.Get(prefix + "parent"));
    const auto sibling = b.Unsigned(b.Get(prefix + "sibling/index"));
    if (!b.valid || (parent && !entities.contains(parent)))
      return fail();
    children[parent].emplace_back(sibling, id);
  }
  for (auto &[parent, siblings] : children) {
    static_cast<void>(parent);
    std::ranges::sort(siblings);
    for (std::size_t i = 0; i < siblings.size(); ++i)
      if (siblings[i].first != i)
        return fail();
  }
  std::vector<Id> stack, order;
  if (const auto roots = children.find(0); roots != children.end())
    for (auto i = roots->second.rbegin(); i != roots->second.rend(); ++i)
      stack.push_back(i->second);
  std::set<Id> visited;
  while (!stack.empty()) {
    const auto id = stack.back();
    stack.pop_back();
    if (!visited.insert(id).second)
      return fail();
    order.push_back(id);
    if (const auto found = children.find(id); found != children.end())
      for (auto i = found->second.rbegin(); i != found->second.rend(); ++i)
        stack.push_back(i->second);
  }
  if (order.size() != entities.size())
    return fail();
  std::string metadata, snapshot;
  bool opaque{}, placements{};
  b.Append(snapshot, "NEXORA_SCENE 3 " + Builder::Quote(b.Get("scene/name")) + ' ' +
                         b.Boolean("scene/persistent") + ' ' + std::to_string(entities.size()) +
                         '\n');
  for (const auto id : order) {
    if (!b.valid)
      return fail();
    const auto text = std::to_string(id);
    const auto prefix = "entities/" + text + '/';
    const auto parent = b.Get(prefix + "parent");
    if (b.Boolean(prefix + "authoring/tracked") == "1") {
      b.Append(metadata, "node " + text + ' ' + parent + ' ' + b.Get(prefix + "name") + '\n');
      bool has_euler{};
      for (const auto axis : {"x", "y", "z"})
        has_euler = has_euler || fields.contains(prefix + "authoring/euler/" + axis);
      if (has_euler)
        b.Append(metadata, "euler " + text + ' ' + b.Get(prefix + "authoring/euler/x") + ' ' +
                               b.Get(prefix + "authoring/euler/y") + ' ' +
                               b.Get(prefix + "authoring/euler/z") + '\n');
      for (const auto type : b.NumericChildren(prefix + "opaque/")) {
        const auto path = prefix + "opaque/" + std::to_string(type) + '/';
        const auto &payload = b.Get(path + "payload_hex");
        b.Append(metadata, "opaque " + text + ' ' + std::to_string(type) + ' ' +
                               Builder::Quote(b.Get(path + "type_name")) + ' ' +
                               (payload.empty() ? "-" : payload) + '\n');
        opaque = true;
      }
    }
    std::string line = text + ' ' + parent;
    for (const auto field :
         {"transform/position/x", "transform/position/y", "transform/position/z",
          "transform/rotation/x", "transform/rotation/y", "transform/rotation/z",
          "transform/rotation/w", "transform/scale/x", "transform/scale/y", "transform/scale/z"})
      line += ' ' + b.Get(prefix + field);
    for (const auto field : {"camera/enabled", "light/enabled", "mesh/enabled"})
      line += ' ' + b.Boolean(prefix + field);
    for (const auto field : {"camera/field_of_view", "camera/near_plane", "camera/far_plane",
                             "light/intensity", "mesh/asset", "mesh/shader"})
      line += ' ' + b.Get(prefix + field);
    b.Append(snapshot, line + '\n');
  }
  std::set<std::string> instances;
  constexpr std::string_view prefab_prefix = "prefabs/";
  for (auto i = fields.lower_bound(prefab_prefix);
       i != fields.end() && i->first.starts_with(prefab_prefix); ++i) {
    const auto path = std::string_view(i->first).substr(prefab_prefix.size());
    const auto slash = path.find('/');
    if (slash == path.npos)
      return fail();
    instances.emplace(path.substr(0, slash));
  }
  for (const auto &instance : instances) {
    const auto prefix = "prefabs/" + instance + '/';
    if (b.Get(prefix + "present") != "true")
      return fail();
    b.Append(metadata, "prefab-placement " + instance + ' ' + b.Get(prefix + "source") + ' ' +
                           b.Get(prefix + "revision") + '\n');
    for (const auto target : b.NumericChildren(prefix + "nodes/")) {
      const auto path = prefix + "nodes/" + std::to_string(target) + '/';
      if (b.Get(path + "present") != "true")
        return fail();
      const auto &scope = b.Get(path + "scope");
      const auto count = scope.empty() ? 0 : std::ranges::count(scope, '/') + 1;
      auto words = scope;
      std::ranges::replace(words, '/', ' ');
      b.Append(metadata, "prefab-node " + instance + ' ' + std::to_string(count) +
                             (words.empty() ? "" : " " + words) + ' ' +
                             b.Get(path + "source_node") + ' ' + std::to_string(target) + '\n');
    }
    placements = true;
  }
  std::string candidate = placements ? "NEXORA_EDITOR_SCENE 4\n"
                          : opaque   ? "NEXORA_EDITOR_SCENE 3\n"
                                     : "NEXORA_EDITOR_SCENE 2\n";
  if (!b.Append(candidate, metadata) || !b.Append(candidate, "world\n") ||
      !b.Append(candidate, snapshot))
    return fail();
  runtime::World world;
  const auto scene = world.LoadScene("Merge validation");
  SceneDocument document(world, scene);
  if (!document.ReloadBytes(candidate, SceneComparison::kMaximumEntities))
    return fail();
  for (const auto &entity : world.FindScene(scene)->entities) {
    const auto &camera = entity.camera_data;
    if ((entity.camera &&
         (!(camera.vertical_field_of_view > 0 && camera.vertical_field_of_view < 180) ||
          !(camera.near_plane > 0 && camera.far_plane > camera.near_plane))) ||
        (entity.light && !(entity.light_data.intensity >= 0)))
      return fail();
  }
  if (std::exchange(fail_after_load, false))
    throw std::bad_alloc{};
  const auto prepared = document.PrepareSave();
  if (!prepared || prepared->Bytes().size() > SceneComparison::kMaximumSourceBytes)
    return fail();
  return prepared->Bytes();
}
} // namespace nexora::editor::detail
