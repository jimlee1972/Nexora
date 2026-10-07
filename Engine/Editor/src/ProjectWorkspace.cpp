#include "AtomicFile.h"
#include "Nexora/Editor/EditorProduction.h"
#include "Nexora/Editor/EditorWorkspace.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace nexora::editor {
namespace {
constexpr std::uint64_t kHashOffset = 1469598103934665603ULL;
constexpr std::uint64_t kHashPrime = 1099511628211ULL;

std::uint64_t Hash(std::string_view text, std::uint64_t seed) {
  auto value = seed;
  for (const unsigned char byte : text) {
    value ^= byte;
    value *= kHashPrime;
  }
  return value;
}

void StripCarriageReturn(std::string &line) {
  if (!line.empty() && line.back() == '\r')
    line.pop_back();
}

bool SafeLine(std::string_view value) {
  return !value.empty() && value.size() <= 1024 &&
         value.find_first_of(std::string_view("\r\n\0", 3)) == std::string_view::npos &&
         foundation::IsValidUtf8(value);
}

bool ValidateFrameProcessingSamples(std::span<const FrameSample> samples, std::string *error) {
  std::uint64_t previous = 0;
  for (const auto &sample : samples) {
    if (sample.frame == 0 || sample.frame <= previous || !std::isfinite(sample.cpu_ms) ||
        sample.cpu_ms < 0) {
      if (error)
        *error = "invalid or unordered Editor frame processing samples";
      return false;
    }
    previous = sample.frame;
  }
  return true;
}

std::string PathUtf8(const std::filesystem::path &path) {
  const auto encoded = path.generic_u8string();
  std::string result;
  result.reserve(encoded.size());
  for (const char8_t byte : encoded)
    result.push_back(static_cast<char>(byte));
  return result;
}

bool AtomicWrite(const std::filesystem::path &path, std::string_view contents, std::string *error) {
  if (error)
    error->clear();
  std::error_code ec;
  if (!path.parent_path().empty())
    std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    if (error)
      *error = "could not create " + PathUtf8(path.parent_path()) + ": " + ec.message();
    return false;
  }
  return detail::AtomicWrite(path, contents, error);
}

std::optional<std::filesystem::path> PathFromUtf8(std::string_view text) noexcept {
  if (!foundation::IsValidUtf8(text))
    return std::nullopt;
  try {
    std::u8string encoded;
    encoded.reserve(text.size());
    for (const unsigned char byte : text)
      encoded.push_back(static_cast<char8_t>(byte));
    return std::filesystem::path(encoded);
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

foundation::Uuid DerivedProjectId(const std::filesystem::path &root, std::string_view name) {
  const auto key = PathUtf8(root) + "\n" + std::string(name);
  foundation::Uuid id{Hash(key, kHashOffset), Hash(key, kHashPrime)};
  if (id.IsNil())
    id.low = 1;
  return id;
}

std::string ProjectContents(const ProjectDescriptor &project) {
  return "schema=2\nuuid=" + project.id.ToString() + "\nname=" + project.name + "\n";
}

bool ReadProjectDescriptor(const std::filesystem::path &path, ProjectDescriptor &project,
                           bool &legacy, std::string *error) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status) ||
      size > 4096) {
    if (error)
      *error = "project descriptor is unavailable, unsafe, or too large";
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  std::vector<std::string> lines;
  for (std::string line; std::getline(input, line);) {
    StripCarriageReturn(line);
    lines.push_back(std::move(line));
    if (lines.size() > 3)
      break;
  }
  if ((!input.good() && !input.eof()) || lines.empty()) {
    if (error)
      *error = "project descriptor could not be read";
    return false;
  }
  legacy = lines[0] == "schema=1";
  if (legacy && lines.size() == 2 && lines[1].starts_with("name=") &&
      SafeLine(std::string_view(lines[1]).substr(5))) {
    project = {{}, lines[1].substr(5), 1};
    return true;
  }
  if (lines[0] == "schema=2" && lines.size() == 3 && lines[1].starts_with("uuid=") &&
      lines[2].starts_with("name=") && SafeLine(std::string_view(lines[2]).substr(5))) {
    const auto parsed = foundation::Uuid::Parse(
        std::string_view(lines[1]).substr(std::string_view("uuid=").size()));
    if (parsed && !parsed.Value().IsNil()) {
      project = {parsed.Value(), lines[2].substr(5), ProjectDescriptor::kSchemaVersion};
      return true;
    }
  }
  if (error)
    *error = "invalid or unsupported project descriptor";
  return false;
}

