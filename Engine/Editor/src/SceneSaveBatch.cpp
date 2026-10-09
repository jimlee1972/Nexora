#include "Nexora/Editor/SceneSaveBatch.h"
#include "AtomicFile.h"
#include "SceneSaveBatchTestAccess.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <limits>
#include <unordered_set>

namespace nexora::editor {
namespace {
constexpr std::size_t kFileLimit = SceneFileSession::kMaximumDiskBaselineBytes;
constexpr std::size_t kManifestLimit = 32 * 1024;
constexpr std::string_view kMagic = "NXSBATCH1";
enum class Phase : unsigned char { Preparing, Prepared, Committed, RolledBack };
struct Disk final {
  bool exists{};
  std::string bytes;
  bool operator==(const Disk &) const = default;
};
std::uint64_t Hash(std::string_view bytes) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char byte : bytes) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  return hash;
}
bool Absent(const std::filesystem::path &path) {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  return error == std::errc::no_such_file_or_directory ||
         (!error && status.type() == std::filesystem::file_type::not_found);
}
bool PlainFile(const std::filesystem::path &path) {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (error || !std::filesystem::is_regular_file(status))
    return false;
  return std::filesystem::hard_link_count(path, error) == 1 && !error;
}
std::optional<Disk> ReadDisk(const std::filesystem::path &path, std::size_t limit = kFileLimit) {
  if (Absent(path))
    return Disk{};
  if (!PlainFile(path))
    return std::nullopt;
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size > limit)
    return std::nullopt;
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return std::nullopt;
  Disk disk{true, std::string(static_cast<std::size_t>(size), '\0')};
  input.read(disk.bytes.data(), static_cast<std::streamsize>(disk.bytes.size()));
  if (input.bad() || static_cast<std::size_t>(input.gcount()) != disk.bytes.size() ||
      input.peek() != std::char_traits<char>::eof() || !input.eof())
    return std::nullopt;
  return disk;
}
std::filesystem::path Journal(const ProjectWorkspace &workspace) {
  return workspace.Root() / ".nexora/scene-save-all.recovery";
}
std::string PortableDestinationKey(const std::filesystem::path &path) {
  const auto utf8 = path.generic_u8string();
  std::string key(utf8.begin(), utf8.end());
  std::ranges::transform(key, key.begin(), [](unsigned char c) {
    return static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
  });
  return key;
}
std::optional<std::filesystem::path> SafeDestination(const std::filesystem::path &root,
                                                     const std::filesystem::path &relative) {
  const auto encoded = relative.generic_u8string();
  const std::string text(encoded.begin(), encoded.end());
  if (text.empty() || text.size() >= 1024 || !foundation::IsValidUtf8(text) ||
      relative.has_root_path() || relative.extension() != ".scene" ||
      text.find_first_of("\\:<>\"|?*") != std::string::npos ||
      std::ranges::any_of(text, [](unsigned char c) { return c < 32 || c == 127; }))
    return std::nullopt;
  auto parent = root;
  std::size_t index = 0;
  for (const auto &part : relative) {
    const auto bytes = part.generic_u8string();
    std::string name(bytes.begin(), bytes.end());
    if (part.empty() || part == "." || part == ".." || name.back() == '.' || name.back() == ' ')
      return std::nullopt;
    std::ranges::transform(name, name.begin(), [](unsigned char c) {
      return static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
    });
    if (index == 0 && name == ".nexora") {
      auto next = relative.begin();
      ++next;
      if (next == relative.end())
        return std::nullopt;
      const auto next_bytes = next->generic_u8string();
      std::string next_name(next_bytes.begin(), next_bytes.end());
      std::ranges::transform(next_name, next_name.begin(), [](unsigned char c) {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
      });
      if (next_name != "scenes")
        return std::nullopt;
    }
    const auto stem = name.substr(0, name.find('.'));
    if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" ||
        (stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) &&
         stem.back() >= '1' && stem.back() <= '9'))
      return std::nullopt;
    parent /= part;
    ++index;
    if (parent != root / relative) {
      std::error_code error;
      if (!std::filesystem::is_directory(std::filesystem::symlink_status(parent, error)) || error)
        return std::nullopt;
    }
  }
  return parent;
}
std::filesystem::path Sibling(const std::filesystem::path &destination, bool rollback = false) {
  auto result = destination;
  result += rollback ? ".save-all-rollback-tmp" : ".save-all-tmp";
  return result;
}
std::filesystem::path CopyPath(const std::filesystem::path &journal, std::size_t index,
                               bool before) {
  return journal / ((before ? "before-" : "after-") + std::to_string(index) + ".scene");
}
bool WriteNew(const std::filesystem::path &path, std::string_view bytes) {
  if (!Absent(path))
    return false;
  std::ofstream output(path, std::ios::binary);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!output || (output.close(), output.fail()))
    return false;
  const auto readback = ReadDisk(path);
  return readback && readback->exists && readback->bytes == bytes;
}
bool Replace(const std::filesystem::path &source, const std::filesystem::path &destination) {
#if defined(_WIN32)
  return MoveFileExW(source.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  std::error_code error;
  std::filesystem::rename(source, destination, error);
  return !error;
#endif
}
void Append(std::string &bytes, std::uint64_t value) {
  for (unsigned int shift = 0; shift != 64; shift += 8)
    bytes.push_back(static_cast<char>((value >> shift) & 255));
}
struct Record final {
  std::filesystem::path relative;
  bool before_exists{};
  std::uint64_t before_size{}, before_hash{}, after_size{}, after_hash{};
  bool operator==(const Record &) const = default;
};
struct Manifest final {
  Phase phase{};
  foundation::Uuid project;
  std::vector<Record> records;
};
std::string Encode(const Manifest &manifest) {
  std::string bytes(kMagic);
  Append(bytes, static_cast<std::uint64_t>(manifest.phase));
  Append(bytes, manifest.project.high);
  Append(bytes, manifest.project.low);
  Append(bytes, manifest.records.size());
  for (const auto &record : manifest.records) {
    const auto path = record.relative.generic_u8string();
    Append(bytes, path.size());
    bytes.append(path.begin(), path.end());
    Append(bytes, record.before_exists ? 1 : 0);
    Append(bytes, record.before_size);
    Append(bytes, record.before_hash);
    Append(bytes, record.after_size);
    Append(bytes, record.after_hash);
  }
  Append(bytes, Hash(bytes));
  return bytes;
}
class Reader final {
public:
  explicit Reader(std::string_view bytes) : bytes_(bytes) {}
  std::optional<std::uint64_t> Number() {
    if (bytes_.size() - at_ < 8)
      return std::nullopt;
    std::uint64_t value = 0;
    for (unsigned int shift = 0; shift != 64; shift += 8)
      value |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes_[at_++])) << shift;
    return value;
  }
  std::optional<std::string_view> Text() {
    const auto size = Number();
    if (!size || *size == 0 || *size >= 1024 || *size > bytes_.size() - at_)
      return std::nullopt;
    const auto text = bytes_.substr(at_, static_cast<std::size_t>(*size));
    at_ += static_cast<std::size_t>(*size);
    return text;
  }
  bool Done() const { return at_ == bytes_.size(); }

