#include "Nexora/Editor/PrefabAssets.h"
#include "AtomicFile.h"
#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <set>

namespace nexora::editor {
namespace {
using Uuid = foundation::Uuid;
using Key = std::pair<std::uint64_t, std::uint64_t>;
Key Identity(Uuid id) { return {id.high, id.low}; }
bool Reference(PrefabRevisionReference ref) { return !ref.asset.IsNil() && ref.revision; }
std::vector<std::string> Fields(const SceneDocument &scene, runtime::Id id) {
  std::vector<std::string> fields{"camera",
                                  "layer",
                                  "light",
                                  "mesh",
                                  "name",
                                  "parent",
                                  "transform.position",
                                  "transform.rotation",
                                  "transform.scale"};
  const auto key = scene.Key(id);
  if (!key)
    return {};
  if (const auto opaque = scene.OpaqueComponents(*key))
    for (const auto &component : *opaque)
      fields.push_back("opaque/" + std::to_string(component.type));
  std::ranges::sort(fields);
  return fields;
}
struct Writer final {
  std::vector<std::byte> bytes;
  void Number(std::uint64_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i)
      bytes.push_back(static_cast<std::byte>(value >> (8 * i)));
  }
  void Id(Uuid id) {
    Number(id.high, 8);
    Number(id.low, 8);
  }
  void Text(std::string_view text) {
    const auto *begin = reinterpret_cast<const std::byte *>(text.data());
    bytes.insert(bytes.end(), begin, begin + text.size());
  }
};
struct Reader final {
  std::span<const std::byte> bytes;
  std::size_t cursor{};
  bool valid{true};
  std::uint64_t Number(unsigned width) {
    if (!valid || width > bytes.size() - cursor) {
      valid = false;
      return 0;
    }
    std::uint64_t result{};
    for (unsigned i = 0; i < width; ++i)
      result |= std::to_integer<std::uint64_t>(bytes[cursor++]) << (8 * i);
    return result;
  }
  Uuid Id() {
    const auto high = Number(8);
    return {high, Number(8)};
  }
  std::string Text(std::size_t count) {
    if (!valid || count > bytes.size() - cursor) {
      valid = false;
      return {};
    }
    std::string result(reinterpret_cast<const char *>(bytes.data() + cursor), count);
    cursor += count;
    return result;
  }
};
constexpr std::string_view magic = "NXPFAB1\n";
bool Directory(const std::filesystem::path &path) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  return !ec && std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status);
}
std::filesystem::path Destination(const ProjectWorkspace &workspace, Uuid id) {
  return workspace.Root() / ".nexora/prefabs" / (id.ToString() + ".nxprefab");
}
std::optional<std::vector<std::byte>> ReadFile(const std::filesystem::path &path) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec || !std::filesystem::is_regular_file(status) ||
      std::filesystem::hard_link_count(path, ec) != 1 || ec)
    return {};
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || size > PrefabAssets::kMaximumAssetBytes)
    return {};
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return {};
  std::vector<std::byte> bytes;
  std::array<char, 16384> chunk;
  while (input) {
    input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    const auto count = static_cast<std::size_t>(input.gcount());
    if (count > PrefabAssets::kMaximumAssetBytes - bytes.size())
      return {};
    const auto *begin = reinterpret_cast<const std::byte *>(chunk.data());
    bytes.insert(bytes.end(), begin, begin + count);
  }
  return input.eof() && !input.bad() ? std::optional(std::move(bytes)) : std::nullopt;
}
} // namespace