enum class WorkspaceLineState { Line, End, Invalid };

WorkspaceLineState ReadWorkspaceLine(std::istream &input, std::string &line) {
  // Prefix, maximum UTF-8 document bytes, optional CR, and the terminating buffer NUL.
  std::array<char, 9 + ProjectWorkspace::kMaximumDocumentPathBytes + 2> buffer{};
  input.getline(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  if (input.fail())
    return input.eof() && !input.bad() && input.gcount() == 0 ? WorkspaceLineState::End
                                                              : WorkspaceLineState::Invalid;
  const auto count = static_cast<std::size_t>(input.gcount());
  line.assign(buffer.data(), count - (input.eof() ? 0 : 1));
  StripCarriageReturn(line);
  return WorkspaceLineState::Line;
}

bool ReadWorkspace(const std::filesystem::path &path, std::vector<std::string> &documents,
                   std::string *error, std::string_view label = "workspace",
                   bool missing_allowed = true) {
  const auto fail = [&](std::string_view reason) {
    if (error)
      *error = std::string(label) + std::string(reason);
    return false;
  };
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec == std::errc::no_such_file_or_directory ||
      (!ec && status.type() == std::filesystem::file_type::not_found)) {
    if (!missing_allowed)
      return fail(" is missing or unreadable");
    documents.clear();
    return true;
  }
  if (ec || !std::filesystem::is_regular_file(status))
    return fail(" is unavailable or unsafe");
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return fail(" is unreadable");
  std::string line;
  if (ReadWorkspaceLine(input, line) != WorkspaceLineState::Line)
    return fail(" is empty, overlong or unreadable");
  if (line != "schema=1")
    return fail(" uses an unsupported schema");
  std::vector<std::string> candidate;
  for (;;) {
    const auto state = ReadWorkspaceLine(input, line);
    if (state == WorkspaceLineState::End)
      break;
    if (state == WorkspaceLineState::Invalid)
      return fail(" contains an overlong or unreadable line");
    if (!line.starts_with("document=") || !SafeLine(std::string_view(line).substr(9)))
      return fail(" contains an invalid document entry");
    if (candidate.size() == ProjectWorkspace::kMaximumDocuments)
      return fail(" contains too many documents");
    candidate.push_back(line.substr(9));
  }
  documents = std::move(candidate);
  return true;
}

bool ValidateWorkspaceDocuments(std::span<const std::string> documents, std::string *error) {
  if (documents.size() > ProjectWorkspace::kMaximumDocuments) {
    if (error)
      *error = "workspace contains too many documents";
    return false;
  }
  for (const auto &document : documents) {
    if (!SafeLine(document)) {
      if (error)
        *error = "workspace document path is invalid";
      return false;
    }
  }
  return true;
}

std::string ReadLockOwner(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::string schema;
  std::string process;
  if (!input || !std::getline(input, schema) || !std::getline(input, process))
    return {};
  StripCarriageReturn(schema);
  StripCarriageReturn(process);
  if (schema == "schema=1" && process.starts_with("process="))
    return process.substr(8);
  return {};
}

std::string LockedMessage(const std::filesystem::path &path) {
  const auto owner = ReadLockOwner(path);
  return owner.empty() ? "project is already open for writing"
                       : "project is already open for writing by process " + owner;
}

bool EnsureWritable(ProjectAccess access, std::string *error) {
  if (access == ProjectAccess::ReadWrite)
    return true;
  if (error)
    *error = "project is open read-only";
  return false;
}

#if defined(_WIN32)
std::optional<std::filesystem::path> EnvironmentPath(const wchar_t *name) {
  wchar_t *value = nullptr;
  std::size_t size = 0;
  if (_wdupenv_s(&value, &size, name) != 0 || value == nullptr)
    return std::nullopt;
  const std::filesystem::path path(value);
  std::free(value);
  if (path.empty())
    return std::nullopt;
  return path;
}
#else
std::optional<std::filesystem::path> EnvironmentPath(const char *name) {
  const char *value = std::getenv(name);
  if (value == nullptr || *value == '\0')
    return std::nullopt;
  return std::filesystem::path(value);
}
#endif
} // namespace

struct ProjectWorkspace::LockState final {
  std::filesystem::path path;
#if defined(_WIN32)
  HANDLE handle = INVALID_HANDLE_VALUE;
#else
  int handle = -1;
#endif

