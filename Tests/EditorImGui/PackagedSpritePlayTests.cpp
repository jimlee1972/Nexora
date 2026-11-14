#include "../Runtime/GameSceneFixture.h"
#include "Nexora/Editor/SpriteSourceCatalog.h"
#include "PackagedPlayContext.h"
#include "PackagedSpriteTextures.h"
#include <iostream>

namespace {
using namespace nexora;
using Nexora::Presentation::SurfaceStatus;
constexpr runtime::AssetUuid atlasId{101, 1}, sourceId{102, 1};
void Check(bool value, const char *text) {
  if (!value)
    throw std::runtime_error(text);
}
runtime::GameProjectPackage Package() {
  auto package = testscene::Package();
  const twod::Image image{1, 1, 1, {std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}}};
  const auto atlas = twod::PackAtlas(atlasId, std::span(&image, 1), 8, 8, true);
  Check(bool(atlas), "fixture Atlas");
  twod::SpriteAsset source;
  source.id = sourceId;
  source.atlas = atlasId;
  source.region = 1;
  source.sourceWidth = source.sourceHeight = source.trimWidth = source.trimHeight = 1;
  source.pixelsPerUnit = 1;
  package.assets.push_back(testscene::Cook(atlasId, "sprite_atlas", twod::EncodeAtlas(*atlas)));
  package.assets.push_back(
      testscene::Cook(sourceId, "sprite_source", twod::EncodeSpriteAsset(source), {atlasId}));
  for (unsigned scene_index = 0; scene_index < 2; ++scene_index) {
    auto blob = testscene::Scene(scene_index ? testscene::kLevel : testscene::kTitle,
                                 scene_index ? "Level01" : "Title", scene_index ? -2 : 2, 0);
    const auto decoded = runtime::DecodeCookedScene(blob.payload);
    Check(bool(decoded), "fixture Scene decode");
    auto scene = decoded.Value();
    const auto reference = editor::SpriteSourceReference(sourceId);
    const auto bytes = std::as_bytes(std::span(reference.data));
    scene.opaque.push_back({2, reference.type, reference.type_name, {bytes.begin(), bytes.end()}});
    auto dependencies = blob.dependencies;
    dependencies.push_back(sourceId);
    package.assets[scene_index] =
        testscene::Cook(blob.id, runtime::kCookedSceneType,
                        runtime::EncodeCookedScene(scene).Value(), dependencies);
  }
  return package;
}
std::shared_ptr<const runtime::LoadedGameProject> Load(const runtime::GameProjectPackage &package) {
  std::string error;
  auto loaded = runtime::LoadGameProjectPackage(package, &error);
  Check(bool(loaded), error.c_str());
  return std::make_shared<const runtime::LoadedGameProject>(std::move(*loaded));
}
void Lifecycle() {
  const auto package = Load(Package());
  editor::imgui::EditorImGuiHost ui;
  std::uint64_t previous_texture{};
  for (unsigned cycle = 0; cycle < 32; ++cycle) {
    editor::preview::PackagedPlayContext context(package);
    Check(context.HasSprites() && !context.Ready() && context.Commits() == 0,
          "candidate published before presentation");
    editor::preview::PackagedSpriteTextures textures(ui);
    Check(textures.Prepare(*package, context.SpriteAssets()) && textures.Size() == 1,
          "whole package texture admission");
    Check(context.Render({0, 0, 640, 480}, false, [](const auto &) { return SurfaceStatus::Ready; })
                      .status == SurfaceStatus::Unsupported &&
              !context.Ready(),
          "source package silently used the legacy renderer");
    const auto present = [&](const auto &data, const twod::SpriteFrame &frame) {
      Check(!data.instances.empty() && frame.statistics.unavailable == 0,
            "mixed candidate palette");
      const auto mapped = textures.Remap(frame);
      Check(mapped && !mapped->batches.empty(), "native texture remapping");
      auto foreign = frame;
      foreign.retainedAtlases[0].atlas =
          std::make_shared<const twod::Atlas>(*foreign.retainedAtlases[0].atlas);
      Check(!textures.Remap(foreign), "foreign same-UUID Atlas owner passed frame admission");
      return SurfaceStatus::Ready;
    };
    const auto first =
        context.RenderComposed({0, 0, 640, 480}, false, [&](const auto &data, const auto &frame) {
          Check(!context.Ready() && context.Commits() == 0 &&
                    context.Scenes().View().instances.empty(),
                "candidate World published before complete draw");
          const auto mapped = textures.Remap(frame);
          Check(mapped && mapped->batches[0].textureId != previous_texture,
                "restart reused a stale public native texture generation");
          previous_texture = mapped->batches[0].textureId;
          return present(data, frame);
        });
    Check(first.committed && first.visible_sprites == 1 && context.Commits() == 1,
          "startup composition");
    const auto primary = context.Scenes().View().primary;
    const auto before = context.World().InternalWorld().SaveScene(primary);
    std::uint64_t request{};
    for (const bool throwing : {false, true}) {
      Check(context.Scenes().RequestLoad(testscene::kTitle, runtime::GameSceneLoadMode::Additive,
                                         false, request) == runtime::GameSceneError::None,
            "additive request");
      unsigned calls{};
      const auto rejected =
          context.RenderComposed({0, 0, 640, 480}, false, [&](const auto &data, const auto &frame) {
            Check(context.Commits() == 1 && context.Scenes().View().primary == primary &&
                      context.World().InternalWorld().SaveScene(primary) == before,
                  "unpublished native candidate changed the live World");
            if (++calls == 1) {
              if (throwing)
                throw std::bad_alloc{};
              return SurfaceStatus::InvalidDescriptor;
            }
            return present(data, frame);
          });
      Check(rejected.rejected && !rejected.committed && rejected.drawn && calls == 2 &&
                context.Commits() == 1 && context.Scenes().Stats().instances == 1,
            "failed composition did not retain/redraw the old World");
    }
    Check(context.Scenes().RequestLoad(testscene::kTitle, runtime::GameSceneLoadMode::Additive,
                                       false, request) == runtime::GameSceneError::None,
          "accepted additive request");
    const auto additive = context.RenderComposed({0, 0, 640, 480}, false, present);
    const auto view = context.Scenes().View();
    Check(additive.committed && additive.visible_sprites == 2 && view.instances.size() == 2 &&
              view.instances[0].entities != view.instances[1].entities,
          "source extraction lost additive remapping");
    Check(context.Scenes().RequestLoad(testscene::kTitle, runtime::GameSceneLoadMode::Replace,
                                       false, request) == runtime::GameSceneError::None,
          "replace request");
    const auto replaced = context.RenderComposed({0, 0, 640, 480}, false, present);
    Check(replaced.committed && replaced.visible_sprites == 1 && context.Commits() == 3 &&
              context.Scenes().Stats().instances == 1,
          "Replace retained retired source instances");
  }
  // A failed new owner cannot revoke registrations retained by another view/session.
  const std::array pixels{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}};
  std::vector<std::uint64_t> held;
  for (unsigned slot = 0; slot < 64; ++slot) {
    held.push_back(ui.RegisterNativeTexture(1, 1, pixels));
    Check(held.back() != 0, "preceding preview leaked native registrations");
  }
  {
    editor::preview::PackagedPlayContext context(package);
    editor::preview::PackagedSpriteTextures rejected(ui);
    Check(!rejected.Prepare(*package, context.SpriteAssets()) && !context.Ready(),
          "capacity failure published a new owner");
  }
  for (const auto id : held)
    Check(ui.UnregisterTexture(id), "failed admission revoked the retained owner");
}
void Malformed() {
  auto package = Package();
  auto bad = package.assets.back();
  auto source = twod::DecodeSpriteAsset(bad.payload);
  Check(bool(source), "fixture source");
  source->trimWidth = 2; // NXAB remains structurally valid; typed Atlas closure must reject.
  source->sourceWidth = 2;
  package.assets.back() =
      testscene::Cook(sourceId, "sprite_source", twod::EncodeSpriteAsset(*source), {atlasId});
  const auto loaded = Load(package);
  bool rejected{};
  try {
    editor::preview::PackagedPlayContext context(loaded);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  Check(rejected, "typed malformed source admitted before Play");
}
} // namespace
int main() {
  try {
    Lifecycle();
    Malformed();
    std::cout
        << "Packaged Sprite complete admission, deferred World publication, retained rollback, "
           "Additive/Replace and 32 owner retirements passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