private:
  std::string_view bytes_;
  std::size_t at_{};
};
std::optional<Manifest> Decode(const ProjectWorkspace &workspace, std::string_view bytes) {
  if (bytes.size() > kManifestLimit || bytes.size() < kMagic.size() + 40 ||
      !bytes.starts_with(kMagic))
    return std::nullopt;
  Reader tail(bytes.substr(bytes.size() - 8));
  const auto checksum = tail.Number();
  if (!checksum || *checksum != Hash(bytes.substr(0, bytes.size() - 8)))
    return std::nullopt;
  Reader reader(bytes.substr(kMagic.size(), bytes.size() - kMagic.size() - 8));
  const auto phase = reader.Number(), high = reader.Number(), low = reader.Number();
  const auto count = reader.Number();
  if (!phase || *phase > 3 || !high || !low || !count || *count == 0 ||
      *count > SceneSaveBatch::kMaximumDocuments ||
      foundation::Uuid{*high, *low} != workspace.Project().id)
    return std::nullopt;
  Manifest manifest{static_cast<Phase>(*phase), {*high, *low}, {}};
  std::unordered_set<std::string> paths;
  std::uint64_t budget = 0;
  for (std::uint64_t i = 0; i < *count; ++i) {
    const auto text = reader.Text();
    const auto exists = reader.Number(), before_size = reader.Number();
    const auto before_hash = reader.Number(), after_size = reader.Number();
    const auto after_hash = reader.Number();
    if (!text || !exists || *exists > 1 || !before_size || *before_size > kFileLimit ||
        !before_hash || !after_size || *after_size == 0 || *after_size > kFileLimit ||
        !after_hash || (!*exists && (*before_size != 0 || *before_hash != Hash(""))) ||
        !foundation::IsValidUtf8(*text))
      return std::nullopt;
    const auto relative = std::filesystem::path(std::u8string(text->begin(), text->end()));
    if (!paths.insert(PortableDestinationKey(relative)).second ||
        !SafeDestination(workspace.Root(), relative) ||
        *before_size > SceneSaveBatch::kMaximumPayloadBytes - budget)
      return std::nullopt;
    budget += *before_size;
    if (*after_size > SceneSaveBatch::kMaximumPayloadBytes - budget)
      return std::nullopt;
    budget += *after_size;
    manifest.records.push_back(
        {relative, *exists != 0, *before_size, *before_hash, *after_size, *after_hash});
  }
  return reader.Done() ? std::optional<Manifest>(std::move(manifest)) : std::nullopt;
}
bool Matches(const Disk &disk, bool exists, std::uint64_t size, std::uint64_t hash) {
  return disk.exists == exists && disk.bytes.size() == size && Hash(disk.bytes) == hash;
}
void Error(std::string *error, std::string message) {
  if (error)
    *error = std::move(message);
}
bool PlainDirectory(const std::filesystem::path &path) {
  std::error_code error;
  return std::filesystem::is_directory(std::filesystem::symlink_status(path, error)) && !error;
}
bool RemoveFile(const std::filesystem::path &path) {
  std::error_code error;
  return std::filesystem::remove(path, error) && !error;
}
bool RemoveCopy(const std::filesystem::path &path, std::uint64_t size, std::uint64_t hash,
                bool preparing) {
  if (Absent(path))
    return true;
  const auto copy = ReadDisk(path);
  if (!copy || !copy->exists ||
      (preparing ? copy->bytes.size() > size : !Matches(*copy, true, size, hash)))
    return false;
  return RemoveFile(path);
}
bool ClearManifestTemporary(const ProjectWorkspace &workspace, const Manifest &manifest) {
  const auto temporary = Journal(workspace) / "manifest.tmp";
  if (Absent(temporary))
    return true;
  const auto bytes = ReadDisk(temporary, kManifestLimit);
  const auto decoded = bytes ? Decode(workspace, bytes->bytes) : std::nullopt;
  return decoded && decoded->records == manifest.records && RemoveFile(temporary);
}
bool KnownJournalEntries(const std::filesystem::path &journal, std::size_t count) {
  std::unordered_set<std::string> known{"manifest", "manifest.tmp"};
  for (std::size_t i = 0; i < count; ++i) {
    known.insert(CopyPath(journal, i, true).filename().string());
    known.insert(CopyPath(journal, i, false).filename().string());
  }
  std::error_code error;
  for (std::filesystem::directory_iterator at(journal, error), end; !error && at != end;
       at.increment(error))
    if (!known.contains(at->path().filename().string()))
      return false;
  return !error;
}
bool Cleanup(const ProjectWorkspace &workspace, const Manifest &manifest) {
  const auto journal = Journal(workspace);
  if (!KnownJournalEntries(journal, manifest.records.size()))
    return false;
  if (!ClearManifestTemporary(workspace, manifest))
    return false;
  const bool preparing = manifest.phase == Phase::Preparing;
  for (std::size_t i = 0; i < manifest.records.size(); ++i) {
    const auto &record = manifest.records[i];
    const auto destination = SafeDestination(workspace.Root(), record.relative);
    if (!destination ||
        !RemoveCopy(Sibling(*destination), record.after_size, record.after_hash, preparing) ||
        !RemoveCopy(Sibling(*destination, true), record.before_size, record.before_hash,
                    preparing) ||
        !RemoveCopy(CopyPath(journal, i, true), record.before_size, record.before_hash,
                    preparing) ||
        !RemoveCopy(CopyPath(journal, i, false), record.after_size, record.after_hash, preparing))
      return false;
  }
  return RemoveFile(journal / "manifest") && RemoveFile(journal);
}
} // namespace

