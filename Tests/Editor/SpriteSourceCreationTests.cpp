#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/Editor/SpriteSourceCreation.h"
#include "Nexora/Editor/TwoDAssetImport.h"
#include "TemporaryDirectoryCleanup.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>

using namespace nexora;
namespace {
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Write(const std::filesystem::path &p, std::span<const std::byte> bytes) {
  std::ofstream out(p, std::ios::binary);
  out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  Check(bool(out), "fixture write");
}
std::string Read(const std::filesystem::path &p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), {}};
}
auto Files(const std::filesystem::path &root) {
  std::map<std::filesystem::path, std::string> result;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file())
      result.emplace(entry.path().lexically_relative(root), Read(entry.path()));
  return result;
}
void Run() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-source-create-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup(root);
  editor::ProjectWorkspace workspace;
  Check(workspace.Create(root, "Source creation"), "workspace");
  constexpr runtime::AssetUuid atlasId{101, 1};
  twod::Atlas atlas{
      atlasId, 16, 16, 1, false, {{7, 3, 4, 7, 9}}, twod::Bytes(16 * 16 * 4, std::byte{255})};
  Check(twod::ValidateAtlas(atlas), "Atlas fixture");
  Write(root / "Content/Atlas.nx2atlas", twod::EncodeAtlas(atlas));
  editor::AssetWorkspace assets;
  Check(assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
        "import");
  editor::ProjectContentSession content;
  Check(content.Open(workspace, assets, 5), "Content");
  editor::SpriteSourceCreationRequest request;
  request.project = workspace.Project().id;
  request.root = workspace.Root();
  request.folder = "Content";
  request.version = {5, content.Browser().Revision()};
  request.name = "Created";
  request.source.atlas = atlasId;
  request.source.region = 7;
  request.source.sourceWidth = 19;
  request.source.sourceHeight = 23;
  request.source.trimX = 3;
  request.source.trimY = 5;
  request.source.trimWidth = 7;
  request.source.trimHeight = 9;
  request.source.pivotX = 4.5F;
  request.source.pivotY = 8.25F;
  request.source.pixelsPerUnit = 2;
  const auto reject = [&](editor::SpriteSourceCreationRequest r, bool allowed = true) {
    const auto before = Files(root);
    const auto revision = content.Browser().Revision();
    const auto result = editor::CreateSpriteSourceFile(workspace, content, r, allowed);
    Check(result.state == editor::SpriteSourceCreationState::Rejected && before == Files(root) &&
              revision == content.Browser().Revision(),
          "rejection changed files or catalog");
  };
  reject(request, false);
  editor::ProjectWorkspace reader;
  Check(reader.Open(root, editor::ProjectAccess::ReadOnly), "read-only workspace");
  editor::ProjectContentSession readonly;
  Check(readonly.Open(reader, assets, 5, false), "read-only Content");
  const auto readonly_files = Files(root);
  Check(editor::CreateSpriteSourceFile(reader, readonly, request, true).state ==
                editor::SpriteSourceCreationState::Rejected &&
            Files(root) == readonly_files,
        "read-only source creation changed files");
#if !defined(_WIN32)
  std::filesystem::create_directory_symlink(root / "Content", root / "Content/Alias");
  auto aliased = request;
  aliased.folder = "Content/Alias";
  reject(aliased);
  std::filesystem::remove(root / "Content/Alias");
