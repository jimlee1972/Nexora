#include "Nexora/Editor/SpriteSourceCreation.h"
#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/Editor/TwoDAssetImport.h"
#include "Nexora/TwoD/Scene.h"

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <random>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace nexora::editor {
namespace {
using Error = SpriteSourceCreationError;
using State = SpriteSourceCreationState;
std::string Fold(std::string text) {
  for (auto &c : text)
    if (c >= 'A' && c <= 'Z')
      c = char(c + ('a' - 'A'));
  return text;
}
std::string Utf8(const std::filesystem::path &path) {
  const auto text = path.generic_u8string();
  return {text.begin(), text.end()};
}
bool Missing(const std::filesystem::path &p) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(p, ec);
  return ec == std::errc::no_such_file_or_directory ||
         (!ec && status.type() == std::filesystem::file_type::not_found);
}
bool PlainDirectory(const std::filesystem::path &p) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(p, ec);
  return !ec && std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status);
}
bool Folder(const std::filesystem::path &root, const std::filesystem::path &relative) {
  if (relative.empty() || relative.has_root_path() || *relative.begin() != "Content")
    return false;
  auto current = root;
  for (const auto &part : relative) {
    if (part.empty() || part == "." || part == "..")
      return false;
    current /= part;
    if (!PlainDirectory(current))
      return false;
  }
  return true;
}
struct Temporary final {
  std::filesystem::path file, directory;
  bool created{};
  ~Temporary() {
    if (created) {
      std::error_code ec;
      std::filesystem::remove(file, ec);
      std::filesystem::remove(directory, ec);
    }
  }
};
} // namespace

bool ValidSpriteSourceName(std::string_view name) noexcept {
  if (name.empty() || name.size() > 128 || !foundation::IsValidUtf8(name) || name.back() == '.' ||
      name.back() == ' ' || name == "." || name == "..")
    return false;
  for (const unsigned char c : name)
    if (c < 32 || c == 127 ||
        std::string_view("/\\:<>\"|?*").find(char(c)) != std::string_view::npos)
      return false;
  // Windows device names remain reserved even with additional filename extensions.
  const auto base = name.substr(0, name.find('.'));
  std::array<char, 128> folded{};
  for (std::size_t i = 0; i < base.size(); ++i)
    folded[i] = base[i] >= 'A' && base[i] <= 'Z' ? char(base[i] + ('a' - 'A')) : base[i];
  const std::string_view stem(folded.data(), base.size());
  return stem != "con" && stem != "prn" && stem != "aux" && stem != "nul" && stem != "conin$" &&
         stem != "conout$" &&
         !(stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) &&
           stem[3] >= '1' && stem[3] <= '9');
}
std::string_view SpriteSourceCreationMessage(Error e) noexcept {
  switch (e) {
  case Error::None:
    return "Sprite source file created.";
  case Error::Denied:
    return "Source creation requires a writable, idle project and stopped Play.";
  case Error::Stale:
    return "Project or Content changed. Reopen source creation to refresh it.";
  case Error::Name:
    return "Choose a portable source name of at most 128 UTF-8 bytes.";
  case Error::Destination:
    return "The destination must be an existing ordinary Content folder.";
  case Error::Atlas:
    return "The selected imported Atlas or region is unavailable.";
  case Error::Geometry:
    return "Source dimensions, trim, Pivot or PPU are invalid.";
  case Error::Capacity:
    return "The project asset or Sprite source capacity is exhausted.";
  case Error::Content:
    return "Content cannot admit this source. A pending Undo may reserve its name or capacity.";
  case Error::Exists:
    return "A source or identity with this name already exists. Choose another name.";
  case Error::Storage:
    return "The new source could not be staged. Existing files are preserved.";
  case Error::Publication:
    return "Source publication is uncertain. Inspect Content before retrying.";
  }
  return "Source creation failed.";
}