struct SceneSaveBatch::State final {
  struct Entry final {
    SceneFileSession *session;
    SceneFileToken token;
    std::filesystem::path relative;
    SceneDocument::PreparedSave prepared;
    Disk before;
    SceneFileSession::DiskSnapshot next_baseline;
  };
  const ProjectWorkspace &workspace;
  std::filesystem::path root;
  foundation::Uuid project;
  std::vector<Entry> entries;
  std::function<void(std::size_t)> before_publish;
  explicit State(const ProjectWorkspace &owner)
      : workspace(owner), root(owner.Root()), project(owner.Project().id) {}
};
SceneSaveBatch::SceneSaveBatch(const ProjectWorkspace &workspace)
    : state_(std::make_unique<State>(workspace)) {}
SceneSaveBatch::~SceneSaveBatch() = default;
void SceneSaveBatch::Cancel() noexcept { state_->entries.clear(); }
bool SceneSaveBatch::HasRecovery(const ProjectWorkspace &workspace) {
  return !workspace.Root().empty() && !Absent(Journal(workspace));
}
bool SceneSaveBatch::Prepare(std::span<SceneFileSession *const> sessions, std::string *error) {
  Cancel();
  auto &state = *state_;
  const auto fail = [&](std::string message) {
    Error(error, std::move(message));
    return false;
  };
  if (state.root.empty() || state.workspace.Root() != state.root ||
      state.workspace.Project().id != state.project || !state.workspace.Writable() ||
      state.workspace.HasRecoveryJournal() || state.workspace.HasExternalChange() ||
      HasRecovery(state.workspace) || sessions.empty() || sessions.size() > kMaximumDocuments)
    return fail(
        "Save All requires a current writable project, named scenes and no pending recovery.");
  if (!PlainDirectory(state.root) || !PlainDirectory(state.root / ".nexora"))
    return fail("Save All metadata parents must be ordinary project directories.");
  std::vector<State::Entry> entries;
  entries.reserve(sessions.size());
  std::unordered_set<const SceneDocument *> documents;
  std::unordered_set<std::string> destination_keys;
  std::vector<std::filesystem::path> destinations;
  std::size_t budget = 0;
  for (auto *session : sessions) {
    if (!session || &session->workspace_ != &state.workspace || !session->Live(session->Token()) ||
        session->SaveBlocked() || !session->current_ || !session->disk_baseline_ ||
        !documents.insert(&session->document_).second)
      return fail("Each Save All scene needs one current, writable named document.");
    const auto destination = SafeDestination(state.root, *session->current_);
    if (!destination || !destination_keys.insert(PortableDestinationKey(*destination)).second ||
        !Absent(Sibling(*destination)) || !Absent(Sibling(*destination, true)))
      return fail("A Save All destination is unsafe or its sibling staging path is occupied.");
    for (const auto &previous : destinations) {
      std::error_code ec;
      if (previous == *destination || std::filesystem::equivalent(previous, *destination, ec))
        return fail("Save All destinations must be distinct ordinary files.");
    }
    auto before = ReadDisk(*destination);
    if (!before || before->exists != session->disk_baseline_->exists ||
        before->bytes != session->disk_baseline_->bytes)
      return fail("A scene changed outside the Editor. Review that file before Save All.");
    auto prepared = session->document_.PrepareSave();
    if (!prepared || prepared->Bytes().size() > kFileLimit ||
        before->bytes.size() > kMaximumPayloadBytes - budget)
      return fail("Save All preparation exceeds its document or aggregate byte budget.");
    budget += before->bytes.size();
    if (prepared->Bytes().size() > kMaximumPayloadBytes - budget)
      return fail("Save All preparation exceeds its aggregate byte budget.");
    budget += prepared->Bytes().size();
    SceneFileSession::DiskSnapshot next{true, prepared->Bytes()};
    entries.push_back({session, session->Token(), *session->current_, std::move(*prepared),
                       std::move(*before), std::move(next)});
    destinations.push_back(*destination);
  }
  state.entries = std::move(entries);
  if (error)
    error->clear();
  return true;
}
bool SceneSaveBatch::Recover(const ProjectWorkspace &workspace, std::string *error) {
  const auto fail = [&](std::string message) {
    Error(error, std::move(message));
    return false;
  };
  const auto journal = Journal(workspace);
  if (!workspace.Writable() || workspace.Root().empty() || !PlainDirectory(workspace.Root()) ||
      !PlainDirectory(journal.parent_path()) || !PlainDirectory(journal) ||
      !Absent(workspace.Root() / ".nexora/workspace.recovery"))
    return fail("Save All recovery needs the original writer project and an ordinary journal.");
  const auto encoded = ReadDisk(journal / "manifest", kManifestLimit);
  const auto manifest =
      encoded && encoded->exists ? Decode(workspace, encoded->bytes) : std::nullopt;
  if (!manifest)
    return fail(
        "Save All recovery manifest is incomplete or invalid. Retained data was preserved.");
  if (!KnownJournalEntries(journal, manifest->records.size()))
    return fail("Save All recovery has unknown retained entries. No source was replaced.");
  if (manifest->phase == Phase::Preparing) {
    // This phase never authorizes source replacement. Partial copies are owned preparation data;
    // cleanup checks regular files, bounds and known entries and touches no canonical source.
    if (!Cleanup(workspace, *manifest))
      return fail("Save All preparation cleanup is blocked. Inspect the retained staging paths.");
    if (error)
      error->clear();
    return true;
  }
  if (manifest->phase == Phase::Committed || manifest->phase == Phase::RolledBack) {
    for (const auto &record : manifest->records) {
      const auto destination = SafeDestination(workspace.Root(), record.relative);
      const auto current = destination ? ReadDisk(*destination) : std::nullopt;
      const bool committed = manifest->phase == Phase::Committed;
      if (!current || !Matches(*current, committed || record.before_exists,
                               committed ? record.after_size : record.before_size,
                               committed ? record.after_hash : record.before_hash))
        return fail("A completed Save All destination changed. Recovery data was preserved.");
    }
    if (!Cleanup(workspace, *manifest))
      return fail("Save All files are consistent, but retained-data cleanup is blocked.");
    if (error)
      error->clear();
    return true;
  }
  std::vector<std::pair<Disk, Disk>> copies;
  copies.reserve(manifest->records.size());
  for (std::size_t i = 0; i < manifest->records.size(); ++i) {
    const auto &record = manifest->records[i];
    auto before = ReadDisk(CopyPath(journal, i, true));
    auto after = ReadDisk(CopyPath(journal, i, false));
    const auto destination = SafeDestination(workspace.Root(), record.relative);
    const auto current = destination ? ReadDisk(*destination) : std::nullopt;
    if (!before || !after || !current ||
        !Matches(*before, record.before_exists, record.before_size, record.before_hash) ||
        !Matches(*after, true, record.after_size, record.after_hash) ||
        (*current != *before && *current != *after))
      return fail("Save All recovery data or a destination changed. No source was replaced.");
    copies.emplace_back(std::move(*before), std::move(*after));
  }
  // A failed atomic manifest replacement may retain a complete matching temporary manifest.
  // Validate it before source restoration, then release only that owned temporary destination.
  if (!ClearManifestTemporary(workspace, *manifest))
    return fail("Save All temporary recovery metadata is invalid. No source was replaced.");
  if (manifest->phase == Phase::Prepared) {
    for (std::size_t i = 0; i < manifest->records.size(); ++i) {
      const auto destination = SafeDestination(workspace.Root(), manifest->records[i].relative);
      const auto current = destination ? ReadDisk(*destination) : std::nullopt;
      if (!current || (*current != copies[i].first && *current != copies[i].second))
        return fail("A scene changed during Save All recovery. Remaining originals are retained.");
      if (*current == copies[i].first)
        continue;
      if (!copies[i].first.exists) {
        if (!RemoveFile(*destination))
          return fail("Could not remove a newly published scene during recovery.");
      } else {
        const auto temporary = Sibling(*destination, true);
        const auto retained = ReadDisk(temporary);
        if (!retained || (retained->exists && *retained != copies[i].first) ||
            (!retained->exists && !WriteNew(temporary, copies[i].first.bytes)) ||
            !Replace(temporary, *destination))
          return fail("Could not restore a scene. Original recovery copies remain available.");
      }
    }
  }
  auto restored = *manifest;
  restored.phase = Phase::RolledBack;
  if (!detail::AtomicWrite(journal / "manifest", Encode(restored), error))
    return false;
  if (!Cleanup(workspace, restored))
    return fail("All scene files are consistent, but recovery cleanup is blocked.");
  if (error)
    error->clear();
  return true;
}
SceneSaveBatchResult SceneSaveBatch::Publish() {
  auto &state = *state_;
  if (state.entries.empty())
    return {SceneSaveBatchStatus::Rejected, "No prepared Save All batch."};
  const auto current = [&] {
    if (state.workspace.Root() != state.root || state.workspace.Project().id != state.project ||
        !state.workspace.Writable() || state.workspace.HasExternalChange() ||
        !Absent(state.root / ".nexora/workspace.recovery"))
      return false;
    for (const auto &entry : state.entries)
      if (!entry.session->Live(entry.token) || entry.session->SaveBlocked() ||
          entry.session->current_ != entry.relative ||
          !entry.session->document_.MatchesPreparedSave(entry.prepared))
        return false;
    return true;
  };
  const auto rejected = [&](std::string message) {
    Cancel();
    return SceneSaveBatchResult{SceneSaveBatchStatus::Rejected, std::move(message)};
  };
  if (!current() || state.workspace.HasRecoveryJournal() || HasRecovery(state.workspace))
    return rejected("Save All input changed or recovery became pending. Prepare again.");
  Manifest manifest{Phase::Preparing, state.project, {}};
  for (const auto &entry : state.entries) {
    const auto destination = SafeDestination(state.root, entry.relative);
    const auto disk = destination ? ReadDisk(*destination) : std::nullopt;
    if (!disk || *disk != entry.before || !Absent(Sibling(*destination)) ||
        !Absent(Sibling(*destination, true)))
      return rejected("A Save All destination or staging path changed before publication.");
    manifest.records.push_back({entry.relative, entry.before.exists, entry.before.bytes.size(),
                                Hash(entry.before.bytes), entry.prepared.Bytes().size(),
                                Hash(entry.prepared.Bytes())});
  }
  const auto journal = Journal(state.workspace);
  std::error_code error;
  if (!PlainDirectory(journal.parent_path()) ||
      !std::filesystem::create_directory(journal, error) || error)
    return rejected("Save All recovery destination is unavailable or occupied.");
  const auto failed = [&](std::string message) {
    std::string recovery_error;
    const bool recovered = Recover(state.workspace, &recovery_error);
    Cancel();
    if (!recovered)
      return SceneSaveBatchResult{SceneSaveBatchStatus::RecoveryRequired,
                                  std::move(message) + " Original recovery data is retained. " +
                                      recovery_error};
    return SceneSaveBatchResult{SceneSaveBatchStatus::Rejected, std::move(message)};
  };
  if (!detail::AtomicWrite(journal / "manifest", Encode(manifest), nullptr))
    return failed("Could not write Save All recovery metadata.");
  for (std::size_t i = 0; i < state.entries.size(); ++i) {
    const auto &entry = state.entries[i];
    const auto destination = SafeDestination(state.root, entry.relative);
    if (!destination ||
        (entry.before.exists && !WriteNew(CopyPath(journal, i, true), entry.before.bytes)) ||
        !WriteNew(CopyPath(journal, i, false), entry.prepared.Bytes()) ||
        !WriteNew(Sibling(*destination), entry.prepared.Bytes()))
      return failed("Could not stage every Save All output and original.");
  }
  if (!current())
    return failed("A prepared document changed during Save All staging.");
  manifest.phase = Phase::Prepared;
  if (!detail::AtomicWrite(journal / "manifest", Encode(manifest), nullptr))
    return failed("Could not authorize Save All publication.");
  for (std::size_t i = 0; i < state.entries.size(); ++i) {
    if (state.before_publish) {
      try {
        state.before_publish(i);
      } catch (...) {
        return failed("Save All publication was interrupted; restoring the original batch.");
      }
    }
    const auto &entry = state.entries[i];
    const auto destination = SafeDestination(state.root, entry.relative);
    const auto disk = destination ? ReadDisk(*destination) : std::nullopt;
    const auto staged = destination ? ReadDisk(Sibling(*destination)) : std::nullopt;
    if (!current() || !disk || *disk != entry.before || !staged || !staged->exists ||
        staged->bytes != entry.prepared.Bytes() || !Replace(Sibling(*destination), *destination))
      return failed("Save All publication failed; restoring the original batch.");
  }
  for (const auto &entry : state.entries) {
    const auto destination = SafeDestination(state.root, entry.relative);
    const auto disk = destination ? ReadDisk(*destination) : std::nullopt;
    if (!current() || !disk || !disk->exists || disk->bytes != entry.prepared.Bytes())
      return failed("A published Save All output changed before commit.");
  }
  manifest.phase = Phase::Committed;
  if (!detail::AtomicWrite(journal / "manifest", Encode(manifest), nullptr))
    return failed("Could not commit the complete Save All recovery record.");
  // All owning baseline storage was allocated before any publication. Moves preserve document
  // identity, selection, Undo/Redo and opaque state while acknowledging the complete committed
  // batch.
  for (auto &entry : state.entries) {
    auto &document = entry.session->document_;
    document.saved_signature_ = std::move(entry.prepared.signature_);
    document.saved_opaque_records_ = std::move(entry.prepared.opaque_records_);
    document.opaque_dirty_ = false;
    entry.session->disk_baseline_ = std::move(entry.next_baseline);
    entry.session->pending_overwrite_.reset();
  }
  const bool cleaned = Cleanup(state.workspace, manifest);
  Cancel();
  return {cleaned ? SceneSaveBatchStatus::Published
                  : SceneSaveBatchStatus::PublishedRecoveryRequired,
          cleaned ? "All scene files saved."
                  : "All scene files saved; retained recovery cleanup needs attention."};
}

void SceneSaveBatchTestAccess::BeforePublish(SceneSaveBatch &batch,
                                             std::function<void(std::size_t)> hook) {
  batch.state_->before_publish = std::move(hook);
}
} // namespace nexora::editor