  ~LockState() {
#if defined(_WIN32)
    if (handle != INVALID_HANDLE_VALUE) {
      OVERLAPPED overlap{};
      static_cast<void>(UnlockFileEx(handle, 0, MAXDWORD, MAXDWORD, &overlap));
      CloseHandle(handle);
    }
#else
    if (handle >= 0) {
      static_cast<void>(flock(handle, LOCK_UN));
      close(handle);
    }
#endif
  }

  static std::unique_ptr<LockState> Acquire(const std::filesystem::path &path, std::string *error) {
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory)
      ec.clear();
    else if (ec || std::filesystem::is_symlink(status) ||
             (std::filesystem::exists(status) && !std::filesystem::is_regular_file(status))) {
      if (error)
        *error = "project lock is unavailable or unsafe";
      return nullptr;
    }

    auto lock = std::make_unique<LockState>();
    lock->path = path;
#if defined(_WIN32)
    lock->handle =
        CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (lock->handle == INVALID_HANDLE_VALUE) {
      if (error)
        *error = LockedMessage(path);
      return nullptr;
    }
    BY_HANDLE_FILE_INFORMATION information{};
    if (!GetFileInformationByHandle(lock->handle, &information) ||
        (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
      if (error)
        *error = "project lock is unavailable or unsafe";
      return nullptr;
    }
    OVERLAPPED overlap{};
    if (!LockFileEx(lock->handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, MAXDWORD,
                    MAXDWORD, &overlap)) {
      if (error)
        *error = LockedMessage(path);
      return nullptr;
    }
    const auto metadata =
        "schema=1\nprocess=" + std::to_string(GetCurrentProcessId()) + "\naccess=read-write\n";
    LARGE_INTEGER start{};
    DWORD written = 0;
    if (!SetFilePointerEx(lock->handle, start, nullptr, FILE_BEGIN) ||
        !SetEndOfFile(lock->handle) ||
        !WriteFile(lock->handle, metadata.data(), static_cast<DWORD>(metadata.size()), &written,
                   nullptr) ||
        written != metadata.size() || !FlushFileBuffers(lock->handle)) {
      if (error)
        *error = "project lock metadata could not be written";
      return nullptr;
    }
#else
    int flags = O_RDWR | O_CREAT;
#if defined(O_CLOEXEC)
    flags |= O_CLOEXEC;
#endif
#if defined(O_NOFOLLOW)
    flags |= O_NOFOLLOW;
#endif
    lock->handle = open(path.c_str(), flags, 0600);
    if (lock->handle < 0) {
      if (error)
        *error = errno == ELOOP
                     ? "project lock is unavailable or unsafe"
                     : "project lock could not be opened: " + std::string(std::strerror(errno));
      return nullptr;
    }
    struct stat information{};
    if (fstat(lock->handle, &information) != 0 || !S_ISREG(information.st_mode)) {
      if (error)
        *error = "project lock is unavailable or unsafe";
      return nullptr;
    }
    if (flock(lock->handle, LOCK_EX | LOCK_NB) != 0) {
      if (error)
        *error = LockedMessage(path);
      return nullptr;
    }
    const auto metadata = "schema=1\nprocess=" + std::to_string(getpid()) + "\naccess=read-write\n";
    if (ftruncate(lock->handle, 0) != 0 || lseek(lock->handle, 0, SEEK_SET) < 0) {
      if (error)
        *error = "project lock metadata could not be written";
      return nullptr;
    }
    std::size_t offset = 0;
    while (offset < metadata.size()) {
      const auto written = write(lock->handle, metadata.data() + offset, metadata.size() - offset);
      if (written <= 0) {
        if (error)
          *error = "project lock metadata could not be written";
        return nullptr;
      }
      offset += static_cast<std::size_t>(written);
    }
    if (fsync(lock->handle) != 0) {
      if (error)
        *error = "project lock metadata could not be flushed";
      return nullptr;
    }
#endif
    return lock;
  }
};

