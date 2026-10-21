#include "SceneMergeStorage.h"
#include "Nexora/Cryptography/Signature.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <unordered_set>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif
namespace nexora::editor::merge_storage {
namespace {
constexpr auto Limit = SceneComparison::kMaximumSourceBytes;
constexpr std::string_view Magic = "NXMERGE1";
std::string Text(const Path &path) {
  const auto u = path.generic_u8string();
  return {u.begin(), u.end()};
}
bool NotReparse(const Path &path) {
#if defined(_WIN32)
  const auto attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#else
  static_cast<void>(path);
  return true;
#endif
}
void Number(std::string &out, std::uint64_t n) {
  for (unsigned i = 0; i < 8; ++i)
    out.push_back(static_cast<char>((n >> (8 * i)) & 255));
}
void String(std::string &out, std::string_view value) {
  Number(out, value.size());
  out.append(value);
}
struct Reader {
  std::string_view data;
  bool good{true};
  std::uint64_t Number() {
    if (data.size() < 8) {
      good = false;
      return 0;
    }
    std::uint64_t n{};
    for (unsigned i = 0; i < 8; ++i)
      n |= static_cast<std::uint64_t>(static_cast<unsigned char>(data[i])) << (8 * i);
    data.remove_prefix(8);
    return n;
  }
  std::string String(std::size_t limit) {
    const auto n = Number();
    if (!good || n > limit || n > data.size()) {
      good = false;
      return {};
    }
    std::string result(data.substr(0, static_cast<std::size_t>(n)));
    data.remove_prefix(static_cast<std::size_t>(n));
    return result;
  }
};
bool Error(std::string *error, const char *message) {
  if (error)
    *error = message;
  return false;
}
std::array<std::optional<std::string_view>, 4> Sources(const SceneMergeArchive &a) {
  return {a.base ? std::optional<std::string_view>(*a.base) : std::nullopt, a.local, a.disk,
          a.candidate};
}
constexpr std::array<const char *, 4> Names{"base.scene", "local.scene", "disk.scene",
                                            "candidate.scene"};
} // namespace
bool Missing(const Path &path) {
  std::error_code e;
  const auto s = std::filesystem::symlink_status(path, e);
  return e == std::errc::no_such_file_or_directory ||
         (!e && s.type() == std::filesystem::file_type::not_found);
}
bool PlainDirectory(const Path &path) {
  std::error_code e;
  return std::filesystem::is_directory(std::filesystem::symlink_status(path, e)) && !e &&
         NotReparse(path);
}
bool PlainFile(const Path &path) {
  std::error_code e;
  if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path, e)) || e ||
      !NotReparse(path))
    return false;
  return std::filesystem::hard_link_count(path, e) == 1 && !e;
}
bool SafeRelativeScene(const Path &path) {
  const auto text = Text(path);
  if (text.empty() || text.size() >= 1024 || !foundation::IsValidUtf8(text) ||
      path.has_root_path() || path.extension() != ".scene" ||
      text.find_first_of("\\:<>\"|?*") != std::string::npos ||
      std::ranges::any_of(text, [](unsigned char c) { return c < 32 || c == 127; }))
    return false;
#if defined(_WIN32)
  // Reject DOS short-name spellings rather than treating an alias as a separate reviewed path.
  if (text.find('~') != std::string::npos)
    return false;
#endif
  for (const auto &p : path) {
    auto name = Text(p);
    if (name.empty() || name == "." || name == ".." || name.back() == '.' || name.back() == ' ')
      return false;
    std::ranges::transform(name, name.begin(), [](unsigned char c) {
      return static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
    });
    const auto stem = name.substr(0, name.find('.'));
    if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" ||
        (stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) &&
         stem.back() >= '1' && stem.back() <= '9'))
      return false;
  }
  return true;
}
bool SafeParents(const Path &root, const Path &relative) {
  if (!PlainDirectory(root) || relative.empty() || relative.has_root_path())
    return false;
  auto parent = root;
  for (const auto &part : relative.parent_path()) {
    if (part.empty() || part == "." || part == "..")
      return false;
    parent /= part;
    if (!PlainDirectory(parent))
      return false;
  }
  return true;
}
std::optional<std::string> Read(const Path &path, std::size_t limit) {
  if (!PlainFile(path))
    return {};
  std::error_code e;
  const auto size = std::filesystem::file_size(path, e);
  if (e || size > limit)
    return {};
  std::ifstream in(path, std::ios::binary);
  std::string bytes(static_cast<std::size_t>(size), '\0');
  in.read(bytes.data(), static_cast<std::streamsize>(size));
  if (!in || static_cast<std::size_t>(in.gcount()) != size ||
      in.peek() != std::char_traits<char>::eof() || !in.eof() || !PlainFile(path))
    return {};
  return bytes;
}
bool WriteNew(const Path &path, std::string_view bytes, const core::CancellationToken &cancel) {
#if defined(_WIN32)
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;
#else
  const int file = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (file < 0)
    return false;
#endif
  bool ok = true;
  while (!bytes.empty() && ok && !cancel.IsCancellationRequested()) {
    const auto count = std::min<std::size_t>(bytes.size(), 65536);
#if defined(_WIN32)
    DWORD wrote{};
    ok = WriteFile(file, bytes.data(), static_cast<DWORD>(count), &wrote, nullptr) && wrote != 0;
    if (ok)
      bytes.remove_prefix(wrote);
#else
    const auto wrote = ::write(file, bytes.data(), count);
    if (wrote < 0 && errno == EINTR)
      continue;
    ok = wrote > 0;
    if (ok)
      bytes.remove_prefix(static_cast<std::size_t>(wrote));
#endif
  }
  ok = ok && bytes.empty() && !cancel.IsCancellationRequested();
#if defined(_WIN32)
  if (ok)
    ok = FlushFileBuffers(file) != 0;
  if (!CloseHandle(file))
    ok = false;
#else
  if (ok)
    ok = ::fsync(file) == 0;
  if (::close(file) != 0)
    ok = false;
#endif
  return ok && PlainFile(path);
}
bool SyncDirectory(const Path &path) noexcept {
#if defined(_WIN32)
  // MoveFileExW uses WRITE_THROUGH; Windows does not expose portable directory fsync.
  static_cast<void>(path);
  return true;
#else
  const int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0)
    return false;
  const bool ok = ::fsync(fd) == 0;
  const bool closed = ::close(fd) == 0;
  return ok && closed;