SpriteSourceCreationResult CreateSpriteSourceFile(const ProjectWorkspace &workspace,
                                                  const ProjectContentSession &content,
                                                  const SpriteSourceCreationRequest &request,
                                                  bool authorized) noexcept {
  SpriteSourceCreationResult result;
  const auto fail = [&](Error e) {
    result.error = e;
    return std::move(result);
  };
  try {
    if (!authorized || !workspace.Writable() || !content.Writable() || content.ReimportBusy() ||
        workspace.HasRecoveryJournal() || workspace.HasExternalChange())
      return fail(Error::Denied);
    const auto &browser = content.Browser();
    if (request.project != workspace.Project().id || request.root != workspace.Root() ||
        content.Root() != request.root || !request.version.project_generation ||
        request.version !=
            SpriteSourceContentVersion{browser.ProjectGeneration(), browser.Revision()})
      return fail(Error::Stale);
    if (!ValidSpriteSourceName(request.name))
      return fail(Error::Name);
    if (!Folder(request.root, request.folder))
      return fail(Error::Destination);
    const auto *item = browser.Find(request.source.atlas);
    const auto atlas = item && item->twod ? item->twod->Atlas() : nullptr;
    if (!atlas || item->id != atlas->id || item->type != item->twod->Type() ||
        item->thumbnail != ThumbnailState::Ready)
      return fail(Error::Atlas);
    auto source = request.source;
    if (source.id != runtime::AssetUuid{})
      return fail(Error::Geometry);
    source.id =
        atlas->id == runtime::AssetUuid{1, 1} ? runtime::AssetUuid{1, 2} : runtime::AssetUuid{1, 1};
    twod::SpriteInstance witness;
    witness.id = 1;
    witness.matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    if (!twod::ResolveSpriteAsset(source, *atlas, std::move(witness)))
      return fail(Error::Geometry);
    std::size_t count{}, bytes{};
    for (const auto &row : browser.Items())
      if (row.twod) {
        ++count;
        bytes += row.twod->RetainedBytes();
      }
    if (browser.Items().size() >= AssetIndexLimits{}.max_assets ||
        count >= kMaximumWorkspaceTwoDAssets ||
        bytes > kMaximumWorkspaceTwoDBytes - sizeof(TwoDAssetPayload) - sizeof(source))
      return fail(Error::Capacity);
    result.path = request.folder /
                  std::filesystem::path(std::u8string(request.name.begin(), request.name.end()) +
                                        u8".nx2sprite");
    if (Utf8(result.path.lexically_relative("Content")).size() >= AssetIndexLimits{}.max_path_bytes)
      return fail(Error::Destination);
    const auto destination = request.root / result.path;
    auto sidecar = AssetWorkspace::IdentitySidecar(destination);
    if (!Missing(destination) || !Missing(sidecar))
      return fail(Error::Exists);
    const auto wanted = Fold(Utf8(destination.filename()));
    const auto identity = Fold(Utf8(sidecar.filename()));
    std::size_t names{};
    for (const auto &entry : std::filesystem::directory_iterator(destination.parent_path())) {
      if (++names > 65536)
        return fail(Error::Capacity);
      const auto name = Fold(Utf8(entry.path().filename()));
      if (name == wanted || name == identity)
        return fail(Error::Exists);
    }
    std::random_device random;
    for (int attempt = 0; attempt < 8; ++attempt) {
      const auto word = [&] { return (std::uint64_t(random()) << 32) | random(); };
      source.id = {word(), word()};
      if (source.id != runtime::AssetUuid{} && source.id != atlas->id && !browser.Find(source.id))
        break;
      source.id = {};
    }
    if (source.id == runtime::AssetUuid{})
      return fail(Error::Capacity);
    result.asset = source.id;
    const auto encoded = twod::EncodeSpriteAsset(source);
    if (encoded.size() != 96)
      return fail(Error::Geometry);
    {
      // Validate the live and retained Undo sets before any primary file is published.
      // Discover stages only this private copy; it never changes the owner's view/history.
      auto admission = browser;
      const auto imported = ImportTwoDAsset(".nx2sprite", encoded, source.id);
      if (!imported.payload || !admission.Discover({source.id,
                                                    result.path,
                                                    ".nx2sprite",
                                                    {},
                                                    ThumbnailState::Ready,
                                                    {},
                                                    {},
                                                    imported.payload}))
        return fail(Error::Content);
    }
    const auto metadata = request.root / ".nexora";
    const auto staging = metadata / "source-staging";
    if (!PlainDirectory(metadata))
      return fail(Error::Storage);
    std::error_code ec;
    std::filesystem::create_directory(staging, ec);
    if (ec || !PlainDirectory(staging))
      return fail(Error::Storage);
    Temporary temporary{staging / source.id.ToString() / "source.bin",
                        staging / source.id.ToString()};
    temporary.created = std::filesystem::create_directory(temporary.directory, ec);
    if (ec || !temporary.created)
      return fail(Error::Storage);
    {
      std::ofstream output(temporary.file, std::ios::binary);
      output.write(reinterpret_cast<const char *>(encoded.data()), std::streamsize(encoded.size()));
      output.close();
      if (output.fail())
        return fail(Error::Storage);
    }
    // Repeat authority at the publication boundary; all result storage is already allocated.
    if (!workspace.Writable() || workspace.HasRecoveryJournal() || workspace.HasExternalChange())
      return fail(Error::Denied);
#if defined(_WIN32)
    const bool published =
        MoveFileExW(temporary.file.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
    const auto error = published ? ERROR_SUCCESS : GetLastError();
    const bool occupied = error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS;
#else
    const bool published = ::link(temporary.file.c_str(), destination.c_str()) == 0;
    const bool occupied = !published && errno == EEXIST;
#endif
    if (published) {
      result.state = State::Created;
      result.error = Error::None;
    } else if (occupied) {
      result.error = Error::Exists;
    } else {
      result.state = State::Uncertain;
      result.error = Error::Publication;
    }
    return result;
  } catch (...) {
    // Publication is the final throwing-free step. Earlier allocation/IO failures own no target.
    return fail(Error::Storage);
  }
}
} // namespace nexora::editor
