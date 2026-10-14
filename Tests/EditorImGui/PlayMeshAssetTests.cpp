#include "GameViewPreview.h"
#include "PlayGameplayModule.h"
#include <bit>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
constexpr runtime::AssetUuid identity{0x123456789abcdef0ULL, 0xfedcba9876543210ULL};
constexpr std::uint64_t resource = 0xb0f7765a695fb756ULL;
static_assert(editor::MeshResourceId(identity) == resource);
NexoraGameplayHostV3 host{};
bool expect_assets{}, fail_start{}, stop_resolved{}, destroy_resolved{};
NexoraAssetHandle created{}, started{};
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
int32_t Resolve(NexoraAssetHandle &out) {
  return host.resolve_asset ? host.resolve_asset(host.context, identity.high, identity.low, &out)
                            : NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
}
int32_t Create(void **state, const NexoraGameplayHostV3 *api) {
  host = *api;
  *state = nullptr;
  return expect_assets ? Resolve(created) : NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *) {
  if (expect_assets && Resolve(started) != NEXORA_GAMEPLAY_OK)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  return fail_start ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : NEXORA_GAMEPLAY_OK;
}
int32_t Update(void *, double) { return NEXORA_GAMEPLAY_OK; }
void Stop(void *) {
  NexoraAssetHandle out{};
  stop_resolved = !expect_assets || (Resolve(out) == NEXORA_GAMEPLAY_OK && out.value == resource);
}
void Destroy(void *) {
  NexoraAssetHandle out{};
  destroy_resolved =
      !expect_assets || (Resolve(out) == NEXORA_GAMEPLAY_OK && out.value == resource);
}
int32_t Loader(std::uint32_t version, NexoraGameModuleV3 *module) {
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = version;
  module->create = Create;
  module->on_start = Start;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}
} // namespace
int main() {
  try {
    using namespace nexora;
    using namespace editor;
    const auto old_mesh = ImportObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    const auto new_mesh = ImportObjMesh("v 9 0 0\nv 10 0 0\nv 9 1 0\nf 1 2 3\n");
    Require(old_mesh.geometry && new_mesh.geometry, "mesh fixtures failed");
    AssetEntry entry;
    entry.id = identity;
    entry.relative_path = "Triangle.obj";
    entry.state = ImportState::Imported;
    MeshAssetCatalog authoring;
    runtime::World source;
    const auto scene = source.LoadScene("Source");
    Require(source.Activate(scene), "source scene activation failed");
    SceneDocument document(source, scene);
    const auto camera = document.Create("Camera");
    Require(document.SetCamera(*document.Key(camera), runtime::CameraComponent{}) &&
                document.SetTransform(camera, {0, 0, 5}),
            "source camera failed");
    const auto baseline = source.SaveScene(scene);
    const auto dirty = document.Dirty();
    runtime::PlaySession play(source);
    preview::PlayGameplayModule module;
    Require(!preview::FreezeGameMeshes(authoring, 0), "unbound catalog was frozen");
    for (std::uint64_t cycle = 1; cycle <= 32; ++cycle) {
      entry.id = identity;
      entry.mesh = std::make_shared<const MeshGeometry>(*old_mesh.geometry);
      Require(authoring.Publish(std::span{&entry, 1}, cycle), "mesh publication failed");
      auto frozen = preview::FreezeGameMeshes(authoring, cycle);
      Require(frozen && !preview::FreezeGameMeshes(authoring, cycle + 1),
              "snapshot generation validation failed");
      Require(!module.Load(source, Loader, frozen), "asset services admitted an Editor World");
      Require(play.Start(0.25, [](runtime::World &, double) { return true; }), "Play start failed");
      expect_assets = true;
      fail_start = false;
      Require(module.Load(*play.PlayWorld(), Loader, frozen) && created.value == resource &&
                  started.value == resource,
              "Create/Start did not receive exact frozen mesh handles");
      Require(host.resolve_asset && !host.debug_draw_line, "asset service availability incorrect");
      NexoraFrameDiagnostics before_errors{};
      Require(host.get_diagnostics &&
                  host.get_diagnostics(host.context, &before_errors) == NEXORA_GAMEPLAY_OK,
              "mesh resolver diagnostics unavailable");
      const auto reject = [&](runtime::AssetUuid id, int32_t expected) {
        NexoraAssetHandle out{UINT64_MAX};
        Require(host.resolve_asset(host.context, id.high, id.low, &out) == expected &&
                    out.value == UINT64_MAX,
                "rejected asset request changed output/result");
      };
      reject({}, NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT);
      reject({4, 7}, NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT);
      auto collision = identity;
      collision.high ^= 1;
      collision.low ^= std::rotl(std::uint64_t{1}, 23);
      Require(MeshResourceId(collision) == resource, "collision fixture failed");
      reject(collision, NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT);
      Require(host.resolve_asset(host.context, identity.high, identity.low, nullptr) ==
                  NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
              "null output accepted");
      NexoraFrameDiagnostics after_errors{};
      Require(host.get_diagnostics(host.context, &after_errors) == NEXORA_GAMEPLAY_OK &&
                  after_errors.api_errors == before_errors.api_errors + 4,
              "asset request failures escaped the service-error counter");
      NexoraAssetHandle handle{};
      Require(Resolve(handle) == NEXORA_GAMEPLAY_OK && handle.value == resource,
              "owning handle changed");
      NexoraEntitySpawnDescriptor descriptor{};
      descriptor.struct_size = sizeof(descriptor);
      descriptor.components = NEXORA_SPAWN_MESH;
      descriptor.mesh = handle;
      std::uint64_t entity{};
      Require(host.spawn_entity(host.context, scene, &descriptor, &entity) == NEXORA_GAMEPLAY_OK &&
                  play.PlayWorld()->FindEntity(entity)->mesh_data.mesh == resource,
              "resolved handle could not spawn a real Play mesh");
      authoring.Clear();
      Require(Resolve(handle) == NEXORA_GAMEPLAY_OK && handle.value == resource,
              "authoring delete invalidated a Play binding");
      entry.mesh = std::make_shared<const MeshGeometry>(*new_mesh.geometry);
      Require(authoring.Publish(std::span{&entry, 1}, cycle + 100), "reimport failed");
      const auto frame = preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), *frozen, 2);
      const auto changed = preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), authoring, 2);
      Require(frame.instances.size() == 1 && frame.geometry.vertices.size() == 3 &&
                  frame.geometry.vertices[0].position[0] == 0 &&
                  changed.geometry.vertices[0].position[0] == 9 &&
                  Resolve(handle) == NEXORA_GAMEPLAY_OK && handle.value == resource,
              "callback/renderer did not share the frozen source revision");
      entry.id = {9, cycle};
      Require(authoring.Publish(std::span{&entry, 1}, cycle + 200) &&
                  Resolve(handle) == NEXORA_GAMEPLAY_OK && handle.value == resource,
              "same-count project asset replacement changed an existing binding");
      std::weak_ptr<const MeshAssetCatalog> retained = frozen;
      frozen.reset();
      Require(!retained.expired(), "module did not own its catalog");
      stop_resolved = destroy_resolved = false;
      module.Unload();
      Require(stop_resolved && destroy_resolved && retained.expired(),
              "catalog did not survive callbacks or was retained after teardown");
      reject(identity, NEXORA_GAMEPLAY_ERROR_LIFECYCLE);
      expect_assets = false;
      Require(module.Load(*play.PlayWorld(), Loader), "provider-free load failed");
      reject(identity, NEXORA_GAMEPLAY_ERROR_UNSUPPORTED);
      Require(host.get_diagnostics(host.context, &after_errors) == NEXORA_GAMEPLAY_OK &&
                  after_errors.api_errors == 1,
              "missing-provider failure was not counted");
      module.Unload();
      Require(!module.Load(*play.PlayWorld(), Loader, std::make_shared<const MeshAssetCatalog>()),
              "invalid catalog generation bound");
      entry.id = identity;
      Require(authoring.Publish(std::span{&entry, 1}, cycle + 300),
              "failure fixture publish failed");
      frozen = preview::FreezeGameMeshes(authoring, cycle + 300);
      const std::weak_ptr<const MeshAssetCatalog> failed_snapshot = frozen;
      expect_assets = fail_start = true;
      destroy_resolved = false;
      Require(!module.Load(*play.PlayWorld(), Loader, frozen) && destroy_resolved &&
                  !module.IsLoaded(),
              "failed Start lost catalog before Destroy");
      frozen.reset();
      Require(failed_snapshot.expired(), "failed load retained its catalog");
      reject(identity, NEXORA_GAMEPLAY_ERROR_LIFECYCLE);
      fail_start = false;
      Require(play.Stop() && source.SaveScene(scene) == baseline && document.Dirty() == dirty &&
                  frame.geometry.vertices[0].position[0] == 0,
              "Play asset lifecycle changed source/history or retained frame data");
      Require(document.Undo() && source.FindEntity(camera)->transform.z == 0 && document.Redo() &&
                  source.SaveScene(scene) == baseline,
              "asset resolution changed the authoring Undo cursor");
    }
    std::cout << "Frozen Play mesh assets: 32 Play cycles, startup/failure/unload and renderer "
                 "isolation passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
