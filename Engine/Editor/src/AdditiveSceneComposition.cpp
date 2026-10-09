#include "Nexora/Editor/AdditiveSceneComposition.h"
#include "AtomicFile.h"
#include "SceneCompositionTestAccess.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace nexora::editor {
namespace {
constexpr std::size_t kMetadataLimit = 32 * 1024;
struct Disk final {
  bool exists{};
  std::string bytes;
  bool operator==(const Disk &) const = default;
};
struct Composition final {
  std::vector<SceneCompositionEntry> rows;
  std::size_t active{};
};
bool Fail(std::string *error, const char *message) {
  if (error)
    *error = message;
  return false;
}
std::filesystem::path Metadata(const ProjectWorkspace &workspace) {
  return workspace.Root() / ".nexora/scene-composition.ini";
}
std::optional<Disk> ReadDisk(const ProjectWorkspace &workspace) {
  std::error_code error;
  const auto parent = Metadata(workspace).parent_path();
  if (!std::filesystem::is_directory(std::filesystem::symlink_status(parent, error)) || error)
    return std::nullopt;
  const auto path = Metadata(workspace);
  const auto status = std::filesystem::symlink_status(path, error);
  if (error == std::errc::no_such_file_or_directory ||
      (!error && status.type() == std::filesystem::file_type::not_found))
    return Disk{};
  if (error || !std::filesystem::is_regular_file(status) ||
      std::filesystem::hard_link_count(path, error) != 1 || error)
    return std::nullopt;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size > kMetadataLimit)
    return std::nullopt;
  Disk disk{true, std::string(static_cast<std::size_t>(size), '\0')};
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return std::nullopt;
  input.read(disk.bytes.data(), static_cast<std::streamsize>(disk.bytes.size()));
  if (input.bad() || static_cast<std::size_t>(input.gcount()) != disk.bytes.size() ||
      input.peek() != std::char_traits<char>::eof() || !input.eof())
    return std::nullopt;
  return disk;
}
std::string PortableKey(const std::filesystem::path &path) {
  const auto bytes = path.generic_u8string();
  std::string key(bytes.begin(), bytes.end());
  std::ranges::transform(key, key.begin(), [](unsigned char byte) {
    return static_cast<char>(byte >= 'A' && byte <= 'Z' ? byte + 32 : byte);
  });
  return key;
}
bool ValidPath(const std::filesystem::path &path) {
  const auto encoded = path.generic_u8string();
  const std::string text(encoded.begin(), encoded.end());
  if (text.empty() || text.size() >= 1024 || !foundation::IsValidUtf8(text) ||
      path.has_root_path() || path.extension() != ".scene" ||
      text.find_first_of("\\:<>\"|?*") != std::string::npos ||
      std::ranges::any_of(text, [](unsigned char byte) { return byte < 32 || byte == 127; }))
    return false;
  for (const auto &part : path) {
    const auto bytes = part.generic_u8string();
    if (part.empty() || part == "." || part == ".." || bytes.back() == '.' || bytes.back() == ' ')
      return false;
  }
  return true;
}
std::optional<Composition> Decode(const ProjectWorkspace &workspace, std::string_view bytes) {
  if (bytes.size() > kMetadataLimit || !foundation::IsValidUtf8(bytes))
    return std::nullopt;
  std::istringstream input{std::string(bytes)};
  input.imbue(std::locale::classic());
  std::string magic, project;
  unsigned version{};
  std::size_t count{}, active{};
  if (!(input >> magic >> version >> project >> count >> active) ||
      magic != "NEXORA_SCENE_COMPOSITION" || version != 1 ||
      project != workspace.Project().id.ToString() || !count ||
      count > AdditiveSceneSession::kMaximumDocuments || active >= count)
    return std::nullopt;
  Composition result{{}, active};
  std::unordered_set<std::string> paths;
  for (std::size_t index = 0; index < count; ++index) {
    std::string path;
    unsigned owned{};
    std::size_t dependency_count{};
    if (!(input >> std::quoted(path) >> owned >> dependency_count) || owned > 1 ||
        dependency_count > index || !foundation::IsValidUtf8(path))
      return std::nullopt;
    SceneCompositionEntry row{
        std::filesystem::path(std::u8string(path.begin(), path.end())), owned != 0, {}};
    if (!ValidPath(row.path) || !paths.insert(PortableKey(row.path)).second)
      return std::nullopt;
    std::unordered_set<std::size_t> dependencies;
    for (std::size_t edge = 0; edge < dependency_count; ++edge) {
      std::size_t parent{};
      if (!(input >> parent) || parent >= index || !dependencies.insert(parent).second)
        return std::nullopt;
      row.dependencies.push_back(parent);
    }
    result.rows.push_back(std::move(row));
  }
  input >> std::ws;
  return input.eof() ? std::optional(std::move(result)) : std::nullopt;
}
std::string Encode(const ProjectWorkspace &workspace, const Composition &composition) {
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << "NEXORA_SCENE_COMPOSITION 1\n"
         << workspace.Project().id.ToString() << '\n'
         << composition.rows.size() << ' ' << composition.active << '\n';
  for (const auto &row : composition.rows) {
    const auto bytes = row.path.generic_u8string();
    output << std::quoted(std::string(bytes.begin(), bytes.end())) << ' ' << row.owned << ' '
           << row.dependencies.size();
    for (const auto dependency : row.dependencies)
      output << ' ' << dependency;
    output << '\n';
  }
  return output.str();
}
} // namespace
struct AdditiveSceneComposition::State final {
  AdditiveSceneSession &session;
  const ProjectWorkspace &workspace;
  std::filesystem::path root;
  foundation::Uuid project;
  std::optional<Disk> baseline;
  std::function<void()> before_restore_publish;
  explicit State(AdditiveSceneSession &owner, const ProjectWorkspace &project_workspace)
      : session(owner), workspace(project_workspace), root(workspace.Root()),
        project(workspace.Project().id) {}
  bool Current() const {
    return workspace.Root() == root && workspace.Project().id == project && !root.empty();
  }
};
AdditiveSceneComposition::AdditiveSceneComposition(AdditiveSceneSession &session)
    : state_(std::make_unique<State>(session, session.workspace_)) {}