ProjectWorkspace::ProjectWorkspace() = default;
ProjectWorkspace::~ProjectWorkspace() = default;
ProjectWorkspace::ProjectWorkspace(ProjectWorkspace &&other) noexcept { *this = std::move(other); }
ProjectWorkspace &ProjectWorkspace::operator=(ProjectWorkspace &&other) noexcept {
  if (this == &other)
    return *this;
  root_ = std::move(other.root_);
  project_ = std::move(other.project_);
  documents_ = std::move(other.documents_);
  workspace_write_time_ = other.workspace_write_time_;
  lock_ = std::move(other.lock_);
  access_ = other.access_;
  upgrade_state_ = other.upgrade_state_;
  other.root_.clear();
  other.project_ = {};
  other.documents_.clear();
  other.workspace_write_time_ = {};
  other.access_ = ProjectAccess::ReadOnly;
  other.upgrade_state_ = ProjectUpgradeState::Current;
  return *this;
}

bool ProjectWorkspace::Create(const std::filesystem::path &root, std::string name,
                              std::string *error) {
  if (error)
    error->clear();
  if (root.empty() || !SafeLine(name)) {
    if (error)
      *error = "project root or name is invalid";
    return false;
  }
  std::error_code ec;
  std::filesystem::create_directories(root / "Content", ec);
  if (!ec)
    std::filesystem::create_directories(root / ".nexora", ec);
  if (ec) {
    if (error)
      *error = "project directories could not be created: " + ec.message();
    return false;
  }
  const auto canonical_root = std::filesystem::canonical(root, ec);
  if (ec) {
    if (error)
      *error = "project root could not be resolved: " + ec.message();
    return false;
  }
  auto lock = LockState::Acquire(canonical_root / ".nexora/editor.lock", error);
  if (!lock)
    return false;
  const auto descriptor_path = canonical_root / "project.nexora";
  const auto descriptor_status = std::filesystem::symlink_status(descriptor_path, ec);
  if (ec == std::errc::no_such_file_or_directory)
    ec.clear();
  else if (ec || std::filesystem::exists(descriptor_status)) {
    if (error)
      *error = "project descriptor already exists or cannot be inspected";
    return false;
  }
  ProjectDescriptor project{DerivedProjectId(canonical_root, name), std::move(name),
                            ProjectDescriptor::kSchemaVersion};
  if (!AtomicWrite(descriptor_path, ProjectContents(project), error))
    return false;
  if (!AtomicWrite(canonical_root / ".nexora/workspace", "schema=1\n", error)) {
    std::filesystem::remove(descriptor_path, ec);
    return false;
  }
  const auto write_time =
      std::filesystem::last_write_time(canonical_root / ".nexora/workspace", ec);
  if (ec) {
    if (error)
      *error = "workspace timestamp could not be read: " + ec.message();
    return false;
  }
  root_ = canonical_root;
  project_ = std::move(project);
  documents_.clear();
  workspace_write_time_ = write_time;
  lock_ = std::move(lock);
  access_ = ProjectAccess::ReadWrite;
  upgrade_state_ = ProjectUpgradeState::Current;
  return true;
}

bool ProjectWorkspace::Open(const std::filesystem::path &root, std::string *error) {
  return Open(root, ProjectAccess::ReadWrite, error);
}

bool ProjectWorkspace::Open(const std::filesystem::path &root, ProjectAccess access,
                            std::string *error) {
  if (error)
    error->clear();
  std::error_code ec;
  const auto canonical_root = std::filesystem::canonical(root, ec);
  if (ec || !std::filesystem::is_directory(canonical_root, ec)) {
    if (error)
      *error = "project root is unavailable";
    return false;
  }
  std::unique_ptr<LockState> lock;
  if (access == ProjectAccess::ReadWrite) {
    // Older (schema 1) projects may predate the .nexora directory. Create it only for a root that
    // actually holds a project descriptor, so opening a random directory leaves it untouched.
    if (std::filesystem::is_regular_file(canonical_root / "project.nexora", ec))
      std::filesystem::create_directories(canonical_root / ".nexora", ec);
    lock = LockState::Acquire(canonical_root / ".nexora/editor.lock", error);
    if (!lock)
      return false;
  }
  ProjectDescriptor project;
  bool legacy = false;
  if (!ReadProjectDescriptor(canonical_root / "project.nexora", project, legacy, error))
    return false;
  project.id = legacy ? DerivedProjectId(canonical_root, project.name) : project.id;
  auto upgrade = ProjectUpgradeState::Current;
  std::vector<std::string> documents;
  if (!ReadWorkspace(canonical_root / ".nexora/workspace", documents, error))
    return false;
  if (legacy) {
    if (access == ProjectAccess::ReadWrite) {
      project.schema_version = ProjectDescriptor::kSchemaVersion;
      if (!AtomicWrite(canonical_root / "project.nexora", ProjectContents(project), error))
        return false;
      upgrade = ProjectUpgradeState::Applied;
    } else {
      upgrade = ProjectUpgradeState::Required;
    }
  }
  auto write_time = std::filesystem::last_write_time(canonical_root / ".nexora/workspace", ec);
  if (ec)
    write_time = {};
  root_ = canonical_root;
  project_ = std::move(project);
  documents_ = std::move(documents);
  workspace_write_time_ = write_time;
  lock_ = std::move(lock);
  access_ = access;
  upgrade_state_ = upgrade;
  return true;
}

