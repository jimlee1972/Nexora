#include "Nexora/Editor/GameBuildCapture.h"
#include "AtomicFile.h"
#include "Nexora/Editor/MaterialAssetCatalog.h"
#include "Nexora/Editor/MeshAssetCatalog.h"
#include "ReimportSource.h"
#include <algorithm>
#include <sstream>

namespace nexora::editor {
namespace {
template <class T> std::optional<T> Fail(std::string *error, std::string message) {
  if (error)
    *error = std::move(message);
  return {};
}
bool Alias(const std::filesystem::path &path, const std::filesystem::file_status &status) {
  if (std::filesystem::is_symlink(status))
    return true;
#if defined(_WIN32)
  const auto flags = GetFileAttributesW(path.c_str());
  return flags == INVALID_FILE_ATTRIBUTES || (flags & FILE_ATTRIBUTE_REPARSE_POINT);
#else
  static_cast<void>(path);
  return false;
#endif
}
#if NEXORA_ASSET_PIPELINE_ENABLED
std::string_view Text(const runtime::ByteBuffer &bytes) {
  return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}
bool Identity(const GameBuildSource &source, const ContentItem &item) {
  if (!source.bytes)
    return true; // Explicit legacy path-derived identity; absence is retained.
  std::istringstream input(std::string(Text(*source.bytes)));
  std::string schema, uuid, type, extra;
  if (!std::getline(input, schema) || !std::getline(input, uuid) || !std::getline(input, type) ||
      std::getline(input, extra))
    return false;
  for (auto *line : {&schema, &uuid, &type})
    if (line->ends_with('\r'))
      line->pop_back();
  if (schema != "schema=1" || !uuid.starts_with("uuid=") || !type.starts_with("type="))
    return false;
  auto kind = type.substr(5);
  for (auto &c : kind)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + ('a' - 'A'));
  return runtime::AssetUuid::Parse(std::string_view(uuid).substr(5)) == item.id &&
         kind == item.type;
}
#endif
} // namespace

std::optional<GameBuildSource> ReadGameBuildSource(const std::filesystem::path &root,
                                                   const std::filesystem::path &relative,
                                                   std::size_t maximum_bytes, bool allow_missing,
                                                   std::string *error) {
  if (error)
    error->clear();
  const auto name = detail::PathUtf8(relative);
  if (relative.empty() || relative.is_absolute() || relative.has_root_name() ||
      name.size() > 1024 || !foundation::IsValidUtf8(name) ||
      name.find_first_of("\\:<>\"|?*") != name.npos ||
      std::ranges::any_of(name, [](unsigned char c) { return c < 32 || c == 127; }) ||
      std::ranges::any_of(relative, [](const auto &part) { return part == "." || part == ".."; }))
    return Fail<GameBuildSource>(error, "Build source path is not safe and project-relative.");
  for (const auto &part : relative) {
    auto text = detail::PathUtf8(part);
    if (text.empty() || text.back() == '.' || text.back() == ' ')
      return Fail<GameBuildSource>(error, "Build source uses a nonportable path alias.");
    for (auto &c : text)
      if (c >= 'A' && c <= 'Z')
        c = static_cast<char>(c + ('a' - 'A'));
    const auto stem = text.substr(0, text.find('.'));
    if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" ||
        (stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) &&
         stem.back() >= '1' && stem.back() <= '9'))
      return Fail<GameBuildSource>(error, "Build source uses a reserved device filename.");
  }
  std::error_code ec;
  if (std::filesystem::canonical(root, ec) != root || ec)
    return Fail<GameBuildSource>(error, "Build source root is not the current physical directory.");
  auto path = root;
  for (const auto &part : relative) {
    path /= part;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory ||
        (!ec && status.type() == std::filesystem::file_type::not_found)) {
      if (allow_missing)
        return GameBuildSource{relative, std::nullopt};
      return Fail<GameBuildSource>(error, "Build source is missing: " + name);
    }
    if (ec || Alias(path, status) ||
        (path != root / relative && !std::filesystem::is_directory(status)) ||
        (path == root / relative && !std::filesystem::is_regular_file(status)))
      return Fail<GameBuildSource>(error, "Build source is unavailable or an alias: " + name);
  }
  const auto count = std::filesystem::file_size(path, ec);
  if (ec || count > maximum_bytes)
    return Fail<GameBuildSource>(error, "Build source exceeds its byte budget: " + name);
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return Fail<GameBuildSource>(error, "Build source could not be opened: " + name);
  runtime::ByteBuffer bytes;
  std::array<char, 16384> chunk{};
  while (input.read(chunk.data(), chunk.size()) || input.gcount() > 0) {
    const auto size = static_cast<std::size_t>(input.gcount());
    if (size > maximum_bytes - bytes.size())
      return Fail<GameBuildSource>(error, "Build source grew beyond its byte budget: " + name);
    const auto *first = reinterpret_cast<const std::byte *>(chunk.data());
    bytes.insert(bytes.end(), first, first + size);
  }
  if (!input.eof())
    return Fail<GameBuildSource>(error, "Build source read failed: " + name);
  return GameBuildSource{relative, std::move(bytes)};
}
bool GameBuildSourcesCurrent(const std::filesystem::path &root,
                             std::span<const GameBuildSource> sources, std::string *error) {
  if (error)
    error->clear();
  std::size_t total{};
  if (sources.size() > kMaximumGameBuildSourceFiles) {
    if (error)
      *error = "Build source baseline exceeds its file-count budget.";
    return false;
  }
  for (const auto &expected : sources) {
    const auto size = expected.bytes ? expected.bytes->size() : 0;
    if (size > kMaximumGameBuildSourceBytes - total) {
      if (error)
        *error = "Build source baseline exceeds its byte budget.";
      return false;
    }
    total += size;
    const auto current = ReadGameBuildSource(root, expected.relative, size, !expected.bytes, error);
    if (!current || current->bytes != expected.bytes) {
      if (error && error->empty())
        *error = "Build source changed: " + detail::PathUtf8(expected.relative);
      return false;
    }
  }
  return true;
}