AdditiveSceneComposition::~AdditiveSceneComposition() = default;
std::optional<std::filesystem::path>
AdditiveSceneComposition::BootstrapScene(const ProjectWorkspace &workspace, std::string *error) {
  if (error)
    error->clear();
  const auto disk = workspace.Root().empty() ? std::nullopt : ReadDisk(workspace);
  if (!disk) {
    Fail(error, "Scene composition metadata is unsafe, unavailable or exceeds its byte limit.");
    return std::nullopt;
  }
  if (!disk->exists)
    return std::nullopt;
  const auto decoded = Decode(workspace, disk->bytes);
  if (!decoded) {
    Fail(error, "Saved scene composition is invalid or belongs to another project.");
    return std::nullopt;
  }
  return decoded->rows.front().path;
}
SceneCompositionStatus AdditiveSceneComposition::Restore(std::string *error) {
  auto &state = *state_;
  state.baseline.reset();
  if (!state.Current()) {
    Fail(error, "Composition belongs to a previous project scope.");
    return SceneCompositionStatus::Rejected;
  }
  if (!state.session.Allowed(error))
    return SceneCompositionStatus::Rejected;
  auto disk = ReadDisk(state.workspace);
  if (!disk) {
    Fail(error, "Scene composition metadata is unsafe, unavailable or exceeds its byte limit.");
    return SceneCompositionStatus::Rejected;
  }
  if (!disk->exists) {
    state.baseline = std::move(*disk);
    if (error)
      error->clear();
    return SceneCompositionStatus::Missing;
  }
  const auto decoded = Decode(state.workspace, disk->bytes);
  const auto metadata_current = [&] {
    if (state.before_restore_publish)
      state.before_restore_publish();
    if (!state.Current())
      return false;
    const auto current = ReadDisk(state.workspace);
    return state.Current() && current && *current == *disk;
  };
  if (!decoded ||
      !state.session.RestoreComposition(decoded->rows, decoded->active, metadata_current, error)) {
    if (!decoded)
      Fail(error, "Saved scene composition is invalid or belongs to another project.");
    return SceneCompositionStatus::Rejected;
  }
  state.baseline = std::move(*disk);
  if (error)
    error->clear();
  return SceneCompositionStatus::Restored;
}
bool AdditiveSceneComposition::Save(std::string *error) {
  auto &state = *state_;
  if (!state.Current() || !state.baseline || !state.workspace.Writable() ||
      !state.session.Allowed(error))
    return Fail(
        error,
        "Composition save needs its observed baseline, current writer and resolved recovery.");
  const auto rows = state.session.Snapshot();
  const auto active = state.session.Active();
  if (rows.empty() || !active)
    return Fail(error, "Composition save needs live named documents and active selection.");
  Composition composition;
  std::unordered_map<SceneDocumentId, std::size_t> indexes;
  for (std::size_t index = 0; index < rows.size(); ++index) {
    if (!rows[index].path || !ValidPath(*rows[index].path) || rows[index].save_blocked ||
        !state.session.Document(rows[index].id))
      return Fail(error, "Name every open scene and resolve its source before saving composition.");
    indexes.emplace(rows[index].id, index);
    if (rows[index].id == *active)
      composition.active = index;
    SceneCompositionEntry row{*rows[index].path, rows[index].owned, {}};
    for (const auto parent : rows[index].dependencies) {
      const auto found = indexes.find(parent);
      if (found == indexes.end() || found->second >= index)
        return Fail(error, "Composition dependencies must precede their document.");
      row.dependencies.push_back(found->second);
    }
    std::ranges::sort(row.dependencies);
    composition.rows.push_back(std::move(row));
  }
  std::uintmax_t payload_bytes = 0;
  for (const auto &row : rows) {
    const auto *files = state.session.Files(row.id);
    const auto path = files && files->current_ ? files->Resolve(*files->current_) : std::nullopt;
    const auto current = path ? SceneFileSession::ReadDisk(*path) : std::nullopt;
    if (!files || !files->disk_baseline_ || !current ||
        current->exists != files->disk_baseline_->exists ||
        current->bytes != files->disk_baseline_->bytes ||
        current->bytes.size() > SceneSaveBatch::kMaximumPayloadBytes - payload_bytes)
      return Fail(error, "A composition source changed or exceeds its aggregate payload budget.");
    payload_bytes += current->bytes.size();
  }
  const auto bytes = Encode(state.workspace, composition);
  if (bytes.size() > kMetadataLimit || !Decode(state.workspace, bytes))
    return Fail(error, "Composition metadata exceeds its bounds or has conflicting destinations.");
  const auto current = ReadDisk(state.workspace);
  if (!current || *current != *state.baseline)
    return Fail(error,
                "Scene composition metadata changed externally. Retained versions were preserved.");
  // Own the next baseline before IO; publication is one bounded metadata replacement.
  Disk next{true, bytes};
  if (!detail::AtomicWrite(Metadata(state.workspace), bytes, error))
    return false;
  state.baseline = std::move(next);
  if (error)
    error->clear();
  return true;
}
void SceneCompositionTestAccess::BeforeRestorePublish(AdditiveSceneComposition &composition,
                                                      std::function<void()> hook) {
  composition.state_->before_restore_publish = std::move(hook);
}
} // namespace nexora::editor