bool ProjectWorkspace::WriteWorkspace(std::span<const std::string> documents, std::string *error) {
  if (!EnsureWritable(access_, error))
    return false;
  if (!ValidateWorkspaceDocuments(documents, error))
    return false;
  std::string contents = "schema=1\n";
  for (const auto &document : documents)
    contents += "document=" + document + "\n";
  if (!AtomicWrite(root_ / ".nexora/workspace", contents, error))
    return false;
  std::error_code ec;
  workspace_write_time_ = std::filesystem::last_write_time(root_ / ".nexora/workspace", ec);
  if (ec) {
    if (error)
      *error = "workspace was written but its timestamp is unavailable: " + ec.message();
    return false;
  }
  documents_.assign(documents.begin(), documents.end());
  return true;
}

bool ProjectWorkspace::SaveWorkspace(std::span<const std::string> documents, std::string *error) {
  if (!EnsureWritable(access_, error))
    return false;
  if (!ValidateWorkspaceDocuments(documents, error))
    return false;
  std::string journal = "schema=1\n";
  for (const auto &document : documents)
    journal += "document=" + document + "\n";
  if (!AtomicWrite(root_ / ".nexora/workspace.recovery", journal, error))
    return false;
  if (!WriteWorkspace(documents, error))
    return false;
  std::error_code ec;
  std::filesystem::remove(root_ / ".nexora/workspace.recovery", ec);
  if (ec && error)
    *error = "workspace saved but recovery journal cleanup failed: " + ec.message();
  return !ec;
}

bool ProjectWorkspace::RecoverWorkspace(std::string *error) {
  if (!EnsureWritable(access_, error))
    return false;
  std::vector<std::string> recovered;
  if (!ReadWorkspace(root_ / ".nexora/workspace.recovery", recovered, error, "recovery journal",
                     false))
    return false;
  if (!WriteWorkspace(recovered, error))
    return false;
  std::error_code ec;
  std::filesystem::remove(root_ / ".nexora/workspace.recovery", ec);
  if (ec && error)
    *error = "workspace recovered but journal cleanup failed: " + ec.message();
  return !ec;
}

bool ProjectWorkspace::DiscardRecovery(std::string *error) {
  if (!EnsureWritable(access_, error))
    return false;
  std::error_code ec;
  const bool removed = std::filesystem::remove(root_ / ".nexora/workspace.recovery", ec);
  if (ec && error)
    *error = "could not discard recovery journal: " + ec.message();
  else if (!removed && error)
    *error = "recovery journal does not exist";
  return !ec && removed;
}

namespace {
bool GameplayLibraryPath(std::string_view value) {
  if (value.empty())
    return true;
  if (value.size() >= 1024 || !SafeLine(value) ||
      value.find_first_of("\\:") != std::string_view::npos)
    return false;
  const std::filesystem::path path(std::u8string(value.begin(), value.end()));
  if (path.is_absolute() || path.has_root_path())
    return false;
  for (const auto &part : path)
    if (part == ".." || part == "." || part.empty())
      return false;
  return true;
}
} // namespace

bool ProjectWorkspace::SaveGameplayLibrary(std::string_view relative_path, std::string *error) {
  if (error)
    error->clear();
  if (!EnsureWritable(access_, error))
    return false;
  if (root_.empty() || !GameplayLibraryPath(relative_path)) {
    if (error)
      *error = "gameplay library must be a relative UTF-8 file path shorter than 1024 bytes";
    return false;
  }
  return AtomicWrite(root_ / ".nexora/gameplay-library.ini",
                     "schema=1\nlibrary=" + std::string(relative_path) + "\n", error);
}

