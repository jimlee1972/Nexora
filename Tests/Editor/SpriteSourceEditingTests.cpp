#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/Editor/SpriteSourceEditing.h"
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
void Write(const std::filesystem::path &path, std::string_view bytes) {
  std::ofstream out(path, std::ios::binary);
  out.write(bytes.data(), std::streamsize(bytes.size()));
  Check(bool(out), "fixture write");
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  Write(path, std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
auto Files(const std::filesystem::path &root) {
  std::map<std::filesystem::path, std::string> files;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file())
      files.emplace(entry.path().lexically_relative(root), Read(entry.path()));
  return files;
}
void Run() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-source-edit-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup(root);
  editor::ProjectWorkspace workspace;
  Check(workspace.Create(root, "Edit Sprite source"), "workspace");
  constexpr runtime::AssetUuid atlas_id{101, 1}, source_id{202, 2};
  twod::Atlas atlas{atlas_id,
                    16,
                    16,
                    1,
                    false,
                    {{7, 3, 4, 7, 9}, {8, 12, 1, 1, 1}},
                    twod::Bytes(16 * 16 * 4, std::byte{255})};
  twod::SpriteAsset baseline{source_id, atlas_id, 7, 19, 23, 3, 5, 7, 9, 4.5F, 8.25F, 2, {}};
  Write(root / "Content/Atlas.nx2atlas", twod::EncodeAtlas(atlas));
  const auto path = root / "Content/Source.nx2sprite";
  Write(path, twod::EncodeSpriteAsset(baseline));
  Check(twod::ValidateAtlas(atlas) && twod::ValidateSpriteAsset(baseline), "asset fixture");
  editor::AssetWorkspace assets;
  std::string import_error;
  if (!assets.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite,
                         &import_error))
    throw std::runtime_error("import: " + import_error);
  // Provisional source bytes use an ignored metadata suffix, never a new indexed asset.
  const auto staging_directory = root / "Content/.nexora-sprite-edit-fixture";
  std::filesystem::create_directory(staging_directory);
  Write(staging_directory / "source.meta", twod::EncodeSpriteAsset(baseline));
  editor::AssetWorkspace staging_reader;
  editor::AssetIndexLimits staging_limits;
  staging_limits.max_assets = assets.Entries().size();
  Check(staging_reader.ImportTree(root / "Content", {}, {},
                                  editor::AssetIdentityMode::PersistentReadOnly, nullptr,
                                  staging_limits) &&
            staging_reader.Entries().size() == assets.Entries().size(),
        "ordinary read-only indexing discovered provisional source bytes");
  std::filesystem::remove(staging_directory / "source.meta");
  std::filesystem::remove(staging_directory);
  editor::ProjectContentSession content;
  Check(content.Open(workspace, assets, 5), "Content");
  Check(content.Rename(atlas_id, "Renamed.nx2atlas") && content.CanUndo(), "pending file Undo");
  const auto *item = content.Browser().Find(source_id);
  const auto retained = item->twod;
  editor::SpriteSourceEditingRequest request{
      workspace.Project().id,
      workspace.Root(),
      item->path,
      {content.Browser().ProjectGeneration(), content.Browser().Revision()},
      item->artifact_hash,
      baseline,
      baseline};
  request.source.sourceWidth = 31;
  request.source.sourceHeight = 37;
  request.source.trimX = 6;
  request.source.trimY = 7;
  request.source.pivotX = 9.75F;
  request.source.pivotY = 12.5F;
  request.source.pixelsPerUnit = 4;
  request.source.border = {};
  const auto before = Files(root);
  using State = editor::SpriteSourceEditingState;
  using Error = editor::SpriteSourceEditingError;
  const auto rejected = [&](const auto &candidate, bool authorized = true) {
    const auto snapshot = Files(root);
    const auto revision = content.Browser().Revision();
    const auto result = editor::SaveSpriteSourceFile(workspace, content, candidate, authorized);
    Check(result.state == State::Rejected && result.error != Error::None, "invalid edit admitted");
    Check(snapshot == Files(root) && content.Browser().Revision() == revision &&
              content.Browser().Find(source_id)->twod == retained && content.CanUndo(),
          "rejected edit changed source, owner or Undo");
    return result.error;
  };
  Check(rejected(request, false) == Error::Denied, "authority");
  Write(root / ".nexora/workspace.recovery", "pending");
  Check(rejected(request) == Error::Denied, "recovery authority");
  std::filesystem::remove(root / ".nexora/workspace.recovery");

  auto invalid = request;
  ++invalid.version.content_revision;
  Check(rejected(invalid) == Error::Stale, "version");
  invalid = request;
  invalid.project = {1, 2};
  rejected(invalid);
  invalid = request;
  invalid.root /= "foreign";
  rejected(invalid);
  invalid = request;
  invalid.path = "Content/../Source.nx2sprite";
  rejected(invalid);
  invalid = request;
  invalid.path = "Content/Alias.nx2sprite";
  rejected(invalid);
  invalid = request;
  invalid.artifact = "stale";
  rejected(invalid);
  invalid = request;
  invalid.baseline.pivotX += 1;
  rejected(invalid);
  invalid = request;
  invalid.source.id = {909, 9};
  rejected(invalid);
  invalid = request;
  invalid.source.atlas = {909, 9};
  rejected(invalid);
  invalid = request;
  invalid.source.region = 99;
  rejected(invalid);
  invalid = request;
  invalid.source.trimWidth = 8;
  rejected(invalid);
  invalid = request;
  invalid.source.pixelsPerUnit = 0;
  rejected(invalid);
  invalid = request;
  invalid.source.border[0] = 100;
  rejected(invalid);
  invalid = request;
  invalid.source = baseline;
  Check(rejected(invalid) == Error::Unchanged, "no-op edit");
  auto altered = Read(path);
  altered.back() ^= 1;
  Write(path, altered);
  Check(rejected(request) == Error::Stale, "external source change");
  Write(path, before.at("Content/Source.nx2sprite"));
  const auto sidecar = editor::AssetWorkspace::IdentitySidecar(path);
  const auto identity = Read(sidecar);
  Write(sidecar, "schema=1\nuuid=00000000-0000-0001-0000-000000000001\ntype=.nx2sprite\n");
  Check(rejected(request) == Error::Identity, "foreign sidecar");
  Write(sidecar, "schema=1\nuuid=" + source_id.ToString() + "\nTYPE=.nx2sprite\n");
  Check(rejected(request) == Error::Identity, "unsupported identity field case");
  Write(sidecar, identity);
  const auto alias = root / "Content/Shared.nx2sprite";
  std::error_code ec;
  const auto case_alias = root / "Content/SOURCE.nx2sprite";
  if (!std::filesystem::exists(case_alias, ec)) {
    Write(case_alias, "unrelated case-alias source");
    Check(rejected(request) == Error::Path, "case-alias source");
    std::filesystem::remove(case_alias);
  }
  ec.clear();
  std::filesystem::create_hard_link(path, alias, ec);
  if (!ec) {
    rejected(request);
    std::filesystem::remove(alias);
  }
