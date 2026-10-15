#include "Nexora/Runtime/GameplayStorage.h"
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace {
void Require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
struct Directory {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("nexora-storage-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  ~Directory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};
NexoraStorageApiV1 Api(nexora::runtime::GameplayStorage &storage) {
  NexoraStorageApiV1 api{};
  api.struct_size = sizeof(api);
  api.abi_version = 1;
  Require(storage.GetApi(api) == 0, "API negotiation");
  return api;
}
void Contracts() {
  Directory directory;
  nexora::runtime::GameplayStorage storage(directory.path, "project-identity", 64);
  auto api = Api(storage);
  NexoraStorageApiV1 invalid{};
  invalid.struct_size = 1;
  Require(storage.GetApi(invalid) == NEXORA_STORAGE_INVALID_ARGUMENT && invalid.struct_size == 1,
          "API failure mutated output");
  std::uint32_t exists = 99;
  Require(api.exists(api.context, "run", 3, &exists) == 0 && exists == 0, "missing slot existence");
  const std::array<std::uint8_t, 4> original{0, 17, 255, 0};
  Require(api.save(api.context, "run", 3, 7, original.data(), original.size()) == 0, "save");
  {
    std::ofstream stale(directory.path / "run.tmp");
    stale << "incomplete";
  }
  // Re-create the host service: persistence must outlive a module/service instance.
  nexora::runtime::GameplayStorage reopened(directory.path, "project-identity", 64);
  auto reader = Api(reopened);
  NexoraStorageInfoV1 info{sizeof(info), 1, 99, 99};
  Require(reader.load(reader.context, "run", 3, nullptr, 0, &info) == 0 &&
              info.schema_version == 7 && info.payload_size == 4,
          "probe/persistence");
  std::array<std::uint8_t, 4> output{42, 42, 42, 42};
  info.schema_version = 99;
  Require(reader.load(reader.context, "run", 3, output.data(), 3, &info) ==
                  NEXORA_STORAGE_BUFFER_TOO_SMALL &&
              output[0] == 42 && info.schema_version == 99,
          "bounded load changed output");
  Require(reader.load(reader.context, "run", 3, output.data(), output.size(), &info) == 0 &&
              output == original,
          "binary payload");
  Require(api.save(api.context, "../run", 6, 1, original.data(), original.size()) ==
              NEXORA_STORAGE_INVALID_ARGUMENT,
          "traversal");
  Require(api.save(api.context, "run", 3, 1, original.data(), 65) == NEXORA_STORAGE_LIMIT_EXCEEDED,
          "payload limit");
  Require(api.save(api.context, "run", 3, 9, original.data(), original.size()) == 0 &&
              !std::filesystem::exists(directory.path / "run.tmp"),
          "atomic overwrite/stale temp");
  nexora::runtime::GameplayStorage wrong_scope(directory.path, "other-project", 64);
  auto scoped = Api(wrong_scope);
  Require(scoped.load(scoped.context, "run", 3, nullptr, 0, &info) == NEXORA_STORAGE_CORRUPT,
          "scope identity");
  std::filesystem::copy_file(directory.path / "run.nxsav", directory.path / "copy.nxsav");
  Require(api.load(api.context, "copy", 4, nullptr, 0, &info) == NEXORA_STORAGE_CORRUPT,
          "slot identity");
  int thread_result = 0;
  std::thread foreign([&] { thread_result = api.exists(api.context, "run", 3, &exists); });
  foreign.join();
  Require(thread_result == NEXORA_STORAGE_WRONG_THREAD, "owner thread");
  {
    std::fstream file(directory.path / "run.nxsav",
                      std::ios::binary | std::ios::in | std::ios::out);
    file.seekp(24);
    file.put('!');
  }
  output.fill(42);
  info.schema_version = 99;
  exists = 99;
  Require(api.load(api.context, "run", 3, output.data(), output.size(), &info) ==
                  NEXORA_STORAGE_CORRUPT &&
              output[0] == 42 && info.schema_version == 99,
          "corruption output preservation");
  Require(api.exists(api.context, "run", 3, &exists) == NEXORA_STORAGE_CORRUPT && exists == 99,
          "corrupt existence");
  Require(api.remove(api.context, "run", 3) == 0 &&
              api.remove(api.context, "run", 3) == NEXORA_STORAGE_NOT_FOUND,
          "delete slot");
  {
    std::ofstream orphan(directory.path / "orphan.tmp");
    orphan << "incomplete";
  }
  Require(api.exists(api.context, "orphan", 6, &exists) == 0 && exists == 0, "orphan promoted");
  Require(api.save(api.context, "empty", 5, 0, nullptr, 0) == 0 &&
              api.load(api.context, "empty", 5, nullptr, 0, &info) == 0 && info.payload_size == 0,
          "empty save");
#ifndef _WIN32
  std::filesystem::create_symlink(directory.path / "empty.nxsav", directory.path / "link.nxsav");
  Require(api.save(api.context, "link", 4, 0, nullptr, 0) == NEXORA_STORAGE_IO_ERROR,
          "symlink save target");
#endif
}
} // namespace
int main() {
  try {
    Contracts();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