bool PrefabAssets::Validate(const PrefabAsset &asset) {
  if (asset.id.IsNil() || !asset.revision || asset.scene_bytes.empty() ||
      asset.scene_bytes.size() > kMaximumSceneBytes || asset.nodes.size() > kMaximumNodes ||
      asset.nested.size() > kMaximumInstances ||
      (asset.base && (!Reference(*asset.base) || asset.base->asset == asset.id)))
    return false;
  runtime::World world;
  SceneDocument probe(world, world.LoadScene("Prefab validation"));
  if (!probe.ReloadBytes(asset.scene_bytes, kMaximumNodes))
    return false;
  const auto source_nodes = probe.Nodes();
  if (source_nodes.size() != asset.nodes.size())
    return false;
  std::set<Key> node_ids, property_ids, instances;
  std::size_t properties{}, metadata_bytes{};
  runtime::Id previous{};
  for (const auto &node : asset.nodes) {
    if (!node.serialized_node || node.serialized_node <= previous || node.id.IsNil() ||
        !node_ids.insert(Identity(node.id)).second || !probe.Key(node.serialized_node) ||
        node.properties.size() > 64 || node.properties.size() > kMaximumProperties - properties)
      return false;
    previous = node.serialized_node;
    properties += node.properties.size();
    const auto fields = Fields(probe, node.serialized_node);
    if (fields.size() != node.properties.size())
      return false;
    for (std::size_t i = 0; i < fields.size(); ++i) {
      const auto &property = node.properties[i];
      if (property.id.IsNil() || property.field != fields[i] ||
          !property_ids.insert(Identity(property.id)).second)
        return false;
      metadata_bytes += 20 + property.field.size();
    }
    metadata_bytes += 28;
  }
  if (metadata_bytes > kMaximumAssetBytes - asset.scene_bytes.size() - 72)
    return false;
  for (const auto &nested : asset.nested)
    if (nested.instance.IsNil() || !instances.insert(Identity(nested.instance)).second ||
        !node_ids.contains(Identity(nested.attachment)) || !Reference(nested.source) ||
        nested.source.asset == asset.id)
      return false;
  return metadata_bytes + asset.scene_bytes.size() + 72 + asset.nested.size() * 56 <=
         kMaximumAssetBytes;
}

std::optional<PrefabAsset> PrefabAssets::Capture(Uuid id, const SceneDocument &scene,
                                                 const std::function<Uuid()> &new_identity,
                                                 const PrefabAsset *previous) {
  if (id.IsNil() || !new_identity || (previous && !Validate(*previous)))
    return {};
  // The caller-owned previous asset must not remain borrowed while its identity callback runs.
  const auto previous_snapshot = previous ? std::optional(*previous) : std::nullopt;
  previous = previous_snapshot ? &*previous_snapshot : nullptr;
  auto source_nodes = scene.Nodes();
  if (source_nodes.size() > kMaximumNodes)
    return {};
  const auto saved = scene.PrepareSave();
  if (!saved || saved->Bytes().size() > kMaximumSceneBytes)
    return {};
  PrefabAsset asset;
  asset.id = id;
  asset.scene_bytes = saved->Bytes();
  runtime::World captured_world;
  SceneDocument captured(captured_world, captured_world.LoadScene("Prefab capture"));
  if (!captured.ReloadBytes(asset.scene_bytes, kMaximumNodes))
    return {};
  source_nodes = captured.Nodes();
  if (previous) {
    if (previous->id == id) {
      if (previous->revision == std::numeric_limits<std::uint64_t>::max())
        return {};
      asset.revision = previous->revision + 1;
      asset.base = previous->base;
    } else
      asset.base = PrefabRevisionReference{previous->id, previous->revision};
    asset.nested = previous->nested;
  }
  std::ranges::sort(source_nodes, {}, &SceneDocument::NodeView::id);
  for (const auto &source : source_nodes) {
    const PrefabNodeIdentity *old{};
    if (previous) {
      const auto found =
          std::ranges::find(previous->nodes, source.id, &PrefabNodeIdentity::serialized_node);
      if (found != previous->nodes.end())
        old = &*found;
    }
    PrefabNodeIdentity node{old ? old->id : new_identity(), source.id, {}};
    const auto fields = Fields(captured, source.id);
    for (const auto &field : fields) {
      const PrefabPropertyIdentity *old_property{};
      if (old) {
        const auto found =
            std::ranges::find(old->properties, field, &PrefabPropertyIdentity::field);
        if (found != old->properties.end())
          old_property = &*found;
      }
      node.properties.push_back({old_property ? old_property->id : new_identity(), field});
    }
    asset.nodes.push_back(std::move(node));
  }
  return Validate(asset) ? std::optional(std::move(asset)) : std::nullopt;
}

