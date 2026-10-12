#include "Nexora/Editor/AssetImport.h"
#include "Nexora/Editor/ProjectContent.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#if defined(__linux__)
#include <sys/resource.h>
#endif

namespace {
using namespace nexora;

void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

struct Cleanup final {
  std::filesystem::path root;
  ~Cleanup() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
};

void WritePayload(const std::filesystem::path &path, std::size_t size) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  for (std::size_t index = 0; index < size; ++index)
    output.put(static_cast<char>(index % 251));
  Require(output.good(), "binary source fixture could not be written");
}

void TestCancellation(const std::filesystem::path &root) {
  std::filesystem::create_directories(root);
  WritePayload(root / "Payload.asset", 24595);
  editor::AssetWorkspace candidate;
  std::size_t polls{};
  Require(candidate.ImportTree(root, [&] { return ++polls >= 4; }),
          "cancelled source scan failed instead of recording cancellation");
  Require(candidate.Entries().size() == 1 &&
              candidate.Entries().front().state == editor::ImportState::Cancelled &&
              candidate.Entries().front().artifact_hash.empty() &&
              !candidate.Entries().front().mesh,
          "non-OBJ source read ignored mid-file cancellation or published a partial hash");
}

void WriteIdentity(const std::filesystem::path &path, runtime::AssetUuid asset) {
  std::ofstream output(editor::AssetWorkspace::IdentitySidecar(path), std::ios::binary);
  output << "schema=1\nuuid=" << asset.ToString() << "\ntype=.asset\n";
  Require(output.good(), "persistent source identity could not be written");
}

void TestLargeSource(const std::filesystem::path &root) {
  std::filesystem::create_directories(root);
  const auto path = root / "Payload.asset";
  constexpr std::size_t size = 32 * 1024 * 1024 + 19;
  {
    std::ofstream output(path, std::ios::binary);
    std::array<char, 8192> block{};
    for (std::size_t offset = 0; offset < size; offset += block.size()) {
      const auto count = std::min(block.size(), size - offset);
      for (std::size_t index = 0; index < count; ++index)
        block[index] = static_cast<char>((offset + index) % 251);
      output.write(block.data(), static_cast<std::streamsize>(count));
    }
    Require(output.good(), "large binary source could not be written");
  }
  const runtime::AssetUuid asset{1, 2};
  WriteIdentity(path, asset);
#if defined(__linux__)
  rusage before{};
  Require(getrusage(RUSAGE_SELF, &before) == 0, "source memory baseline unavailable");
#endif
  editor::AssetWorkspace assets;
  Require(assets.ImportTree(root, {}, {}, editor::AssetIdentityMode::PersistentReadOnly) &&
              assets.Find(asset) && assets.Find(asset)->artifact_hash == "2ac10d0f16ae5fd4",
          "large ordinary source was rejected or changed its artifact hash");
#if defined(__linux__)
  rusage after{};
  Require(getrusage(RUSAGE_SELF, &after) == 0 && after.ru_maxrss - before.ru_maxrss < 16 * 1024,
          "ordinary source hashing retained memory proportional to the 32 MiB payload");
  std::cout << "32 MiB source peak RSS growth (KiB): " << after.ru_maxrss - before.ru_maxrss
            << '\n';
#endif
}

editor::ImportOperationResult AwaitResult(editor::AssetImportQueue &imports,
                                          editor::ImportOperationId operation) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    if (auto result = imports.TakeResult(operation))
      return std::move(*result);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  throw std::runtime_error("source import did not finish");
}

