#pragma once
#include "Nexora/Runtime/Api.h"
#include "nexora/gameplay_storage.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace nexora::runtime {
// Resolves a per-user directory without creating it. Portable lower-case scope, 1..128 bytes.
// Throws for invalid scope or unavailable environment. No executable/source directory fallback.
[[nodiscard]] NEXORA_RUNTIME_API std::filesystem::path
UserGameplaySaveDirectory(std::string_view scope);
// Dedicated host-owned directory, never shared by simultaneous service instances/writers.
// Configure from trusted host paths, never from gameplay input. Owner serializes all calls.
class NEXORA_RUNTIME_API GameplayStorage final {
public:
  GameplayStorage(std::filesystem::path root, std::string scope,
                  std::uint32_t max_payload = 16u * 1024u * 1024u);
  GameplayStorage(const GameplayStorage &) = delete;
  GameplayStorage &operator=(const GameplayStorage &) = delete;
  // Caller initializes struct_size >= sizeof(V1), abi_version == 1. Failure preserves table.
  [[nodiscard]] std::int32_t GetApi(NexoraStorageApiV1 &output) noexcept;

private:
  std::filesystem::path root_;
  std::string scope_;
  std::uint32_t max_payload_;
  std::thread::id owner_;
  std::int32_t Check(const char *key, std::uint32_t size) const;
  std::int32_t Read(const char *key, std::uint32_t size, std::vector<std::uint8_t> &payload,
                    std::uint32_t &schema) const;
  static std::int32_t Save(void *, const char *, std::uint32_t, std::uint32_t, const void *,
                           std::uint32_t) noexcept;
  static std::int32_t Load(void *, const char *, std::uint32_t, void *, std::uint32_t,
                           NexoraStorageInfoV1 *) noexcept;
  static std::int32_t Remove(void *, const char *, std::uint32_t) noexcept;
  static std::int32_t Exists(void *, const char *, std::uint32_t, std::uint32_t *) noexcept;
};
} // namespace nexora::runtime
