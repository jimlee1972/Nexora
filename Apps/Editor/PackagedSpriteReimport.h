#pragma once
#include "Nexora/Editor/GameBuildCapture.h"
#include "Nexora/Editor/ProjectTwoDPlayAssets.h"
#include "PackagedPlayContext.h"
#include "PackagedSpriteTextures.h"
#include <algorithm>
#include <set>

namespace nexora::editor::preview {
// Observes published Content generations, not unimported files. All capture/admission calls
// serialize after gameplay dispatch on the host thread; no authoring/World borrow escapes.
class PackagedSpriteReimport final {
public:
  struct Candidate final {
    std::shared_ptr<const ProjectTwoDPlayAssets> provenance;
    std::unique_ptr<twod::GameSpriteSession::PreparedSources> sources;
    std::unique_ptr<PackagedSpriteTextures> textures;
    std::vector<std::shared_ptr<const TwoDAssetPayload>> items;
  };
  PackagedSpriteReimport(const runtime::LoadedGameProject &project,
                         const ProjectContentSession &content,
                         std::vector<GameBuildSource> baselines)
      : project_(project.ProjectId()), root_(content.Root()),
        project_generation_(content.Browser().ProjectGeneration()),
        observed_revision_(content.Browser().Revision()) {
    std::set<std::filesystem::path> dynamic;
    for (const auto &blob : project.SpriteAssets()) {
      const auto *item = content.Browser().Find(blob.id);
      if (!item || !item->twod)
        throw std::invalid_argument("Configured Sprite reimport has no imported source scope.");
      roots_.push_back(blob.id);
      types_.push_back(blob.type);
      items_.push_back(item->twod);
      dynamic.insert(item->path);
      dynamic.insert(AssetWorkspace::IdentitySidecar(item->path));
    }
    std::erase_if(baselines, [&](const auto &source) { return dynamic.contains(source.relative); });
    scope_ = std::move(baselines);
  }
  [[nodiscard]] std::uint64_t Accepted() const noexcept { return accepted_; }
  [[nodiscard]] std::unique_ptr<Candidate> Stage(const ProjectWorkspace &workspace,
                                                 const ProjectContentSession &content,
                                                 PackagedPlayContext &play,
                                                 const PackagedSpriteTextures &textures,
                                                 imgui::EditorImGuiHost &ui, std::string &error) {
    error.clear();
    if (!play.Ready() || content.ReimportBusy() ||
        observed_revision_ == content.Browser().Revision())
      return {};
    observed_revision_ = content.Browser().Revision();
    bool changed = false;
    for (std::size_t i = 0; i < roots_.size(); ++i) {
      const auto *item = content.Browser().Find(roots_[i]);
      changed = changed || !item || item->twod != items_[i];
    }
    if (!changed)
      return {};
    if (!ScopeCurrent(workspace, content, error) ||
        accepted_ == std::numeric_limits<std::uint64_t>::max())
      return {};
    auto provenance = ProjectTwoDPlayAssets::Capture(workspace, content, project_generation_,
                                                     accepted_ + 1, roots_, true, &error);
    if (!provenance)
      return {};
    if (provenance->Assets()->Size() != roots_.size()) {
      error = "Sprite reimport changed the package asset closure; restart Play.";
      return {};
    }
    runtime::AssetCooker cooker;
    runtime::DerivedDataCache cache;
    std::vector<runtime::RuntimeBlob> blobs;
    auto candidate = std::make_unique<Candidate>();
    candidate->provenance = std::move(provenance);
    for (std::size_t i = 0; i < roots_.size(); ++i) {
      runtime::CanonicalAsset asset;
      asset.id = roots_[i];
      asset.type = types_[i];
      const auto snapshot = candidate->provenance->Assets();
      if (const auto source = snapshot->SpriteSource(asset.id)) {
        asset.payload = twod::EncodeSpriteAsset(*source);
        asset.dependencies = {source->atlas};
      } else if (const auto atlas = snapshot->Atlas(asset.id)) {
        asset.payload = twod::EncodeAtlas(*atlas);
      } else if (const auto clip = snapshot->Clip(asset.id)) {
        asset.payload = twod::EncodeClip(*clip);
        asset.dependencies = twod::ClipDependencies(*clip);
      }
      auto blob = asset.payload.empty() ? std::nullopt
                                        : cooker.Cook(asset, "portable", "game-sprite-v1", cache);
      const auto *item = content.Browser().Find(asset.id);
      if (!blob || !item || !item->twod) {
        error = "Sprite reimport has an incomplete or changed typed source inventory.";
        return {};
      }
      blobs.push_back(std::move(*blob));
      candidate->items.push_back(item->twod);
    }
    candidate->sources = play.PrepareSourceUpdate(blobs, &error);
    if (!candidate->sources)
      return {};
    candidate->textures = std::make_unique<PackagedSpriteTextures>(ui);
    if (!candidate->textures->Prepare(blobs, candidate->sources->Assets(), &textures)) {
      error = "Sprite reimport exceeds current preview texture capacity; retained the live assets.";
      return {};
    }
    return candidate;
  }
  using Draw = std::function<Nexora::Presentation::SurfaceStatus(
      const Nexora::Presentation::SceneDrawData &, const twod::SpriteFrame &,
      const PackagedSpriteTextures &)>;
  PackagedPlayContext::Frame Present(Candidate &candidate, const ProjectWorkspace &workspace,
                                     const ProjectContentSession &content,
                                     PackagedPlayContext &play,
                                     std::unique_ptr<PackagedSpriteTextures> &textures,
                                     Nexora::Presentation::SceneViewport viewport,
                                     bool retain_geometry, const Draw &draw) {
    std::string error;
    auto result = play.RenderSourceUpdate(
        *candidate.sources, viewport, retain_geometry, [&](const auto &scene, const auto &sprites) {
          // Last cooperative source/scope recheck occurs after CPU extraction and before any
          // native recording or EndFrame. An obsolete candidate never reaches the GUI packet.
          if (!ScopeCurrent(workspace, content, error) ||
              !candidate.provenance->Matches(workspace, content, true, &error))
            return Nexora::Presentation::SurfaceStatus::InvalidDescriptor;
          return draw(scene, sprites, *candidate.textures);
        });
    if (result.drawn) {
      // Infallible publication follows the successful exact native frame, without callbacks.
      items_.swap(candidate.items);
      textures.swap(candidate.textures);
      ++accepted_;
    } else if (!error.empty()) {
      result.error = std::move(error);
    }
    candidate.sources.reset();
    return result;
  }

private:
  bool ScopeCurrent(const ProjectWorkspace &workspace, const ProjectContentSession &content,
                    std::string &error) const {
    if (runtime::AssetUuid{workspace.Project().id.high, workspace.Project().id.low} != project_ ||
        workspace.Root() != root_ || content.Root() != root_ ||
        content.Browser().ProjectGeneration() != project_generation_) {
      error = "Sprite reimport belongs to an obsolete project or Play owner.";
      return false;
    }
    return GameBuildSourcesCurrent(root_, scope_, &error);
  }
  runtime::AssetUuid project_;
  std::filesystem::path root_;
  std::uint64_t project_generation_{}, observed_revision_{}, accepted_{};
  std::vector<runtime::AssetUuid> roots_;
  std::vector<std::string> types_;
  std::vector<std::shared_ptr<const TwoDAssetPayload>> items_;
  std::vector<GameBuildSource> scope_;
};
} // namespace nexora::editor::preview
