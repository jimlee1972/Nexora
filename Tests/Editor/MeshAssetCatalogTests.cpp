#include "Nexora/Editor/MeshAssetCatalog.h"

#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
} // namespace

int main() {
  try {
    using namespace nexora;
    constexpr runtime::AssetUuid identity{0x123456789abcdef0ULL, 0xfedcba9876543210ULL};
    static_assert(editor::MeshResourceId(identity) == 0xb0f7765a695fb756ULL);
    static_assert(editor::MeshResourceId({}) == 0);
    const auto parsed = editor::ImportObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    Require(parsed.geometry.has_value(), "fixture import failed");
    auto geometry = std::make_shared<const editor::MeshGeometry>(*parsed.geometry);
    editor::AssetEntry asset;
    asset.id = identity;
    asset.relative_path = "Shapes/Triangle.obj";
    asset.state = editor::ImportState::Imported;
    asset.mesh = geometry;
    editor::MeshAssetCatalog catalog;
    std::string error;
    Require(catalog.Publish(std::span{&asset, 1}, 7, &error) && error.empty(),
            "mesh publication failed");
    const auto retained = catalog.ResolveAsset(identity, 7);
    Require(retained && retained->geometry == geometry &&
                catalog.ResolveResource(retained->resource, 7)->asset == identity &&
                !catalog.ResolveAsset(identity, 6) && !catalog.ResolveAsset(identity, 0) &&
                !catalog.ResolveResource(0, 7),
            "owning resolution or project generation check failed");
    asset.relative_path = "Moved/Renamed.obj";
    Require(catalog.Publish(std::span{&asset, 1}, 8) &&
                catalog.ResolveAsset(identity, 8)->resource == retained->resource &&
                !catalog.ResolveResource(retained->resource, 7),
            "rename or generation replacement changed persistent identity");
    // The fold of 128 UUID bits into 64 bits cannot be injective. A deliberate collision must
    // reject the whole candidate and preserve the previous publication, never pick a winner.
    auto collision = asset;
    collision.id.high ^= 1;
    collision.id.low ^= std::rotl(std::uint64_t{1}, 23);
    Require(editor::MeshResourceId(collision.id) == retained->resource,
            "collision fixture is invalid");
    const std::array conflicting{asset, collision};
    Require(!catalog.Publish(conflicting, 9, &error) && !error.empty() &&
                catalog.Generation() == 8 && catalog.ResolveAsset(identity, 8) &&
                !catalog.ResolveAsset(collision.id, 8),
            "identity collision published partial or aliased geometry");
    Require(!catalog.Publish(std::span{&asset, 1}, 0, &error) && catalog.Generation() == 8,
            "zero generation replaced the live catalog");
    asset.id = {};
    Require(!catalog.Publish(std::span{&asset, 1}, 9), "zero UUID was accepted");
    asset.id = identity;
    asset.state = editor::ImportState::Failed;
    Require(catalog.Publish(std::span{&asset, 1}, 9) &&
                !catalog.ResolveResource(retained->resource, 9),
            "failed import remained assignable");
    asset.state = editor::ImportState::Imported;
    asset.mesh.reset();
    Require(catalog.Publish(std::span{&asset, 1}, 10) && !catalog.ResolveAsset(identity, 10),
            "missing CPU payload remained assignable");
    catalog.Clear();
    geometry.reset();
    Require(catalog.Generation() == 0 && !catalog.ResolveResource(retained->resource, 10) &&
                retained->geometry->indices == std::vector<std::uint16_t>{0, 1, 2},
            "unload invalidated an owning geometry snapshot");
    std::cout << "Mesh asset catalog contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
