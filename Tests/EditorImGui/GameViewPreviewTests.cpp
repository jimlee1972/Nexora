#include "EditorImGuiTestAccess.h"
#include "GameViewPreview.h"
#include "Nexora/EditorImGui/EditorImGui.h"
#include <iostream>
#include <limits>
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
    using namespace editor;
    runtime::World world;
    const auto scene = world.LoadScene("Game");
    Require(world.Activate(scene), "activate failed");
    const auto camera = world.CreateEntity(scene).id;
    const auto mesh = world.CreateEntity(scene).id;
    const auto duplicate = world.CreateEntity(scene).id;
    const auto missing = world.CreateEntity(scene).id;
    const auto inactive = world.LoadScene("Inactive");
    auto &hidden = world.CreateEntity(inactive);
    hidden.camera = true;
    hidden.mesh_renderer = true;
    const runtime::AssetUuid uuid{0x123456789abcdef0, 0xfedcba9876543210};
    const auto imported = ImportObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    Require(imported.geometry.has_value(), "import failed");
    auto geometry = std::make_shared<const MeshGeometry>(*imported.geometry);
    ContentBrowserModel browser;
    Require(browser.Reset(std::array{ContentItem{uuid, "Triangle.obj", ".obj", "old",
                                                 ThumbnailState::Ready, geometry}},
                          7),
            "content reset failed");
    MeshAssetCatalog assets;
    Require(assets.PublishContent(browser), "catalog failed");
    runtime::WorldCommandBuffer commands;
    commands.SetCamera(camera, runtime::CameraComponent{});
    commands.SetTransform(camera, {0, 0, 5});
    commands.SetLight(camera, runtime::LightComponent{2.5F});
    commands.SetParent(duplicate, camera, false);
    commands.SetMeshRenderer(mesh, runtime::MeshComponent{MeshResourceId(uuid), {}});
    commands.SetMeshRenderer(
        duplicate,
        runtime::MeshComponent{MeshResourceId(uuid), {std::numeric_limits<runtime::Id>::max()}});
    commands.SetMeshRenderer(missing, runtime::MeshComponent{99, {}});
    Require(commands.Apply(world), "components failed");
    runtime::PlaySession play(world);
    Require(play.Start(1.0 / 60.0,
                       [mesh](runtime::World &clone, double) {
                         auto pose = clone.FindEntity(mesh)->transform;
                         pose.x += 1;
                         runtime::WorldCommandBuffer move;
                         move.SetTransform(mesh, pose);
                         return move.Apply(clone);
                       }),
            "Play start failed");
    const auto owning = play.Inspect();
    Require(owning.entities.front().id == camera && owning.entities.front().camera_data &&
                owning.entities.front().camera_data->vertical_field_of_view == 60 &&
                owning.entities.front().scene_state == runtime::SceneState::Active &&
                owning.entities.front().parent == 0 && owning.entities.front().light_data &&
                owning.entities.front().light_data->intensity == 2.5F &&
                owning.entities[2].parent == camera && owning.entities[2].world_transform.z == 5 &&
                owning.entities[2].mesh_data->material.shader ==
                    std::numeric_limits<runtime::Id>::max() &&
                owning.entities.back().scene_state == runtime::SceneState::LoadedInactive &&
                owning.entities[1].mesh_data &&
                owning.entities[1].mesh_data->mesh == MeshResourceId(uuid) &&
                !owning.entities[1].camera_data &&
                owning.entities.back().light_data == std::nullopt,
            "owning component payloads or scene metadata failed");
    const MeshAssetCatalog frozen = assets;
    auto frame = preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), frozen, 2);
    Require(frame.camera == camera && frame.instances.size() == 2 && frame.unavailable == 1 &&
                frame.geometry.vertices.size() == 3 && frame.geometry.indices.size() == 3 &&
                frame.batches[0].firstIndex == frame.batches[1].firstIndex,
            "active scenes, missing assets, or shared geometry violated");
    const auto camera_view = runtime::CameraView(*play.PlayWorld(), camera, 2);
    Require(camera_view && frame.view_projection.values == camera_view->view_projection.values,
            "Game camera did not use the runtime camera contract");
    Require(play.Tick() && play.Pause(), "tick/pause failed");
    auto paused = preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), frozen, 2);
    Require(paused.instances[0].translation[0] == 1 && !play.Tick(), "paused pose failed");
    Require(play.Step(), "manual step failed");
    auto stepped = preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), frozen, 2);
    Require(stepped.instances[0].translation[0] == 2 && world.FindEntity(mesh)->transform.x == 0,
            "step escaped the isolated Play World");
    auto replacement = *geometry;
    replacement.vertices[0].position[0] = 9;
    Require(browser.PublishArtifact(uuid, "new", ThumbnailState::Ready, nullptr,
                                    std::make_shared<const MeshGeometry>(replacement)) &&
                assets.PublishContent(browser),
            "reimport publication failed");
    auto retained = preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), frozen, 2);
    auto changed = preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), assets, 2);
    Require(retained.geometry.vertices[0].position[0] == 0 &&
                changed.geometry.vertices[0].position[0] == 9,
            "editor reimport changed frozen Play geometry");
    // A valid runtime double scale can fall below the native float minimum. Omit it instead of
    // rejecting the entire acquired frame in Presentation.
    runtime::WorldCommandBuffer tiny;
    auto tiny_pose = play.PlayWorld()->FindEntity(mesh)->transform;
    tiny_pose.sx = 0.000001;
    tiny.SetTransform(mesh, tiny_pose);
    Require(tiny.Apply(*play.PlayWorld()), "small scale fixture failed");
    const auto bounded = preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), frozen, 2);
    Require(bounded.instances.size() == 1 && bounded.unavailable == 2,
            "unsupported scale reached the native surface");
    imgui::EditorImGuiHost host;
    host.SetDisplay(640, 360, 2);
    ProductShell shell;
    SceneDocument document(world, scene);
    for (int index = 0; index < 4; ++index) {
      host.BeginFrame();
      Require(!host.NativeGameViewport(), "Game viewport survived BeginFrame");
      host.DrawProductShell(shell, &document, nullptr, nullptr, nullptr, nullptr, nullptr, &play);
      static_cast<void>(host.EndFrame());
    }
    const auto viewport = host.NativeGameViewport();
    Require(viewport && viewport->width && viewport->height &&
                viewport->x + viewport->width <= 1280 && viewport->y + viewport->height <= 720 &&
                !host.NativeScenePreviewViewport(),
            "Game tab focus or framebuffer-clipped viewport failed");
    imgui::EditorImGuiTestAccess::SelectPlayEntity(host, mesh);
    host.BeginFrame();
    host.DrawProductShell(shell, &document, nullptr, nullptr, nullptr, nullptr, nullptr, &play);
    static_cast<void>(host.EndFrame());
    Require(imgui::EditorImGuiTestAccess::PlayInspectorEntity(host) == mesh &&
                world.FindEntity(mesh)->transform.x == 0,
            "read-only Play selection changed the Editor World");
    imgui::EditorImGuiTestAccess::SelectPlayEntity(host, std::numeric_limits<runtime::Id>::max());
    host.BeginFrame();
    host.DrawProductShell(shell, &document, nullptr, nullptr, nullptr, nullptr, nullptr, &play);
    static_cast<void>(host.EndFrame());
    Require(imgui::EditorImGuiTestAccess::PlayInspectorEntity(host) == 0,
            "missing Play entity retained stale inspection");
    play.SetInputFocus(true);
    Require(play.ReportRuntimeFailure() && play.State() == runtime::PlayState::Paused &&
                play.LastPauseReason() == runtime::PauseReason::RuntimeFailure &&
                !play.AcceptsInput() && play.Stats().crashes == 1,
            "embedding callback failure did not pause and revoke input");
    host.SetNativeGameStatus("unsupported", false);
    host.BeginFrame();
    host.DrawProductShell(shell, &document, nullptr, nullptr, nullptr, nullptr, nullptr, &play);
    static_cast<void>(host.EndFrame());
    Require(!host.NativeGameViewport(), "unsupported backend retained native Game canvas");
    runtime::WorldCommandBuffer remove_camera;
    remove_camera.SetCamera(camera, std::nullopt);
    Require(remove_camera.Apply(*play.PlayWorld()), "camera removal failed");
    auto no_camera = preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), frozen, 2);
    Require(!no_camera.camera && no_camera.instances.empty(), "missing camera did not fail closed");
    Require(play.Stop() && !play.PlayWorld(), "Stop retained the clone");
    Require(owning.entities.front().camera_data &&
                owning.entities[1].mesh_data->mesh == MeshResourceId(uuid) &&
                !play.ReportRuntimeFailure(),
            "snapshot did not survive component removal/Stop or stopped failure accepted");
    host.BeginFrame();
    host.DrawProductShell(shell, &document, nullptr, nullptr, nullptr, nullptr, nullptr, &play);
    static_cast<void>(host.EndFrame());
    Require(imgui::EditorImGuiTestAccess::PlayInspectorEntity(host) == 0,
            "Stop retained Play inspector state");
    const auto draw = stepped.DrawData({1, 2, 400, 200});
    Require(draw.vertices.size() == 3 && draw.instances[0].translation[0] == 2 &&
                Nexora::Presentation::ValidateSceneMeshBatches(draw.batches, draw.indices.size(),
                                                               draw.instances.size()),
            "frame data borrowed destroyed Play World or invalid ranges");
    std::cout << "Native Game View isolation contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