std::optional<std::string> ProjectWorkspace::LoadGameplayLibrary(std::string *error) const {
  if (error)
    error->clear();
  if (root_.empty())
    return std::nullopt;
  const auto settings_path = root_ / ".nexora/gameplay-library.ini";
  std::ifstream input(settings_path, std::ios::binary);
  if (!input) {
    std::error_code ec;
    const bool exists = std::filesystem::exists(settings_path, ec);
    if (error && (exists || ec))
      *error = "could not read gameplay library settings";
    return std::nullopt;
  }
  std::array<char, 1100> buffer{};
  input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  const auto count = static_cast<std::size_t>(input.gcount());
  const std::string_view text(buffer.data(), count);
  constexpr std::string_view prefix = "schema=1\nlibrary=";
  if (input.bad() || count == buffer.size() || !text.starts_with(prefix) || !text.ends_with('\n')) {
    if (error)
      *error = "invalid or unsupported gameplay library settings";
    return std::nullopt;
  }
  const auto library = text.substr(prefix.size(), text.size() - prefix.size() - 1);
  if (!GameplayLibraryPath(library)) {
    if (error)
      *error = "invalid gameplay library path in project settings";
    return std::nullopt;
  }
  return std::string(library);
}

bool ProjectWorkspace::ExportEditorFrameProcessing(std::span<const FrameSample> samples,
                                                   std::uint64_t dropped_frames,
                                                   std::string *error) {
  if (error)
    error->clear();
  if (!EnsureWritable(access_, error))
    return false;
  if (root_.empty() || HasRecoveryJournal() || samples.empty() || samples.size() > 600) {
    if (error)
      *error = "frame export requires 1-600 samples and a resolved project recovery journal";
    return false;
  }
  std::ostringstream csv;
  csv.imbue(std::locale::classic());
  csv << std::setprecision(std::numeric_limits<double>::max_digits10);
  csv << "frame,frame_processing_wall_ms,older_frames_dropped,gpu_ms,memory_bytes\n";
  if (!ValidateFrameProcessingSamples(samples, error))
    return false;
  for (const auto &sample : samples) {
    csv << sample.frame << ',' << sample.cpu_ms << ',' << dropped_frames << ",,\n";
  }
  return AtomicWrite(root_ / ".nexora/frame-processing.csv", csv.str(), error);
}

bool ProjectWorkspace::ExportEditorFrameProcessingJson(std::span<const FrameSample> samples,
                                                       std::uint64_t dropped_frames,
                                                       std::string *error) {
  if (error)
    error->clear();
  if (!EnsureWritable(access_, error))
    return false;
  if (root_.empty() || HasRecoveryJournal() || samples.empty() || samples.size() > 600) {
    if (error)
      *error = "frame export requires 1-600 samples and a resolved project recovery journal";
    return false;
  }
  if (!ValidateFrameProcessingSamples(samples, error))
    return false;
  std::ostringstream json;
  json.imbue(std::locale::classic());
  json << std::setprecision(std::numeric_limits<double>::max_digits10);
  json << "{\n  \"schema\": 1,\n  \"source\": \"NexoraEditor\",\n"
          "  \"metric\": \"editor_frame_processing_wall_ms\",\n"
          "  \"scope\": \"after_begin_frame_before_present\",\n"
          "  \"unit\": \"milliseconds\",\n  \"project_uuid\": \""
       << project_.id.ToString() << "\",\n  \"sample_count\": " << samples.size()
       << ",\n  \"older_frames_dropped\": \"" << dropped_frames
       << "\",\n  \"gpu_timing_available\": false,\n"
          "  \"memory_measurement_available\": false,\n  \"samples\": [\n";
  for (std::size_t index = 0; index < samples.size(); ++index) {
    const auto &sample = samples[index];
    json << "    {\"frame\": \"" << sample.frame
         << "\", \"frame_processing_wall_ms\": " << sample.cpu_ms
         << ", \"gpu_ms\": null, \"memory_bytes\": null}"
         << (index + 1 == samples.size() ? "\n" : ",\n");
  }
  json << "  ]\n}\n";
  return AtomicWrite(root_ / ".nexora/frame-processing.json", json.str(), error);
}

bool ProjectWorkspace::SaveEditorLayout(std::string_view layout, std::string *error) {
  if (error)
    error->clear();
  if (!EnsureWritable(access_, error))
    return false;
  if (root_.empty() || layout.empty() || layout.size() > kMaximumEditorLayoutBytes ||
      layout.find('\0') != std::string_view::npos) {
    if (error)
      *error = "editor layout is empty, invalid or exceeds the byte limit";
    return false;
  }
  return AtomicWrite(root_ / ".nexora/editor-layout.ini", "schema=1\n" + std::string(layout),
                     error);
}

