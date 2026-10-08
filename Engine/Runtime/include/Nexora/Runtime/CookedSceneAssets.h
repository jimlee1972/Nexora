#pragma once

#include "Nexora/Foundation/Types.h"
#include "Nexora/Renderer/Material.h"
#include "Nexora/Renderer/SceneFrame.h"
#include "Nexora/Runtime/AssetPipeline.h"
#include "Nexora/Runtime/Runtime.h"

#include <array>
#include <bit>

namespace nexora::runtime {

inline constexpr std::string_view kCookedMeshType = "nexora.mesh.v1";
inline constexpr std::string_view kCookedScalarPbrType = "nexora.scalar-pbr.v1";
inline constexpr std::string_view kCookedSceneType = "nexora.scene.v1";
inline constexpr std::size_t kCookedMeshMaximumVertices = 65535;
inline constexpr std::size_t kCookedMeshMaximumIndices = 1048576;
inline constexpr std::size_t kCookedGeometryMaximumBytes = 128 * 1024 * 1024;
inline constexpr std::size_t kCookedSceneMaximumSnapshotBytes = 64 * 1024 * 1024;
inline constexpr std::size_t kCookedSceneMaximumEntities = 100000;
inline constexpr std::size_t kCookedOpaqueMaximumRecordBytes = 1024 * 1024;
inline constexpr std::size_t kCookedOpaqueMaximumBytes = 16 * 1024 * 1024;
inline constexpr std::size_t kCookedOpaqueMaximumRecords = 4096;
inline constexpr std::size_t kCookedOpaqueMaximumPerEntity = 64;
inline constexpr std::size_t kCookedOpaqueMaximumNameBytes = 256;

// Persistent identity, identical to the existing Editor derivation. Zero UUID is invalid.
// A package must still reject distinct UUIDs that collide in this 64-bit resource identity.
[[nodiscard]] constexpr std::uint64_t MeshResourceId(AssetUuid asset) noexcept {
  if (asset.high == 0 && asset.low == 0)
    return 0;
  auto value = std::rotl(asset.high, 23) ^ asset.low;
  value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
  value ^= value >> 31;
  return value == 0 ? 1 : value;
}

struct CookedMeshVertex final {
  std::array<float, 3> position{};
  std::array<float, 3> normal{};
  std::array<float, 2> uv{};
  std::array<float, 4> tangent{};
};

struct CookedMesh final {
  std::vector<CookedMeshVertex> vertices;
  std::vector<std::uint16_t> indices;
};

struct CookedScalarPbr final {
  std::array<float, 3> base_color{1, 1, 1};
  std::array<float, 3> emission{};
  float metallic{};
  float roughness{0.5F};
  float occlusion{1};
};

struct SceneAssetBinding final {
  Id entity{};
  AssetUuid mesh;
  std::optional<AssetUuid> material;
  std::uint64_t mesh_resource{};
};

struct PreservedOpaqueRecord final {
  Id entity{};
  std::uint64_t type{};
  std::string type_name;
  ByteBuffer data;
};

struct CookedScene final {
  std::string world_snapshot;
  std::vector<SceneAssetBinding> bindings;
  std::vector<PreservedOpaqueRecord> opaque;
};

// Schema-1 little-endian codecs. Inputs are borrowed only for the call; results own all storage.
// Invalid/unsupported/truncated/nonfinite/oversized content returns InvalidArgument. No I/O,
// publication, GPU work, source lookup or unknown-component execution occurs here.
[[nodiscard]] NEXORA_RUNTIME_API foundation::Result<ByteBuffer>
EncodeCookedMesh(const CookedMesh &mesh);
[[nodiscard]] NEXORA_RUNTIME_API foundation::Result<CookedMesh>
DecodeCookedMesh(std::span<const std::byte> bytes);
[[nodiscard]] NEXORA_RUNTIME_API foundation::Result<ByteBuffer>
EncodeCookedScalarPbr(const CookedScalarPbr &material);
[[nodiscard]] NEXORA_RUNTIME_API foundation::Result<CookedScalarPbr>
DecodeCookedScalarPbr(std::span<const std::byte> bytes);
[[nodiscard]] NEXORA_RUNTIME_API foundation::Result<ByteBuffer>
EncodeCookedScene(const CookedScene &scene);
[[nodiscard]] NEXORA_RUNTIME_API foundation::Result<CookedScene>
DecodeCookedScene(std::span<const std::byte> bytes);

// Owning validated CPU adapters to the existing Renderer contract. Tangents remain in CookedMesh;
// scalar PBR emits a fixed opaque profile with no textures and never interprets a legacy shader ID.
[[nodiscard]] NEXORA_RUNTIME_API foundation::Result<renderer::Mesh>
ToRendererMesh(const CookedMesh &mesh);
[[nodiscard]] NEXORA_RUNTIME_API foundation::Result<renderer::MaterialSchema>
ToRendererMaterial(const CookedScalarPbr &material);

} // namespace nexora::runtime