std::optional<GameBuildCapture> CaptureGameBuild(const ProjectWorkspace &workspace,
                                                 const ContentBrowserModel &content,
                                                 const MeshAssetCatalog &meshes,
                                                 const MaterialAssetCatalog &materials,
                                                 std::string *error) {
  if (error)
    error->clear();
#if !NEXORA_ASSET_PIPELINE_ENABLED
  static_cast<void>(workspace);
  static_cast<void>(content);
  static_cast<void>(meshes);
  static_cast<void>(materials);
  return Fail<GameBuildCapture>(error, "Game builds require the Asset Pipeline feature.");
#else
  if (workspace.HasExternalChange())
    return Fail<GameBuildCapture>(error, "Project descriptor changed; reopen before building.");
  GameBuildCapture result;
  std::size_t total{};
  const auto add = [&](const std::filesystem::path &path, std::size_t budget,
                       bool missing = false) -> const GameBuildSource * {
    const auto existing = std::ranges::find(result.sources, path, &GameBuildSource::relative);
    if (existing != result.sources.end())
      return &*existing;
    if (result.sources.size() == kMaximumGameBuildSourceFiles)
      return nullptr;
    auto source =
        ReadGameBuildSource(workspace.Root(), path,
                            std::min(budget, kMaximumGameBuildSourceBytes - total), missing, error);
    if (!source)
      return nullptr;
    total += source->bytes ? source->bytes->size() : 0;
    result.sources.push_back(std::move(*source));
    return &result.sources.back();
  };
  const auto *saved = add("project.build.json", ProjectBuildSettings::kMaximumBytes);
  if (!saved || !saved->bytes)
    return {};
  auto settings = ParseProjectBuildSettings(Text(*saved->bytes), error);
  if (!settings || !add("project.nexora", ProjectBuildSettings::kMaximumBytes))
    return {};
  auto input = CaptureGameProject(workspace, *settings, content, meshes, materials, error);
  if (!input)
    return {};
  result.project = std::move(*input);
  for (const auto &scene : result.project.scenes) {
    const auto path =
        std::filesystem::path(std::u8string(scene.entry.path.begin(), scene.entry.path.end()));
    const auto *source = add(path, runtime::kCookedSceneMaximumSnapshotBytes);
    if (!source || !source->bytes)
      return {};
    runtime::World world;
    const auto id = world.LoadScene("Game build capture");
    SceneDocument document(world, id);
    if (!document.ReloadBytes(Text(*source->bytes)))
      return Fail<GameBuildCapture>(error, "Saved game scene bytes could not be recaptured.");
    const auto captured = document.CaptureRuntimeScene(error);
    bool same = captured && captured->runtime_snapshot == scene.capture.runtime_snapshot &&
                captured->nodes.size() == scene.capture.nodes.size();
    if (same)
      for (std::size_t i = 0; i < captured->nodes.size(); ++i)
        same = same && captured->nodes[i].key.id == scene.capture.nodes[i].key.id &&
               captured->nodes[i].opaque == scene.capture.nodes[i].opaque;
    // Document/entity generations belong to each isolated reader, not to serialized source data.
    if (!same)
      return Fail<GameBuildCapture>(error, "Saved game scene changed during capture.");
    const auto *identity = add(AssetWorkspace::IdentitySidecar(path), 4096, true);
    const auto *item = content.Find(scene.entry.id);
    if (!identity || (item && !Identity(*identity, *item)))
      return Fail<GameBuildCapture>(
          error, "Saved scene identity changed; refresh Content before building.");
  }
  const auto asset = [&](runtime::AssetUuid id, bool mesh) {
    const auto *item = content.Find(id);
    const auto geometry = meshes.ResolveAsset(id, content.ProjectGeneration());
    const auto material = materials.ResolveAsset(id, content.ProjectGeneration());
    if (!item || (mesh ? (!geometry || geometry->geometry != item->mesh)
                       : (!material || material->material != item->material)))
      return false;
    const auto *source = add(item->path, kMaximumGameBuildSourceBytes);
    if (!source || !source->bytes)
      return false;
    const auto hash = detail::SourceHashHex(detail::HashSourceChunk(
        Text(*source->bytes), detail::HashSourceChunk(id.ToString(), 1469598103934665603ULL)));
    if (hash != item->artifact_hash)
      return false;
    const auto *identity = add(AssetWorkspace::IdentitySidecar(item->path), 4096, true);
    return identity && Identity(*identity, *item);
  };
  for (const auto &mesh : result.project.meshes)
    if (!asset(mesh.asset, true))
      return Fail<GameBuildCapture>(error,
                                    "Mesh source/catalog changed; reimport before building.");
  for (const auto &material : result.project.materials)
    if (!asset(material.asset, false))
      return Fail<GameBuildCapture>(error,
                                    "Material source/catalog changed; reimport before building.");
  if (workspace.HasExternalChange() ||
      !GameBuildSourcesCurrent(workspace.Root(), result.sources, error))
    return {};
  return result;
#endif
}
} // namespace nexora::editor