#endif
  auto bad = request;
  bad.version.content_revision++;
  reject(bad);
  bad = request;
  bad.project = {};
  reject(bad);
  bad = request;
  bad.root = root / "Other";
  reject(bad);
  bad = request;
  bad.folder = "Content/..";
  reject(bad);
  bad = request;
  bad.source.region = 8;
  reject(bad);
  bad = request;
  bad.source.trimWidth = 8;
  reject(bad);
  bad = request;
  bad.source.pixelsPerUnit = 0;
  reject(bad);
  bad = request;
  bad.source.id = {99, 1};
  reject(bad);
  {
    auto previous = content.Browser();
    std::vector<editor::ContentItem> full(previous.Items().begin(), previous.Items().end());
    for (std::uint64_t i = 1; i < editor::kMaximumWorkspaceTwoDAssets; ++i) {
      auto source = request.source;
      source.id = {1000 + i, 1};
      auto imported = editor::ImportTwoDAsset(".nx2sprite", twod::EncodeSpriteAsset(source));
      Check(bool(imported.payload), "capacity fixture");
      full.push_back({source.id,
                      "Content/Source" + std::to_string(i) + ".nx2sprite",
                      ".nx2sprite",
                      "fixture",
                      editor::ThumbnailState::Ready,
                      {},
                      {},
                      std::move(imported.payload)});
    }
    Check(content.Browser().Reset(full, 5), "full typed Content fixture");
    auto capacity = request;
    capacity.version.content_revision = content.Browser().Revision();
    const auto result = editor::CreateSpriteSourceFile(workspace, content, capacity, true);
    Check(result.state == editor::SpriteSourceCreationState::Rejected &&
              result.error == editor::SpriteSourceCreationError::Capacity &&
              Files(root) == readonly_files,
          "4096 typed assets did not reject creation before writes");
    content.Browser() = std::move(previous);
  }
  for (const auto *name :
       {"", "../bad", "bad/name", "bad\\name", "CON", "Lpt1.bin", "trailing.", "x?"}) {
    bad = request;
    bad.name = name;
    reject(bad);
  }
  std::ofstream(root / "Content/cReAtEd.nx2sprite") << "unrelated";
  reject(request);
  std::filesystem::remove(root / "Content/cReAtEd.nx2sprite");
  std::ofstream(root / "Content/Created.nx2sprite.meta") << "unrelated identity";
  reject(request);
  std::filesystem::remove(root / "Content/Created.nx2sprite.meta");
  std::ofstream(root / ".nexora/workspace.recovery") << "pending";
  reject(request);
  std::filesystem::remove(root / ".nexora/workspace.recovery");
  std::ofstream(root / ".nexora/source-staging") << "occupied";
  reject(request);
  std::filesystem::remove(root / ".nexora/source-staging");

  // Discovery preserves the existing file-move Undo, which must also retain the new source.
  Check(content.Rename(atlasId, "MovedAtlas.nx2atlas"), "rename fixture");
  request.version.content_revision = content.Browser().Revision();
  request.name = "\xE4\xBE\x86\xE6\xBA\x90";
  const auto before = Files(root);
  const auto result = editor::CreateSpriteSourceFile(workspace, content, request, true);
  Check(result.state == editor::SpriteSourceCreationState::Created && result.asset != atlasId &&
            result.asset != runtime::AssetUuid{} && content.Browser().Items().size() == 1,
        "new file publication or implicit catalog mutation");
  const auto bytes = Read(root / result.path);
  const auto source = twod::DecodeSpriteAsset(std::as_bytes(std::span(bytes)));
  auto expected = request.source;
  expected.id = result.asset;
  Check(bytes.size() == 96 && source == expected, "canonical asymmetric source bytes");
  for (const auto &[p, value] : before)
    Check(Read(root / p) == value, "existing file overwritten");
  auto imported = assets;
  const auto saved_files = Files(root);
  Check(!imported.ImportSavedSpriteSource(result.path.lexically_relative("Content"), {999, 1}) &&
            imported.Entries().size() == assets.Entries().size() && Files(root) == saved_files,
        "wrong embedded identity import changed files or index");
  Check(imported.ImportSavedSpriteSource(result.path.lexically_relative("Content"), result.asset),
        "single saved-source import");
  const auto *entry = imported.Find(result.asset);
  Check(entry && entry->twod && *entry->twod->SpriteSource() == expected,
        "source identity/payload admission");
  auto browser = content.Browser();
  Check(browser.Discover({entry->id,
                          result.path,
                          entry->type,
                          entry->artifact_hash,
                          editor::ThumbnailState::Ready,
                          {},
                          {},
                          entry->twod}),
        "source discovery");
  auto graph = content.Dependencies();
  Check(graph.Set(result.asset, std::array{atlasId}), "source dependency");
  content.Browser() = std::move(browser);
  content.Dependencies() = std::move(graph);
  Check(content.Undo() && content.Browser().Find(result.asset) &&
            content.Browser().Find(atlasId)->path == "Content/Atlas.nx2atlas",
        "discovery lost the pending Undo or newly created asset");
  editor::AssetWorkspace reopened;
  Check(reopened.ImportTree(root / "Content", {}, {},
                            editor::AssetIdentityMode::PersistentReadOnly) &&
            reopened.Find(result.asset) &&
            *reopened.Find(result.asset)->twod->SpriteSource() == expected,
        "read-only source reopen identity");
  std::cout << "Source creation: exclusive canonical file, asymmetric metadata, guarded failures, "
               "single-file import, preserved Undo and read-only reopen passed\n";
}
} // namespace
int main() {
  try {
    Run();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