std::optional<std::string> ProjectWorkspace::LoadEditorLayout(std::string *error) const {
  if (error)
    error->clear();
  const auto fail = [&](std::string_view reason) -> std::optional<std::string> {
    if (error)
      *error = reason;
    return std::nullopt;
  };
  const auto path = root_ / ".nexora/editor-layout.ini";
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec == std::errc::no_such_file_or_directory ||
      (!ec && status.type() == std::filesystem::file_type::not_found))
    return std::nullopt;
  if (ec || !std::filesystem::is_regular_file(status))
    return fail("editor layout is unavailable or unsafe");
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return fail("could not read editor layout");
  // Include the longest supported schema header (CRLF); never accumulate an unbounded line.
  constexpr auto maximum_file_bytes = kMaximumEditorLayoutBytes + 10;
  std::string bytes;
  std::array<char, 4096> buffer{};
  while (input) {
    const auto requested = std::min(buffer.size(), maximum_file_bytes - bytes.size() + 1);
    input.read(buffer.data(), static_cast<std::streamsize>(requested));
    const auto count = static_cast<std::size_t>(input.gcount());
    if (count > maximum_file_bytes - bytes.size())
      return fail("editor layout exceeds the byte limit");
    bytes.append(buffer.data(), count);
  }
  if (input.bad() || !input.eof())
    return fail("could not read editor layout");
  const auto newline = bytes.find('\n');
  if (newline == std::string::npos || newline > 9)
    return fail("invalid or unsupported editor layout");
  auto schema = bytes.substr(0, newline);
  StripCarriageReturn(schema);
  if (schema != "schema=0" && schema != "schema=1")
    return fail("invalid or unsupported editor layout");
  auto layout = bytes.substr(newline + 1);
  if (layout.empty() || layout.size() > kMaximumEditorLayoutBytes ||
      layout.find('\0') != std::string::npos)
    return fail("editor layout is empty, invalid or exceeds the byte limit");
  std::size_t output = 0;
  for (std::size_t position = 0; position < layout.size(); ++position) {
    if (layout[position] == '\r' && position + 1 < layout.size() && layout[position + 1] == '\n')
      continue;
    layout[output++] = layout[position];
  }
  layout.resize(output);
  return layout;
}

bool ProjectWorkspace::HasRecoveryJournal() const {
  if (root_.empty())
    return false;
  std::error_code ec;
  const auto path = root_ / ".nexora/workspace.recovery";
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec == std::errc::no_such_file_or_directory ||
      (!ec && status.type() == std::filesystem::file_type::not_found)) {
    // Windows may report a missing leaf when a metadata parent is a file. Verify the parent
    // before interpreting absence; valid directory aliases retain their existing behavior.
    const auto parent = std::filesystem::symlink_status(path.parent_path(), ec);
    if (ec == std::errc::no_such_file_or_directory ||
        (!ec && parent.type() == std::filesystem::file_type::not_found))
      return false;
    if (ec)
      return true;
    if (std::filesystem::is_symlink(parent)) {
      const auto target = std::filesystem::status(path.parent_path(), ec);
      return ec || !std::filesystem::is_directory(target);
    }
    return !std::filesystem::is_directory(parent);
  }
  // Occupied or uninspectable metadata requires an explicit recovery decision. Do not follow
  // aliases, or mistake a dangling link/directory for an absent journal and unlock authoring.
  return ec || status.type() != std::filesystem::file_type::not_found;
}

bool ProjectWorkspace::HasExternalChange() const {
  std::error_code ec;
  const auto current = std::filesystem::last_write_time(root_ / ".nexora/workspace", ec);
  return !ec && current != workspace_write_time_;
}

std::filesystem::path RecentProjectStore::DefaultPath() {
#if defined(_WIN32)
  if (const auto local = EnvironmentPath(L"LOCALAPPDATA"))
    return *local / "Nexora" / "Editor" / "recent-projects";
  if (const auto roaming = EnvironmentPath(L"APPDATA"))
    return *roaming / "Nexora" / "Editor" / "recent-projects";
#elif defined(__APPLE__)
  if (const auto home = EnvironmentPath("HOME"))
    return *home / "Library" / "Application Support" / "Nexora" / "Editor" / "recent-projects";
#else
  if (const auto config = EnvironmentPath("XDG_CONFIG_HOME"))
    return *config / "nexora" / "editor" / "recent-projects";
  if (const auto home = EnvironmentPath("HOME"))
    return *home / ".config" / "nexora" / "editor" / "recent-projects";
#endif
  return std::filesystem::temp_directory_path() / "nexora-editor-recent-projects";
}

