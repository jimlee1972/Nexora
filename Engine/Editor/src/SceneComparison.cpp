#include "Nexora/Editor/SceneComparison.h"
#include "Nexora/Editor/EditorWorkspace.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <set>
#include <sstream>

namespace nexora::editor {
namespace {
using Fields = std::map<std::string, std::string, std::less<>>;
bool Fail(std::string *error, const char *message) {
  if (error)
    *error = message;
  return false;
}
bool DisplayText(std::string_view value) {
  return foundation::IsValidUtf8(value) &&
         std::ranges::none_of(value, [](unsigned char c) { return c < 32 || c == 127; });
}
std::string Number(double value) {
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << std::setprecision(17) << (value == 0 ? 0 : value);
  return output.str();
}
class Snapshot final {
public:
  Fields fields;
  bool Put(std::string key, std::string value) {
    if (value.size() > SceneComparison::kMaximumValueBytes ||
        fields.size() >= SceneComparison::kMaximumRows ||
        key.size() > SceneComparison::kMaximumSnapshotBytes - bytes_ ||
        value.size() > SceneComparison::kMaximumSnapshotBytes - bytes_ - key.size())
      return false;
    bytes_ += key.size() + value.size();
    return fields.emplace(std::move(key), std::move(value)).second;
  }

private:
  std::size_t bytes_{};
};
std::optional<Fields> Read(std::optional<std::string_view> source, std::string *error) {
  if (!source)
    return Fields{};
  if (source->empty() || source->size() > SceneComparison::kMaximumSourceBytes) {
    Fail(error, "Scene comparison source is empty or exceeds 8 MiB.");
    return std::nullopt;
  }
  runtime::World world;
  const auto id = world.LoadScene("Comparison");
  SceneDocument document(world, id);
  if (!document.ReloadBytes(*source, SceneComparison::kMaximumEntities)) {
    Fail(error, "Scene comparison source is corrupt or has an unsupported schema.");
    return std::nullopt;
  }
  const auto *scene = world.FindScene(id);
  if (!scene || scene->entities.size() > SceneComparison::kMaximumEntities ||
      !DisplayText(scene->name)) {
    Fail(error, "Scene comparison exceeds entity capacity or has unsafe display text.");
    return std::nullopt;
  }
  // The owning capture also rejects stale/dangling authoring metadata before fields are exposed.
  if (!document.CaptureRuntimeScene(error))
    return std::nullopt;
  Snapshot snapshot;
  bool valid = snapshot.Put("scene/present", "true") && snapshot.Put("scene/name", scene->name) &&
               snapshot.Put("scene/persistent", scene->persistent ? "true" : "false");
  const auto put_number = [&](const std::string &path, double value) {
    return std::isfinite(value) && snapshot.Put(path, Number(value));
  };
  std::map<runtime::Id, std::size_t> siblings;
  for (const auto &entity : scene->entities) {
    const auto prefix = "entities/" + std::to_string(entity.id) + '/';
    valid = valid && snapshot.Put(prefix + "present", "true") &&
            snapshot.Put(prefix + "parent", std::to_string(entity.parent)) &&
            snapshot.Put(prefix + "sibling/index", std::to_string(siblings[entity.parent]++));
    const auto &t = entity.transform;
    // q and -q encode the same rotation. Choose a deterministic hemisphere, including w == 0.
    double sign = 1;
    for (const auto value : {t.qw, t.qx, t.qy, t.qz})
      if (value != 0) {
        sign = value < 0 ? -1 : 1;
        break;
      }
    const std::pair<const char *, double> transform[] = {
        {"position/x", t.x},         {"position/y", t.y},
        {"position/z", t.z},         {"rotation/x", sign * t.qx},
        {"rotation/y", sign * t.qy}, {"rotation/z", sign * t.qz},
        {"rotation/w", sign * t.qw}, {"scale/x", t.sx},
        {"scale/y", t.sy},           {"scale/z", t.sz}};
    for (const auto &[field, value] : transform)
      valid = valid && put_number(prefix + "transform/" + field, value);
    // Presence and stored fields are both retained, including currently disabled component data.
    valid =
        valid && snapshot.Put(prefix + "camera/enabled", entity.camera ? "true" : "false") &&
        put_number(prefix + "camera/field_of_view", entity.camera_data.vertical_field_of_view) &&
        put_number(prefix + "camera/near_plane", entity.camera_data.near_plane) &&
        put_number(prefix + "camera/far_plane", entity.camera_data.far_plane) &&
        snapshot.Put(prefix + "light/enabled", entity.light ? "true" : "false") &&
        put_number(prefix + "light/intensity", entity.light_data.intensity) &&
        snapshot.Put(prefix + "mesh/enabled", entity.mesh_renderer ? "true" : "false") &&
        snapshot.Put(prefix + "mesh/asset", std::to_string(entity.mesh_data.mesh)) &&
        snapshot.Put(prefix + "mesh/shader", std::to_string(entity.mesh_data.material.shader));
    const auto key = document.Key(entity.id);
    valid = valid && snapshot.Put(prefix + "authoring/tracked", key ? "true" : "false");
    if (key) {
      const auto name = document.Name(entity.id);
      const auto euler = document.EulerAngles(entity.id);
      valid = valid && DisplayText(name) && snapshot.Put(prefix + "name", std::string(name));
      if (euler)
        for (std::size_t axis = 0; axis < 3; ++axis)
          valid = valid && put_number(prefix + "authoring/euler/" + "xyz"[axis], (*euler)[axis]);
      const auto components = document.OpaqueComponents(*key);
      if (!components)
        valid = false;
      else
        for (const auto &component : *components) {
          if (component.data.size() > SceneComparison::kMaximumValueBytes / 2) {
            valid = false;
            break;
          }
          const auto path = prefix + "opaque/" + std::to_string(component.type) + '/';
          std::string hex;
          hex.reserve(component.data.size() * 2);
          constexpr char digits[] = "0123456789abcdef";
          for (const auto byte : component.data) {
            hex.push_back(digits[byte >> 4]);
            hex.push_back(digits[byte & 15]);
          }
          valid = valid && DisplayText(component.type_name) &&
                  snapshot.Put(path + "type_name", component.type_name) &&
                  snapshot.Put(path + "payload_hex", std::move(hex));
        }
    }
    if (!valid)
      break;
  }
  if (!valid) {
    Fail(error, "Scene comparison has unsafe fields or exceeds the bounded snapshot/value budget.");
    return std::nullopt;
  }
  return std::move(snapshot.fields);
}
std::optional<std::string> Value(const Fields &fields, const std::string &path) {
  const auto found = fields.find(path);
  return found == fields.end() ? std::nullopt : std::optional(found->second);
}
} // namespace
std::optional<SceneComparison> CompareSceneRevisions(std::optional<std::string_view> base_source,
                                                     std::optional<std::string_view> local_source,
                                                     std::optional<std::string_view> remote_source,
                                                     std::string *error) {
  const auto base = Read(base_source, error);
  if (!base)
    return std::nullopt;
  const auto local = Read(local_source, error);
  if (!local)
    return std::nullopt;
  const auto remote = Read(remote_source, error);
  if (!remote)
    return std::nullopt;
  std::set<std::string, std::less<>> paths;
  for (const auto *fields : {&*base, &*local, &*remote})
    for (const auto &[path, value] : *fields) {
      static_cast<void>(value);
      paths.insert(path);
    }
  SceneComparison result;
  std::size_t bytes = 0;
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
      Fail(error, "Scene comparison exceeds the bounded result budget.");
      return std::nullopt;
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