#if !defined(_WIN32)
  const auto original = root / "Content/Original.bin";
  std::filesystem::rename(path, original);
  std::filesystem::create_symlink(original, path);
  rejected(request);
  std::filesystem::remove(path);
  std::filesystem::rename(original, path);
  const auto content_backup = root / "OriginalContent";
  std::filesystem::rename(root / "Content", content_backup);
  std::filesystem::create_directory_symlink(content_backup, root / "Content");
  Check(rejected(request) == Error::Path, "symlink Content parent");
  std::filesystem::remove(root / "Content");
  std::filesystem::rename(content_backup, root / "Content");
#endif
  editor::ProjectWorkspace reader;
  Check(reader.Open(root, editor::ProjectAccess::ReadOnly), "read-only workspace");
  Check(editor::SaveSpriteSourceFile(reader, content, request, true).error == Error::Denied,
        "read-only authority");
  const auto saved = editor::SaveSpriteSourceFile(workspace, content, request, true);
  Check(saved.state == State::Saved && saved.error == Error::None, "source save");
  Check(Read(sidecar) == identity && Read(path).size() == 96 && content.CanUndo(),
        "save changed identity, wire or file history");
  auto after = Files(root);
  after["Content/Source.nx2sprite"] = before.at("Content/Source.nx2sprite");
  Check(after == before, "edit changed an unrelated file or retained staging file");
  Check(*retained->SpriteSource() == baseline &&
            *content.Browser().Find(source_id)->twod->SpriteSource() == request.source,
        "immutable source publication");
  Check(content.Undo() && content.Browser().Find(atlas_id)->path == "Content/Atlas.nx2atlas" &&
            *content.Browser().Find(source_id)->twod->SpriteSource() == request.source,
        "file Undo lost new source metadata");
  const auto edited_owner = content.Browser().Find(source_id)->twod;
  Check(content.Reimport(source_id) &&
            content.Browser().Find(source_id)->artifact_hash != request.artifact &&
            *content.Browser().Find(source_id)->twod->SpriteSource() == request.source,
        "ordinary reimport disagrees with edited artifact");
  const auto reopened_files = Files(root);
  editor::AssetWorkspace reopened;
  Check(reopened.ImportTree(root / "Content", {}, {},
                            editor::AssetIdentityMode::PersistentReadOnly) &&
            reopened.Find(source_id) &&
            *reopened.Find(source_id)->twod->SpriteSource() == request.source &&
            reopened.Find(source_id)->artifact_hash ==
                content.Browser().Find(source_id)->artifact_hash &&
            Files(root) == reopened_files && *edited_owner->SpriteSource() == request.source,
        "read-only reopen identity, payload or artifact");
  Check(editor::SaveSpriteSourceFile(workspace, content, request, true).error == Error::Stale,
        "previous draft must not overwrite newer revision");
  Check(content.Rename(source_id, "Uppercase.NX2SPRITE"), "uppercase imported extension");
  item = content.Browser().Find(source_id);
  auto bordered = request;
  bordered.path = item->path;
  bordered.artifact = item->artifact_hash;
  bordered.version = {content.Browser().ProjectGeneration(), content.Browser().Revision()};
  bordered.baseline = *item->twod->SpriteSource();
  bordered.source = bordered.baseline;
  bordered.source.sourceWidth = bordered.source.trimWidth;
  bordered.source.sourceHeight = bordered.source.trimHeight;
  bordered.source.trimX = bordered.source.trimY = 0;
  bordered.source.border = {1, 2, 3, 4};
  Check(editor::SaveSpriteSourceFile(workspace, content, bordered, true).state == State::Saved &&
            content.Browser().Find(source_id)->twod->SpriteSource()->border ==
                bordered.source.border &&
            Read(editor::AssetWorkspace::IdentitySidecar(root / bordered.path)) == identity,
        "valid full-canvas borders or uppercase source extension rejected");
  const auto uppercase_sidecar = editor::AssetWorkspace::IdentitySidecar(root / bordered.path);
  const auto crlf_identity = "schema=1\r\nuuid=" + source_id.ToString() + "\r\ntype=.NX2SPRITE\r\n";
  Write(uppercase_sidecar, crlf_identity);
  item = content.Browser().Find(source_id);
  bordered.artifact = item->artifact_hash;
  bordered.version = {content.Browser().ProjectGeneration(), content.Browser().Revision()};
  bordered.baseline = *item->twod->SpriteSource();
  bordered.source = bordered.baseline;
  bordered.source.pivotX += .25F;
  Check(editor::SaveSpriteSourceFile(workspace, content, bordered, true).state == State::Saved &&
            Read(uppercase_sidecar) == crlf_identity &&
            reopened.ImportTree(root / "Content", {}, {},
                                editor::AssetIdentityMode::PersistentReadOnly) &&
            *reopened.Find(source_id)->twod->SpriteSource() == bordered.source,
        "valid CRLF/type-value case must preserve bytes and ordinary reopen compatibility");
}
} // namespace
int main() {
  try {
    Run();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