std::optional<std::vector<std::byte>> PrefabAssets::Encode(const PrefabAsset &asset) {
  if (!Validate(asset))
    return {};
  Writer w;
  w.Text(magic);
  w.Id(asset.id);
  w.Number(asset.revision, 8);
  w.Id(asset.base ? asset.base->asset : Uuid{});
  w.Number(asset.base ? asset.base->revision : 0, 8);
  w.Number(asset.nodes.size(), 4);
  w.Number(asset.nested.size(), 4);
  w.Number(asset.scene_bytes.size(), 8);
  for (const auto &node : asset.nodes) {
    w.Id(node.id);
    w.Number(node.serialized_node, 8);
    w.Number(node.properties.size(), 4);
    for (const auto &property : node.properties) {
      w.Id(property.id);
      w.Number(property.field.size(), 4);
      w.Text(property.field);
    }
  }
  for (const auto &nested : asset.nested) {
    w.Id(nested.instance);
    w.Id(nested.attachment);
    w.Id(nested.source.asset);
    w.Number(nested.source.revision, 8);
  }
  w.Text(asset.scene_bytes);
  return w.bytes.size() <= kMaximumAssetBytes ? std::optional(std::move(w.bytes)) : std::nullopt;
}
std::optional<PrefabAsset> PrefabAssets::Decode(std::span<const std::byte> bytes) {
  if (bytes.size() < 72 || bytes.size() > kMaximumAssetBytes)
    return {};
  Reader r{bytes};
  if (r.Text(magic.size()) != magic)
    return {};
  PrefabAsset asset;
  asset.id = r.Id();
  asset.revision = r.Number(8);
  const auto base_id = r.Id();
  const auto base_revision = r.Number(8);
  if (base_id.IsNil() != (base_revision == 0))
    return {};
  if (!base_id.IsNil())
    asset.base = PrefabRevisionReference{base_id, base_revision};
  const auto nodes = r.Number(4), nested = r.Number(4), scene_bytes = r.Number(8);
  if (nodes > kMaximumNodes || nested > kMaximumInstances || !scene_bytes ||
      scene_bytes > kMaximumSceneBytes)
    return {};
  std::size_t properties{};
  for (std::uint64_t i = 0; i < nodes; ++i) {
    PrefabNodeIdentity node;
    node.id = r.Id();
    node.serialized_node = r.Number(8);
    const auto count = r.Number(4);
    if (count > 64 || count > kMaximumProperties - properties)
      return {};
    properties += static_cast<std::size_t>(count);
    for (std::uint64_t j = 0; j < count; ++j) {
      const auto id = r.Id();
      const auto length = r.Number(4);
      if (length > 256)
        return {};
      node.properties.push_back({id, r.Text(static_cast<std::size_t>(length))});
    }
    asset.nodes.push_back(std::move(node));
  }
  for (std::uint64_t i = 0; i < nested; ++i) {
    NestedPrefabReference reference;
    reference.instance = r.Id();
    reference.attachment = r.Id();
    reference.source.asset = r.Id();
    reference.source.revision = r.Number(8);
    asset.nested.push_back(reference);
  }
  asset.scene_bytes = r.Text(static_cast<std::size_t>(scene_bytes));
  if (!r.valid || r.cursor != bytes.size() || !Validate(asset))
    return {};
  return asset;
}

std::optional<PrefabAsset> PrefabAssets::Load(const ProjectWorkspace &workspace, Uuid id) {
  if (id.IsNil() || workspace.Root().empty() || workspace.HasRecoveryJournal() ||
      workspace.HasExternalChange() || !Directory(workspace.Root() / ".nexora") ||
      !Directory(workspace.Root() / ".nexora/prefabs"))
    return {};
  const auto bytes = ReadFile(Destination(workspace, id));
  auto asset = bytes ? Decode(*bytes) : std::nullopt;
  return asset && asset->id == id ? std::move(asset) : std::nullopt;
}
bool PrefabAssets::Publish(const ProjectWorkspace &workspace, const PrefabAsset &asset,
                           const PrefabAsset *expected, std::string *error) {
  if (error)
    error->clear();
  const auto fail = [&](const char *message) {
    if (error)
      *error = message;
    return false;
  };
  if (!workspace.Writable() || workspace.Root().empty() || workspace.HasRecoveryJournal() ||
      workspace.HasExternalChange() || !Directory(workspace.Root() / ".nexora"))
    return fail("Prefab publication requires a writable resolved project.");
  const auto bytes = Encode(asset);
  const auto previous = expected ? Encode(*expected) : std::nullopt;
  if (!bytes || (expected && (!previous || expected->id != asset.id ||
                              expected->revision == std::numeric_limits<std::uint64_t>::max() ||
                              asset.revision != expected->revision + 1)))
    return fail("Prefab identity or expected revision is invalid.");
  const auto directory = workspace.Root() / ".nexora/prefabs";
  std::error_code ec;
  const auto directory_status = std::filesystem::symlink_status(directory, ec);
  if (ec == std::errc::no_such_file_or_directory ||
      (!ec && directory_status.type() == std::filesystem::file_type::not_found)) {
    ec.clear();
    if (!std::filesystem::create_directory(directory, ec) || ec)
      return fail("Could not create prefab storage.");
  }
  if (!Directory(directory))
    return fail("Prefab storage is occupied or aliased.");
  const auto path = Destination(workspace, asset.id);
  const auto status = std::filesystem::symlink_status(path, ec);
  const bool missing = ec == std::errc::no_such_file_or_directory ||
                       (!ec && status.type() == std::filesystem::file_type::not_found);
  if (expected) {
    const auto current = ReadFile(path);
    if (!current || *current != *previous)
      return fail("Prefab source changed; preserve the existing revision.");
  } else if (!missing)
    return fail("Prefab identity is already occupied.");
  if (!detail::AtomicWriteWith(
          path,
          [&](std::ostream &output) {
            output.write(reinterpret_cast<const char *>(bytes->data()),
                         static_cast<std::streamsize>(bytes->size()));
          },
          error))
    return false;
  const auto installed = ReadFile(path);
  return installed && *installed == *bytes ? true
                                           : fail("Published prefab could not be confirmed.");
}