#endif
}
bool Replace(const Path &from, const Path &to) noexcept {
#if defined(_WIN32)
  return MoveFileExW(from.c_str(), to.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  return ::rename(from.c_str(), to.c_str()) == 0;
#endif
}
std::optional<std::string> Digest(std::string_view bytes) {
  const auto hash = cryptography::Sha256(std::as_bytes(std::span(bytes.data(), bytes.size())));
  if (!hash)
    return {};
  constexpr char hex[] = "0123456789abcdef";
  std::string out;
  out.reserve(64);
  for (auto b : *hash) {
    const auto n = std::to_integer<unsigned char>(b);
    out.push_back(hex[n >> 4]);
    out.push_back(hex[n & 15]);
  }
  return out;
}
std::optional<std::string> Encode(const SceneMergeArchive &a, std::string *error) {
  if (a.schema != 1 || a.project.IsNil() || !SafeRelativeScene(a.source) ||
      a.choices.size() > SceneComparison::kMaximumRows) {
    Error(error, "Invalid merge archive identity or schema");
    return {};
  }
  std::string out(Magic);
  Number(out, a.schema);
  Number(out, a.project.high);
  Number(out, a.project.low);
  String(out, Text(a.source));
  for (const auto &bytes : Sources(a)) {
    Number(out, bytes.has_value());
    Number(out, bytes ? bytes->size() : 0);
    if (bytes && (bytes->empty() || bytes->size() > Limit)) {
      Error(error, "Archive source exceeds budget");
      return {};
    }
    auto hash = Digest(bytes.value_or(""));
    if (!hash) {
      Error(error, "SHA256 backend unavailable");
      return {};
    }
    String(out, *hash);
  }
  Number(out, a.choices.size());
  std::unordered_set<std::string> paths;
  for (const auto &choice : a.choices) {
    if (choice.stable_path.empty() || !foundation::IsValidUtf8(choice.stable_path) ||
        !paths.insert(choice.stable_path).second ||
        (choice.choice != SceneMergeChoice::Local && choice.choice != SceneMergeChoice::Remote)) {
      Error(error, "Invalid archive choices");
      return {};
    }
    String(out, choice.stable_path);
    Number(out, static_cast<std::uint8_t>(choice.choice));
    if (out.size() > SceneMergePublisher::kMaximumManifestBytes - 64) {
      Error(error, "Archive manifest exceeds budget");
      return {};
    }
  }
  auto hash = Digest(out);
  if (!hash) {
    Error(error, "SHA256 backend unavailable");
    return {};
  }
  out += *hash;
  return out;
}
// Decode validates structure/integrity; source payloads are loaded and hashed by ReadArchive.
std::optional<SceneMergeArchive> Decode(std::string_view bytes, std::string *error) {
  if (bytes.size() < Magic.size() + 64 ||
      bytes.size() > SceneMergePublisher::kMaximumManifestBytes || !bytes.starts_with(Magic)) {
    Error(error, "Invalid archive manifest");
    return {};
  }
  const auto hash = Digest(bytes.substr(0, bytes.size() - 64));
  if (!hash || *hash != bytes.substr(bytes.size() - 64)) {
    Error(error, "Archive manifest checksum mismatch");
    return {};
  }
  Reader r{bytes.substr(Magic.size(), bytes.size() - Magic.size() - 64)};
  if (r.Number() != 1) {
    Error(error, "Unsupported archive schema");
    return {};
  }
  SceneMergeArchive a;
  a.project = {r.Number(), r.Number()};
  const auto path = r.String(1023);
  if (!foundation::IsValidUtf8(path)) {
    Error(error, "Invalid archive UTF-8 path");
    return {};
  }
  a.source = Path(std::u8string(path.begin(), path.end()));
  if (!r.good || a.project.IsNil() || !SafeRelativeScene(a.source)) {
    Error(error, "Invalid archive provenance");
    return {};
  }
  for (std::size_t i = 0; i < 4; ++i) {
    const auto present = r.Number();
    const auto length = r.Number();
    const auto digest = r.String(64);
    if (!r.good || present > 1 || (i && !present) || length > Limit || (!present && length) ||
        (present && !length) || digest.size() != 64 || !std::ranges::all_of(digest, [](char c) {
          return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        })) {
      Error(error, "Invalid archive source record");
      return {};
    }
    if (i == 0 && present)
      a.base.emplace();
  }
  const auto count = r.Number();
  if (!r.good || count > SceneComparison::kMaximumRows) {
    Error(error, "Archive choice budget exceeded");
    return {};
  }
  std::unordered_set<std::string> paths;
  for (std::uint64_t i = 0; i < count; ++i) {
    auto path_value = r.String(SceneMergePublisher::kMaximumManifestBytes);
    const auto side = r.Number();
    if (!r.good || path_value.empty() || !foundation::IsValidUtf8(path_value) ||
        !paths.insert(path_value).second || side > 1) {
      Error(error, "Invalid archive choice");
      return {};
    }
    a.choices.push_back({std::move(path_value), static_cast<SceneMergeChoice>(side)});
  }
  if (!r.good || !r.data.empty()) {
    Error(error, "Truncated or trailing archive data");
    return {};
  }
  return a;
}
std::optional<SceneMergeArchive> ReadArchive(const Path &root, foundation::Uuid project,
                                             const Path &relative, std::string *error) {
  auto fail = [&](const char *message) -> std::optional<SceneMergeArchive> {
    Error(error, message);
    return {};
  };
  if (relative.parent_path() != Path(".nexora/scene-merge-history") ||
      Text(relative.filename()).size() != 64 || !SafeParents(root, relative) ||
      !PlainDirectory(root / relative))
    return fail("Unsafe archive path");
  const auto directory = root / relative;
  const auto manifest =
      Read(directory / "manifest.bin", SceneMergePublisher::kMaximumManifestBytes);
  if (!manifest || Digest(*manifest) != Text(relative.filename()))
    return fail("Archive identity mismatch");
  auto a = Decode(*manifest, error);
  if (!a || a->project != project)
    return fail("Archive project or manifest mismatch");
  std::array<std::string *, 4> outputs{a->base ? &*a->base : nullptr, &a->local, &a->disk,
                                       &a->candidate};
  for (std::size_t i = 0; i < 4; ++i) {
    if (!outputs[i]) {
      if (!Missing(directory / Names[i]))
        return fail("Unexpected absent-source payload");
      continue;
    }
    auto bytes = Read(directory / Names[i], Limit);
    if (!bytes)
      return fail("Missing or unsafe archive payload");
    *outputs[i] = std::move(*bytes);
  }
  const auto encoded = Encode(*a, error);
  if (!encoded || *encoded != *manifest)
    return fail("Archive payload integrity mismatch");
  auto review = SceneMerge::Prepare(
      a->base ? std::optional<std::string_view>(*a->base) : std::nullopt, a->local, a->disk, error);
  if (!review)
    return {};
  SceneMergeRequest request{1, review->Identity(), {}};
  for (const auto &choice : a->choices) {
    const auto &rows = review->Comparison().rows;
    const auto found =
        std::ranges::find(rows, choice.stable_path, &SceneComparisonRow::stable_path);
    if (found == rows.end())
      return fail("Archive choice no longer resolves");
    request.choices.push_back({static_cast<std::size_t>(found - rows.begin()), choice.choice});
  }
  const auto resolved = SceneMerge::Resolve(*review, request, error);
  if (!resolved || !resolved->bytes || *resolved->bytes != a->candidate)
    return fail("Archive candidate validation failed");
  return a;
}
} // namespace nexora::editor::merge_storage
