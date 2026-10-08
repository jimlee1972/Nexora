#include "../../Engine/Editor/src/ReimportSource.h"
#include "Nexora/Editor/MaterialImport.h"
#include "Nexora/Editor/ProjectContent.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
std::string Source(float metallic = .25F) {
  return "NEXORA_MATERIAL 1\nbase_color 0.2 0.4 0.6\nmetallic " + std::to_string(metallic) +
         "\nroughness 0.5\nocclusion 1\nemission 0 0 2\n";
}
void Write(const std::filesystem::path &path, const std::string &source) {
  std::ofstream(path, std::ios::binary) << source;
}
void AwaitWorker(editor::ProjectContentSession &content) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    const auto status = content.ReimportStatus();
    if (status && status->state == editor::ImportOperationState::AwaitingPublish)
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  throw std::runtime_error("material worker did not stage its result");
}
void AwaitPublication(editor::ProjectContentSession &content) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (content.ReimportBusy() && std::chrono::steady_clock::now() < deadline) {
    static_cast<void>(content.PollReimport());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Require(!content.ReimportBusy(), "material reimport did not finish");
}
void TestParser() {
  const auto parsed = editor::ImportMaterial(Source());
  Require(parsed.material && parsed.error.empty() && !parsed.cancelled &&
              parsed.material->base_color == std::array{.2F, .4F, .6F} &&
              parsed.material->metallic == .25F && parsed.material->roughness == .5F &&
              parsed.material->occlusion == 1 &&
              parsed.material->emission == std::array{0.F, 0.F, 2.F} &&
              parsed.material->schema.shading_model == renderer::ShadingModel::PBR &&
              parsed.material->schema.surface_mode == renderer::SurfaceMode::Opaque &&
              renderer::ValidateMaterial(parsed.material->schema).valid &&
              parsed.material->schema.parameters.size() == 5 &&
              parsed.material->schema.features ==
                  renderer::FeatureBit(renderer::MaterialFeature::Emission),
          "valid scalar material was not decoded into shared renderer schema");
  Require(std::get<std::array<float, 3>>(parsed.material->schema.parameters[0].value) ==
                  parsed.material->base_color &&
              std::get<float>(parsed.material->schema.parameters[1].value) ==
                  parsed.material->metallic,
          "renderer schema parameters disagree with imported values");
  const auto reject = [](const std::string &source) {
    const auto result = editor::ImportMaterial(source);
    Require(!result.material && !result.error.empty() && !result.cancelled,
            "invalid material published partial data");
  };
  reject("");
  reject(Source() + "unknown 1\n");
  reject(Source().substr(0, Source().find("emission")));
  reject(Source() + std::string(1, '\0'));
  reject(std::string(editor::kMaximumMaterialSourceBytes + 1, ' '));
  reject(Source(-.1F));
  reject(Source(1.1F));
  for (const auto &replacement : {"nan", "inf", "1e100", "-0.1", "1.1"}) {
    auto source = Source();
    source.replace(source.find("0.2"), 3, replacement);
    reject(source);
  }
  for (const auto &replacement :
       {"NEXORA_MATERIAL 2", "NEXORA_MATERIAL 01", "NEXORA_MATERIAL 1.0", "OTHER_MATERIAL 1"}) {
    auto source = Source();
    source.replace(0, std::string("NEXORA_MATERIAL 1").size(), replacement);
    reject(source);
  }
  auto source = Source();
  source.replace(source.find("0.250000\n"), 9, "0.250000");
  reject(source);
  source = Source();
  source.replace(source.find("0.2"), 3, "0,2");
  reject(source);
  source = Source();
  source.replace(source.find("roughness"), 9, "metallic");
  reject(source);
  source = Source();
  source.replace(source.find("emission 0 0 2"), 14, "emission 0 0 65505");
  reject(source);
  source = Source();
  source.replace(source.find("emission 0 0 2"), 14, "emission 0 0 65504");
  Require(editor::ImportMaterial(source).material.has_value(), "HDR emission boundary rejected");
  source = Source();
  source.replace(source.find("emission 0 0 2"), 14, "emission 0 0 0");
  Require(editor::ImportMaterial(source).material->schema.features == 0,
          "zero emission selected an emission feature");
  source = Source();
  source.resize(editor::kMaximumMaterialSourceBytes, ' ');
  Require(editor::ImportMaterial(source).material.has_value(), "exact material budget rejected");
  std::size_t checks{};
  const auto cancelled = editor::ImportMaterial(Source(), [&] { return ++checks == 8; });
  Require(cancelled.cancelled && !cancelled.material && cancelled.error.empty(),
          "parser cancellation published partial material");
  Require(editor::ImportMaterial(Source(), [] { return true; }).cancelled,
          "pre-parse cancellation ignored");
  struct CommaDecimal final : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
  };
  struct RestoreLocale final {
    std::locale previous;
    ~RestoreLocale() { std::locale::global(previous); }
  };
  const auto classic_source = Source();
  const RestoreLocale restore{std::locale()};
  std::locale::global(std::locale(std::locale::classic(), new CommaDecimal));
  Require(editor::ImportMaterial(classic_source).material.has_value(),
          "material decoding depended on the process numeric locale");
}
void TestBudget(const std::filesystem::path &root) {
  const auto material =
      std::make_shared<const editor::MaterialAsset>(*editor::ImportMaterial(Source()).material);
  std::vector<editor::ContentItem> items;
  for (std::size_t index = 0; index < editor::kMaximumWorkspaceMaterials; ++index)
    items.push_back({{1, index + 1},
                     "Content/" + std::to_string(index) + ".nmaterial",
                     ".nmaterial",
                     "old",
                     editor::ThumbnailState::Ready,
                     {},
                     material});
  const runtime::AssetUuid empty{2, 1};
  items.push_back({empty, "Content/Empty.nmaterial", ".nmaterial", "old"});
  editor::ContentBrowserModel browser;
  Require(browser.Reset(items, 77), "bounded material model failed to open");
  const auto revision = browser.Revision();
  std::string error;
  Require(
      !browser.PublishArtifact(empty, "new", editor::ThumbnailState::Ready, &error, {}, material) &&
          !error.empty() && browser.Revision() == revision &&
          browser.Find(empty)->artifact_hash == "old" && !browser.Find(empty)->material,
      "over-budget material publication changed current data/hash/revision");
  items.back().material = material;
  Require(!browser.Reset(items, 78) && browser.ProjectGeneration() == 77 &&
              browser.Revision() == revision && !browser.Find(empty)->material,
          "over-budget material reset replaced the current model");
  const editor::ContentItem extra{{3, 1},  "Content/Extra.nmaterial",     ".nmaterial",
                                  "extra", editor::ThumbnailState::Ready, {},
                                  material};
  Require(!browser.Discover(extra) && browser.Revision() == revision,
          "over-budget discovery changed the current model");
  Require(browser.Delete(std::array{items.front().id}), "budget fixture delete failed");
  const auto deleted_revision = browser.Revision();
  Require(
      !browser.PublishArtifact(empty, "new", editor::ThumbnailState::Ready, &error, {}, material) &&
          browser.Revision() == deleted_revision && !browser.Find(empty)->material &&
          !browser.Discover(extra) && browser.Undo() &&
          browser.Find(items.front().id)->material == material && !browser.Find(empty)->material,
      "publication/discovery exceeded retained Undo budget or lost deletion Undo");

  const auto sources = root / "MaterialBudgetSources";
  std::filesystem::create_directories(sources);
  const auto source = Source();
  for (std::size_t index = 0; index <= editor::kMaximumWorkspaceMaterials; ++index)
    Write(sources / (std::to_string(index) + ".nmaterial"), source);
  editor::AssetWorkspace workspace;
  Require(workspace.ImportTree(sources), "bounded material workspace indexing failed");
  std::size_t imported{}, failed{};
  for (const auto &entry : workspace.Entries()) {
    if (entry.state == editor::ImportState::Imported) {
      Require(entry.material && !entry.artifact_hash.empty(), "material index lost its payload");
      ++imported;
    } else {
      Require(entry.state == editor::ImportState::Failed && !entry.material &&
                  entry.artifact_hash.empty() && !entry.error.empty(),
              "over-budget workspace entry retained an artifact or partial material");
      ++failed;
    }
  }
  Require(imported == editor::kMaximumWorkspaceMaterials && failed == 1,
          "workspace did not enforce its sorted material asset count budget");
}
void TestProject(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  std::string error;
  Require(workspace.Create(root, "Material Import", &error), "workspace creation failed");
  auto source = root / "Content/Surface.nmaterial";
  Write(source, Source());
  editor::AssetWorkspace assets;
  Require(assets.ImportTree(root / "Content", {}, {},
                            editor::AssetIdentityMode::PersistentReadWrite, &error) &&
              assets.Entries().size() == 1 && assets.Entries().front().material &&
              assets.Entries().front().state == editor::ImportState::Imported,
          "typed persistent material import failed");
  const auto asset = assets.Entries().front().id;
  const auto initial = assets.Entries().front().material;
  core::JobSystem jobs{1};
  jobs.Start();
  editor::AssetImportQueue imports{jobs};
  editor::ProjectContentSession content;
  Require(content.Open(workspace, assets, 7, true, &error), "content opening failed");
  Require(content.Browser().Find(asset)->material == initial, "content lost owned material");
  Require(content.Rename(asset, "Moved.material", &error), "material rename failed");
  source = root / "Content/Moved.material";
  Write(source, Source(.5F));
  Require(content.Reimport(asset, &error) &&
              content.Browser().Find(asset)->material->metallic == .5F && content.Undo(&error) &&
              content.Browser().Find(asset)->material->metallic == .5F && initial->metallic == .25F,
          "sync reimport or rename Undo lost the newer material or mutated a retained snapshot");
  source = root / "Content/Surface.nmaterial";
  Require(content.Rename(asset, "Worker.material"), "worker rename failed");
  source = root / "Content/Worker.material";
  Write(source, Source(.75F));
  Require(content.BeginReimport(imports, asset), "background material reimport failed to start");
  AwaitWorker(content);
  Require(content.Browser().Find(asset)->material->metallic == .5F,
          "worker published material before authoring-thread transaction");
  AwaitPublication(content);
  Require(content.ReimportStatus()->state == editor::ImportOperationState::Succeeded &&
              content.Browser().Find(asset)->material->metallic == .75F && content.Undo() &&
              content.Browser().Find(asset)->material->metallic == .75F,
          "worker publication or rename Undo lost newer material");
  source = root / "Content/Surface.nmaterial";
  const auto published = content.Browser().Find(asset)->material;
  const auto artifact = content.Browser().Find(asset)->artifact_hash;
  const auto revision = content.Browser().Revision();
  Write(source, "NEXORA_MATERIAL 2\n");
  Require(!content.Reimport(asset, &error) &&
              content.Browser().Find(asset)->material == published &&
              content.Browser().Find(asset)->artifact_hash == artifact &&
              content.Browser().Revision() == revision,
          "invalid synchronous reimport replaced last good material");
  Require(content.BeginReimport(imports, asset), "invalid worker request failed to start");
  AwaitPublication(content);
  Require(content.ReimportStatus()->state == editor::ImportOperationState::Failed &&
              content.Browser().Find(asset)->material == published &&
              content.Browser().Find(asset)->artifact_hash == artifact &&
              content.Browser().Revision() == revision,
          "invalid worker replaced last good material");
  Write(source, Source(1));
  Require(content.BeginReimport(imports, asset) && content.CancelReimport(),
          "material cancellation failed");
  AwaitPublication(content);
  Require(content.ReimportStatus()->state == editor::ImportOperationState::Cancelled &&
              content.Browser().Find(asset)->material == published &&
              content.Browser().Find(asset)->artifact_hash == artifact,
          "cancelled worker replaced last good material");
  Require(content.BeginReimport(imports, asset), "stale request failed to start");
  AwaitWorker(content);
  Write(source, Source(.1F) + " \n");
  AwaitPublication(content);
  Require(content.ReimportStatus()->state == editor::ImportOperationState::Stale &&
              content.Browser().Find(asset)->material == published &&
              content.Browser().Find(asset)->artifact_hash == artifact,
          "stale worker replaced last good material");
  {
    std::ofstream oversized(source, std::ios::binary);
    oversized.seekp(editor::kMaximumMaterialSourceBytes);
    oversized.put('x');
  }
  const auto oversized = editor::detail::ReadReimportSource(source, ".NMATERIAL", asset);
  Require(!oversized.material && oversized.source_hash.empty() && oversized.artifact_hash.empty() &&
              oversized.error == "Material source exceeds the 64 KiB limit.",
          "material budget rejection exposed partial hashes/data");
  Require(content.BeginReimport(imports, asset), "oversized request failed to start");
  AwaitPublication(content);
  Require(content.ReimportStatus()->state == editor::ImportOperationState::Failed &&
              content.Browser().Find(asset)->material == published,
          "oversized source replaced last good material");
  Write(source, Source(.75F));
  editor::AssetWorkspace reopened;
  Require(reopened.ImportTree(root / "Content", {}, {},
                              editor::AssetIdentityMode::PersistentReadOnly, &error) &&
              reopened.Entries().front().id == asset &&
              reopened.Entries().front().material->metallic == .75F,
          "reopen lost persistent material identity or typed data");
  Require(content.Delete(std::array{asset}) && !content.Browser().Find(asset) && content.Undo() &&
              content.Browser().Find(asset)->material == published,
          "delete Undo lost last good material");
  imports.Shutdown();
  jobs.Stop();
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-material-import-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    TestParser();
    TestBudget(root);
    TestProject(root);
    std::filesystem::remove_all(root);
    std::cout << "Material import/publication contracts passed\n";
    return 0;
  } catch (const std::exception &failure) {
    std::filesystem::remove_all(root);
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