void TestHashCompatibility(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  std::string error;
  Require(workspace.Create(root, "Asset Source", &error), "project creation failed");
  const auto source = root / "Content/Payload.asset";
  const auto empty = root / "Content/Empty.asset";
  const auto boundary = root / "Content/Boundary.asset";
  const runtime::AssetUuid asset{1, 2}, empty_asset{3, 4}, boundary_asset{5, 6};
  WritePayload(source, 24595);
  WritePayload(empty, 0);
  WritePayload(boundary, 8192);
  WriteIdentity(source, asset);
  WriteIdentity(empty, empty_asset);
  WriteIdentity(boundary, boundary_asset);

  // Frozen pre-streaming FNV-1a values: binary bytes i % 251, UUID.ToString() prefix for artifacts.
  editor::AssetWorkspace assets;
  Require(assets.ImportTree(root / "Content", {}, {},
                            editor::AssetIdentityMode::PersistentReadWrite, &error) &&
              assets.Find(asset) && assets.Find(asset)->artifact_hash == "cf593a78267c9069" &&
              assets.Find(empty_asset) &&
              assets.Find(empty_asset)->artifact_hash == "ba0537b31b7e4a4e" &&
              assets.Find(boundary_asset) &&
              assets.Find(boundary_asset)->artifact_hash == "299e0e1b695b82ee",
          "workspace streaming changed empty, binary or exact-chunk artifact hashes");

  core::JobSystem jobs{1};
  jobs.Start();
  editor::AssetImportQueue imports{jobs};
  editor::ProjectContentSession content;
  Require(content.Open(workspace, assets, 77, true, &error), "content session opening failed");
  WritePayload(source, 40967);
  Require(content.Reimport(asset, &error) &&
              content.Browser().Find(asset)->artifact_hash == "2871c35772686f00",
          "synchronous reimport changed the binary source artifact hash");

  const auto operation = imports.Start(
      editor::ReimportJobRequest{77, asset, source, "2871c35772686f00", "default-v1", {}});
  Require(operation != 0, "background binary source import did not start");
  const auto result = AwaitResult(imports, operation);
  Require(result.snapshot.state == editor::ImportOperationState::AwaitingPublish &&
              result.reimport && result.reimport->source_hash == "38ad32fd60b6ee87" &&
              result.reimport->artifact_hash == "2871c35772686f00" && !result.reimport->mesh &&
              content.Browser().Find(asset)->artifact_hash == "2871c35772686f00" &&
              !imports.TakeResult(operation),
          "background stream changed hashes, mutated live state or exposed a result twice");

  const auto empty_operation = imports.Start(
      editor::ReimportJobRequest{77, empty_asset, empty, "ba0537b31b7e4a4e", "default-v1", {}});
  Require(empty_operation != 0, "empty source reimport did not start");
  const auto empty_result = AwaitResult(imports, empty_operation);
  Require(empty_result.reimport && empty_result.reimport->source_hash == "14650fb0739d0383" &&
              empty_result.reimport->artifact_hash == "ba0537b31b7e4a4e",
          "empty source reimport changed its source/artifact hash");

  WritePayload(source, 24595);
  Require(content.BeginReimport(imports, asset, &error), "content background reimport failed");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (content.ReimportBusy() && std::chrono::steady_clock::now() < deadline) {
    static_cast<void>(content.PollReimport(&error));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Require(!content.ReimportBusy() && content.ReimportStatus() &&
              content.ReimportStatus()->state == editor::ImportOperationState::Succeeded &&
              content.Browser().Find(asset)->artifact_hash == "cf593a78267c9069",
          "authoring-thread publication did not preserve the streaming hash contract");
}

std::size_t CountSidecars(const std::filesystem::path &root) {
  std::size_t count{};
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file() && entry.path().extension() == ".meta")
      ++count;
  return count;
}

void TestIndexBounds(const std::filesystem::path &root) {
  std::filesystem::create_directories(root / "Nested");
  for (const auto *name : {"A.asset", "B.asset", "C.asset", "Nested/D.asset"})
    WritePayload(root / name, 16);
  // A pre-existing sidecar must not count against the file budget.
  std::ofstream(root / "A.asset.meta") << "schema=1\n";

  editor::AssetWorkspace assets;
  std::string error;
  Require(assets.ImportTree(root, {}, {}, editor::AssetIdentityMode::DerivedFromPath, &error) &&
              assets.Entries().size() == 4,
          "default limits rejected a small tree");
  const auto before = assets.Entries().size();

  Require(!assets.ImportTree(root, {}, {}, editor::AssetIdentityMode::DerivedFromPath, &error,
                             {3, editor::kMaximumIndexedPathBytes}) &&
              error.find("3-file project index limit") != std::string::npos &&
              assets.Entries().size() == before,
          "an oversized tree was accepted or replaced the previous index");
  Require(assets.ImportTree(root, {}, {}, editor::AssetIdentityMode::DerivedFromPath, &error,
                            {4, editor::kMaximumIndexedPathBytes}) &&
              error.empty() && assets.Entries().size() == 4,
          "a tree exactly at the file limit was rejected");

  // A rejected writable import must not create identity sidecars for the files it did enumerate.
  const auto sidecars = CountSidecars(root);
  Require(!assets.ImportTree(root, {}, {}, editor::AssetIdentityMode::PersistentReadWrite, &error,
                             {2, editor::kMaximumIndexedPathBytes}) &&
              CountSidecars(root) == sidecars && assets.Entries().size() == before,
          "a rejected writable import left identity sidecars behind");

  // "Nested/D.asset" is 14 UTF-8 bytes; the bound is on the project-relative path.
  Require(assets.ImportTree(root, {}, {}, editor::AssetIdentityMode::DerivedFromPath, &error,
                            {editor::kMaximumIndexedAssets, 14}) &&
              assets.Entries().size() == 4,
          "a path exactly at the byte limit was rejected");
  Require(!assets.ImportTree(root, {}, {}, editor::AssetIdentityMode::DerivedFromPath, &error,
                             {editor::kMaximumIndexedAssets, 13}) &&
              error.find("13-byte project index limit") != std::string::npos &&
              assets.Entries().size() == before,
          "an over-long relative path was accepted or replaced the previous index");
}
} // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-asset-source-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Cleanup cleanup{root};
  try {
    TestLargeSource(root / "Large");
    TestCancellation(root / "Cancelled");
    TestHashCompatibility(root / "Project");
    TestIndexBounds(root / "Bounds");
    std::cout << "PASS: streamed binary/empty/chunk hashes, mid-file cancellation, reimport and "
                 "index bounds\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