bool RecentProjectStore::Open(const std::filesystem::path &path, std::string *error) {
  if (error)
    error->clear();
  if (path.empty()) {
    if (error)
      *error = "recent-project storage path is empty";
    return false;
  }
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec == std::errc::no_such_file_or_directory) {
    path_ = path;
    entries_.clear();
    return true;
  }
  if (ec || std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status) ||
      std::filesystem::file_size(path, ec) > 65536 || ec) {
    if (error)
      *error = "recent-project storage is unavailable, unsafe, or too large";
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  std::string line;
  if (!input || !std::getline(input, line)) {
    if (error)
      *error = "recent-project storage is unreadable";
    return false;
  }
  StripCarriageReturn(line);
  if (line != "schema=1") {
    if (error)
      *error = "recent-project storage uses an unsupported schema";
    return false;
  }
  std::vector<RecentProject> candidate;
  std::unordered_set<std::string> ids;
  std::unordered_set<std::string> roots;
  while (std::getline(input, line)) {
    StripCarriageReturn(line);
    std::istringstream record(line);
    std::string tag;
    std::string id_text;
    std::string root_text;
    std::string name;
    std::string extra;
    if (!(record >> tag >> id_text >> std::quoted(root_text) >> std::quoted(name)) ||
        record >> extra || tag != "entry" || !SafeLine(root_text) || !SafeLine(name)) {
      if (error)
        *error = "recent-project storage contains an invalid entry";
      return false;
    }
    const auto id = foundation::Uuid::Parse(id_text);
    const auto root = PathFromUtf8(root_text);
    if (!id || id.Value().IsNil() || !root || !root->is_absolute() || !ids.insert(id_text).second ||
        !roots.insert(PathUtf8(root->lexically_normal())).second) {
      if (error)
        *error = "recent-project storage contains a duplicate or invalid project";
      return false;
    }
    candidate.push_back({id.Value(), root->lexically_normal(), std::move(name)});
    if (candidate.size() > kMaximumEntries) {
      if (error)
        *error = "recent-project storage exceeds the entry limit";
      return false;
    }
  }
  if (!input.good() && !input.eof()) {
    if (error)
      *error = "recent-project storage could not be read";
    return false;
  }
  path_ = path;
  entries_ = std::move(candidate);
  return true;
}

bool RecentProjectStore::Record(const ProjectWorkspace &workspace, std::string *error) {
  if (path_.empty() || workspace.Root().empty() || workspace.Project().id.IsNil()) {
    if (error)
      *error = "recent-project store or project is not initialized";
    return false;
  }
  if (!SafeLine(PathUtf8(workspace.Root())) || !SafeLine(workspace.Project().name)) {
    if (error)
      *error = "recent project root or name exceeds the supported UTF-8 record limits";
    return false;
  }
  const auto previous = entries_;
  std::erase_if(entries_, [&](const RecentProject &entry) {
    return entry.id == workspace.Project().id || entry.root == workspace.Root();
  });
  entries_.insert(entries_.begin(),
                  {workspace.Project().id, workspace.Root(), workspace.Project().name});
  if (entries_.size() > kMaximumEntries)
    entries_.resize(kMaximumEntries);
  if (Save(error))
    return true;
  entries_ = previous;
  return false;
}

bool RecentProjectStore::Remove(foundation::Uuid id, std::string *error) {
  const auto previous = entries_;
  const auto removed =
      std::erase_if(entries_, [&](const RecentProject &entry) { return entry.id == id; });
  if (removed == 0) {
    if (error)
      *error = "recent project does not exist";
    return false;
  }
  if (Save(error))
    return true;
  entries_ = previous;
  return false;
}

bool RecentProjectStore::Save(std::string *error) {
  std::ostringstream contents;
  contents << "schema=1\n";
  for (const auto &entry : entries_)
    contents << "entry " << entry.id.ToString() << ' ' << std::quoted(PathUtf8(entry.root)) << ' '
             << std::quoted(entry.name) << '\n';
  return AtomicWrite(path_, contents.str(), error);
}
} // namespace nexora::editor
