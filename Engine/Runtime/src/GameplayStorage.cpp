#include "Nexora/Runtime/GameplayStorage.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <span>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace nexora::runtime {
namespace {
constexpr std::array<std::uint8_t, 8> Magic{'N', 'X', 'S', 'A', 'V', 'E', 1, 0};
void Put(std::vector<std::uint8_t> &out, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i)
    out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
std::uint32_t Get(const std::vector<std::uint8_t> &in, std::size_t offset) {
  std::uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i)
    value |= static_cast<std::uint32_t>(in[offset + i]) << (8 * i);
  return value;
}
std::uint32_t Checksum(std::span<const std::uint8_t> bytes) {
  std::uint32_t crc = 0xffffffffu;
  for (const auto byte : bytes) {
    crc ^= byte;
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}
bool PlainFile(const std::filesystem::path &path) {
  return std::filesystem::is_regular_file(std::filesystem::symlink_status(path));
}
bool Commit(const std::filesystem::path &temp, const std::filesystem::path &target,
            const std::vector<std::uint8_t> &bytes) {
#ifdef _WIN32
  const HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;
  DWORD written = 0;
  const bool good =
      WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
      written == bytes.size() && FlushFileBuffers(file);
  CloseHandle(file);
  return good && MoveFileExW(temp.c_str(), target.c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
  const int file = open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (file < 0)
    return false;
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const auto count = write(file, bytes.data() + offset, bytes.size() - offset);
    if (count <= 0) {
      close(file);
      return false;
    }
    offset += static_cast<std::size_t>(count);
  }
  const bool good = fsync(file) == 0;
  const bool closed = close(file) == 0;
  if (!good || !closed || rename(temp.c_str(), target.c_str()) != 0)
    return false;
  const int directory = open(target.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
  if (directory < 0)
    return false;
  const bool synced = fsync(directory) == 0;
  close(directory);
  return synced;
#endif
}
} // namespace
std::filesystem::path UserGameplaySaveDirectory(std::string_view scope) {
  const auto alnum = [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };
  if (scope.empty() || scope.size() > 128 || !alnum(scope.front()) || !alnum(scope.back()) ||
      !std::all_of(scope.begin(), scope.end(),
                   [&](char c) { return alnum(c) || c == '.' || c == '_' || c == '-'; }))
    throw std::invalid_argument("invalid gameplay save scope");
  std::filesystem::path base;
#ifdef _WIN32
  const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
  if (length) {
    std::wstring value(length, L'\0');
    const auto copied = GetEnvironmentVariableW(L"LOCALAPPDATA", value.data(), length);
    if (copied && copied < length) {
      value.resize(copied);
      base = value;
    }
  }
#else
  if (const auto *value = std::getenv("XDG_DATA_HOME"); value && *value)
    base = value;
  else if (const auto *home = std::getenv("HOME"); home && *home)
    base = std::filesystem::path(home) / ".local/share";
#endif
  if (base.empty() || !base.is_absolute())
    throw std::runtime_error(
        "absolute user data directory unavailable; configure a save directory");
  return base / "Nexora" / "Games" / ("game-" + std::string(scope)) / "saves";
}
GameplayStorage::GameplayStorage(std::filesystem::path root, std::string scope,
                                 std::uint32_t max_payload)
    : root_(std::move(root)), scope_(std::move(scope)), max_payload_(max_payload),
      owner_(std::this_thread::get_id()) {}
std::int32_t GameplayStorage::Check(const char *key, std::uint32_t size) const {
  if (std::this_thread::get_id() != owner_)
    return NEXORA_STORAGE_WRONG_THREAD;
  if (!key || size == 0 || size > 64 || scope_.empty() || scope_.size() > 256 || root_.empty() ||
      max_payload_ == 0 || max_payload_ > 64u * 1024u * 1024u)
    return NEXORA_STORAGE_INVALID_ARGUMENT;
  for (std::uint32_t i = 0; i < size; ++i) {
    const char c = key[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
          c == '-'))
      return NEXORA_STORAGE_INVALID_ARGUMENT;
  }
  return NEXORA_STORAGE_OK;
}
std::int32_t GameplayStorage::GetApi(NexoraStorageApiV1 &output) noexcept {
  if (std::this_thread::get_id() != owner_)
    return NEXORA_STORAGE_WRONG_THREAD;
  if (output.struct_size < sizeof(output) || output.abi_version != NEXORA_STORAGE_ABI_VERSION)
    return NEXORA_STORAGE_INVALID_ARGUMENT;
  output = {sizeof(output), NEXORA_STORAGE_ABI_VERSION, this, Save, Load, Remove, Exists};
  return NEXORA_STORAGE_OK;
}
std::int32_t GameplayStorage::Read(const char *key, std::uint32_t size,
                                   std::vector<std::uint8_t> &payload,
                                   std::uint32_t &schema) const {
  const auto check = Check(key, size);
  if (check != NEXORA_STORAGE_OK)
    return check;
  const auto path = root_ / (std::string(key, size) + ".nxsav");
  if (!std::filesystem::exists(std::filesystem::symlink_status(path)))
    return NEXORA_STORAGE_NOT_FOUND;
  if (!PlainFile(path))
    return NEXORA_STORAGE_IO_ERROR;
  const auto length = std::filesystem::file_size(path);
  if (length < 28 || length > static_cast<std::uint64_t>(max_payload_) + 348)
    return NEXORA_STORAGE_CORRUPT;
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
  std::ifstream stream(path, std::ios::binary);
  if (!stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(length)))
    return NEXORA_STORAGE_IO_ERROR;
  if (stream.peek() != std::char_traits<char>::eof())
    return NEXORA_STORAGE_CORRUPT;
  if (!std::equal(Magic.begin(), Magic.end(), bytes.begin()))
    return NEXORA_STORAGE_CORRUPT;
  const auto scope_size = Get(bytes, 12), key_size = Get(bytes, 16), payload_size = Get(bytes, 20);
  if (scope_size != scope_.size() || key_size != size || payload_size > max_payload_ ||
      28ull + scope_size + key_size + payload_size != length)
    return NEXORA_STORAGE_CORRUPT;
  if (Get(bytes, bytes.size() - 4) != Checksum(std::span(bytes).first(bytes.size() - 4)) ||
      std::memcmp(bytes.data() + 24, scope_.data(), scope_size) != 0 ||
      std::memcmp(bytes.data() + 24 + scope_size, key, size) != 0)
    return NEXORA_STORAGE_CORRUPT;
  schema = Get(bytes, 8);
  payload.assign(bytes.begin() + 24 + scope_size + key_size, bytes.end() - 4);
  return NEXORA_STORAGE_OK;
}
std::int32_t GameplayStorage::Save(void *context, const char *key, std::uint32_t size,
                                   std::uint32_t schema, const void *data,
                                   std::uint32_t data_size) noexcept {
  try {
    if (!context || (!data && data_size))
      return NEXORA_STORAGE_INVALID_ARGUMENT;
    auto &self = *static_cast<GameplayStorage *>(context);
    const auto check = self.Check(key, size);
    if (check != NEXORA_STORAGE_OK)
      return check;
    if (data_size > self.max_payload_)
      return NEXORA_STORAGE_LIMIT_EXCEEDED;
    std::filesystem::create_directories(self.root_);
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(self.root_)))
      return NEXORA_STORAGE_IO_ERROR;
    const auto target = self.root_ / (std::string(key, size) + ".nxsav");
    const auto temp = self.root_ / (std::string(key, size) + ".tmp");
    for (const auto &path : {target, temp}) {
      if (std::filesystem::exists(std::filesystem::symlink_status(path)) && !PlainFile(path))
        return NEXORA_STORAGE_IO_ERROR;
    }
    std::filesystem::remove(temp); // An interrupted write never replaces the last committed slot.
    std::vector<std::uint8_t> bytes(Magic.begin(), Magic.end());
    Put(bytes, schema);
    Put(bytes, static_cast<std::uint32_t>(self.scope_.size()));
    Put(bytes, size);
    Put(bytes, data_size);
    bytes.insert(bytes.end(), self.scope_.begin(), self.scope_.end());
    bytes.insert(bytes.end(), key, key + size);
    if (data_size) {
      const auto *begin = static_cast<const std::uint8_t *>(data);
      bytes.insert(bytes.end(), begin, begin + data_size);
    }
    Put(bytes, Checksum(bytes));
    if (!Commit(temp, target, bytes)) {
      std::error_code ignored;
      std::filesystem::remove(temp, ignored);
      return NEXORA_STORAGE_IO_ERROR;
    }
    return NEXORA_STORAGE_OK;
  } catch (...) {
    return NEXORA_STORAGE_IO_ERROR;
  }
}
std::int32_t GameplayStorage::Load(void *context, const char *key, std::uint32_t size, void *data,
                                   std::uint32_t capacity, NexoraStorageInfoV1 *info) noexcept {
  try {
    if (!context || !info || info->struct_size < sizeof(*info) ||
        info->abi_version != NEXORA_STORAGE_ABI_VERSION || (!data && capacity))
      return NEXORA_STORAGE_INVALID_ARGUMENT;
    std::vector<std::uint8_t> payload;
    std::uint32_t schema = 0;
    const auto result = static_cast<GameplayStorage *>(context)->Read(key, size, payload, schema);
    if (result != NEXORA_STORAGE_OK)
      return result;
    if (data && capacity < payload.size())
      return NEXORA_STORAGE_BUFFER_TOO_SMALL;
    if (data && !payload.empty())
      std::memcpy(data, payload.data(), payload.size());
    *info = {sizeof(*info), NEXORA_STORAGE_ABI_VERSION, schema,
             static_cast<std::uint32_t>(payload.size())};
    return NEXORA_STORAGE_OK;
  } catch (...) {
    return NEXORA_STORAGE_IO_ERROR;
  }
}
std::int32_t GameplayStorage::Remove(void *context, const char *key, std::uint32_t size) noexcept {
  try {
    if (!context)
      return NEXORA_STORAGE_INVALID_ARGUMENT;
    auto &self = *static_cast<GameplayStorage *>(context);
    const auto result = self.Check(key, size);
    if (result != NEXORA_STORAGE_OK)
      return result;
    const auto target = self.root_ / (std::string(key, size) + ".nxsav");
    if (!std::filesystem::exists(std::filesystem::symlink_status(target)))
      return NEXORA_STORAGE_NOT_FOUND;
    if (!PlainFile(target))
      return NEXORA_STORAGE_IO_ERROR;
    return std::filesystem::remove(target) ? NEXORA_STORAGE_OK : NEXORA_STORAGE_IO_ERROR;
  } catch (...) {
    return NEXORA_STORAGE_IO_ERROR;
  }
}
std::int32_t GameplayStorage::Exists(void *context, const char *key, std::uint32_t size,
                                     std::uint32_t *output) noexcept {
  try {
    if (!context || !output)
      return NEXORA_STORAGE_INVALID_ARGUMENT;
    std::vector<std::uint8_t> payload;
    std::uint32_t schema = 0;
    const auto result = static_cast<GameplayStorage *>(context)->Read(key, size, payload, schema);
    if (result != NEXORA_STORAGE_OK && result != NEXORA_STORAGE_NOT_FOUND)
      return result;
    *output = result == NEXORA_STORAGE_OK ? 1u : 0u;
    return NEXORA_STORAGE_OK;
  } catch (...) {
    return NEXORA_STORAGE_IO_ERROR;
  }
}
} // namespace nexora::runtime