std::optional<ResolvedPrefabGraph> PrefabAssets::Resolve(PrefabRevisionReference root,
                                                         std::span<const PrefabAsset> sources) {
  if (!Reference(root) || sources.empty() || sources.size() > kMaximumSources)
    return {};
  std::map<Key, const PrefabAsset *> lookup;
  std::size_t input_bytes{};
  const auto account = [&](std::size_t bytes) {
    if (bytes > kMaximumGraphBytes - input_bytes)
      return false;
    input_bytes += bytes;
    return true;
  };
  for (const auto &asset : sources) {
    if (asset.nodes.size() > kMaximumNodes || asset.nested.size() > kMaximumInstances ||
        !account(72) || !account(asset.scene_bytes.size()) || !account(asset.nested.size() * 56))
      return {};
    for (const auto &node : asset.nodes) {
      if (node.properties.size() > 64 || !account(28))
        return {};
      for (const auto &property : node.properties)
        if (property.field.size() > 256 || !account(20 + property.field.size()))
          return {};
    }
  }
  for (const auto &asset : sources)
    if (!Validate(asset) || !lookup.emplace(Identity(asset.id), &asset).second)
      return {};
  ResolvedPrefabGraph result;
  std::set<Key> active, retained;
  std::map<Key, std::size_t> checked_height;
  std::size_t retained_bytes{};
  const auto visit = [&](auto &&self, PrefabRevisionReference reference,
                         const std::vector<Uuid> &scope, std::optional<Uuid> attachment,
                         bool instance, std::size_t depth) -> bool {
    if (depth > kMaximumDepth || scope.size() > kMaximumDepth)
      return false;
    const auto key = Identity(reference.asset);
    const auto found = lookup.find(key);
    if (found == lookup.end() || found->second->revision != reference.revision ||
        active.contains(key))
      return false;
    if (!instance)
      if (const auto checked = checked_height.find(key); checked != checked_height.end())
        return checked->second <= kMaximumDepth - depth;
    active.insert(key);
    const auto &asset = *found->second;
    if (retained.insert(key).second) {
      const auto encoded = Encode(asset);
      if (!encoded || encoded->size() > kMaximumGraphBytes - retained_bytes)
        return false;
      retained_bytes += encoded->size();
      result.assets.push_back(asset);
    }
    if (instance) {
      if (result.instances.size() == kMaximumInstances ||
          asset.nodes.size() > kMaximumNodes - result.expanded_nodes)
        return false;
      result.expanded_nodes += asset.nodes.size();
      result.instances.push_back({scope, reference, attachment});
    }
    std::size_t height{};
    if (asset.base) {
      if (!self(self, *asset.base, scope, {}, false, depth + 1))
        return false;
      height = checked_height.at(Identity(asset.base->asset)) + 1;
    }
    for (const auto &nested : asset.nested) {
      auto nested_scope = scope;
      nested_scope.push_back(nested.instance);
      if (!self(self, nested.source, nested_scope, nested.attachment, instance, depth + 1))
        return false;
      height = std::max(height, checked_height.at(Identity(nested.source.asset)) + 1);
    }
    checked_height[key] = height;
    active.erase(key);
    return true;
  };
  return visit(visit, root, {}, {}, true, 0) ? std::optional(std::move(result)) : std::nullopt;
}
} // namespace nexora::editor
