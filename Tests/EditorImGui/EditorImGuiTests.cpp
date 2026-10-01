#include "EditorImGuiTestAccess.h"
#include "Nexora/EditorImGui/EditorImGui.h"

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <thread>
#include <vector>

int main() {
  using nexora::editor::imgui::EditorImGuiTestAccess;
  nexora::editor::imgui::EditorImGuiHost host;
  const auto initial_state = EditorImGuiTestAccess::Inspect(host);
  assert(!initial_state.platform_viewports_enabled);
  assert(initial_state.keyboard_navigation_enabled);
  assert(initial_state.input_trickle_enabled);
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Editor ImGui contract");
  assert(world.Activate(scene_id));
  nexora::editor::SceneDocument scene(world, scene_id);
  const auto root = scene.Create("Scene Root");
  assert(root != 0 && scene.Nodes().size() == 1);
  const auto content_root =
      std::filesystem::temp_directory_path() /
      ("nexora-imgui-content-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  nexora::editor::ProjectWorkspace content_workspace;
  std::string content_error;
  assert(content_workspace.Create(content_root, "Content", &content_error));
  std::ofstream(content_root / "Content/Hero.mesh") << "mesh";
  std::ofstream(content_root / "Content/Hero.material") << "material";
  nexora::editor::AssetWorkspace content_assets;
  assert(content_assets.ImportTree(content_root / "Content", {}, {},
                                   nexora::editor::AssetIdentityMode::PersistentReadWrite,
                                   &content_error));
  nexora::core::JobSystem import_jobs{1};
  import_jobs.Start();
  nexora::editor::AssetImportQueue imports{import_jobs};
  nexora::editor::ProjectContentSession content;
  assert(content.Open(content_workspace, content_assets, 3, true, &content_error));
  nexora::editor::RecentProjectStore recent_projects;
  assert(recent_projects.Open(content_root / ".nexora/test-ui-recents", &content_error));
  assert(recent_projects.Record(content_workspace, &content_error));
  const auto items = content.Browser().Items();
  const auto mesh = std::ranges::find(items, std::filesystem::path("Content/Hero.mesh"),
                                      &nexora::editor::ContentItem::path);
  const auto material = std::ranges::find(items, std::filesystem::path("Content/Hero.material"),
                                          &nexora::editor::ContentItem::path);
  assert(mesh != items.end() && material != items.end());
  const std::array material_dependency{material->id};
  assert(content.Dependencies().Set(mesh->id, material_dependency));
  assert(content.Browser().Select(mesh->id));
  // Dear ImGui's input trickling (ConfigInputTrickleEventQueue, on by default) deliberately applies
  // only one input-type transition per NewFrame() so fast real interleaved events (e.g. a mouse
  // move followed by a click) keep correct sub-frame chronology; a batch mixing pointer/text/key
  // events queued in one ProcessEvents() call below would then need several frames to fully drain.
  // This test replays a synthetic event batch as a single deterministic unit rather than live
  // input, so disable trickling to make ProcessEvents() -> one NewFrame() a reliable, complete
  // apply.
  EditorImGuiTestAccess::SetInputTrickle(host, false);
  assert(!EditorImGuiTestAccess::Inspect(host).input_trickle_enabled);
  host.SetDisplay(1280.0F, 720.0F, 1.0F);
  host.BeginFrame();
  host.DrawProjectSelector(&recent_projects);
  const auto selector_metrics = host.EndFrame();
  const auto selector_state = EditorImGuiTestAccess::Inspect(host);
  assert(selector_metrics.vertices > 0 && selector_metrics.indices > 0);
  assert(selector_state.project_selector_visible && selector_state.selector_recent_projects == 1);
  assert(!selector_state.app_focused && selector_state.selector_root_focus_pending);
  const std::array focus_event{Nexora::Window::WindowEvent{
      {}, Nexora::Window::WindowEventType::FocusChanged, 0, 0, 0, 1.0F, 1, 0}};
  host.ProcessEvents(focus_event);
  host.BeginFrame();
  host.DrawProjectSelector(&recent_projects);
  static_cast<void>(host.EndFrame());
  const auto focused_selector_state = EditorImGuiTestAccess::Inspect(host);
  assert(focused_selector_state.app_focused && !focused_selector_state.selector_root_focus_pending);
  host.BeginFrame();
  host.DrawProjectSelector(&recent_projects);
  static_cast<void>(host.EndFrame());
  assert(EditorImGuiTestAccess::Inspect(host).selector_root_active);
  std::vector<Nexora::Window::WindowEvent> selector_text;
  for (const char character : std::string_view("selected"))
    selector_text.push_back(
        {{}, Nexora::Window::WindowEventType::Text, 0, 0, 0, 1.0F, character, 0});
  host.ProcessEvents(selector_text);
  host.BeginFrame();
  host.DrawProjectSelector(&recent_projects);
  static_cast<void>(host.EndFrame());
  assert(EditorImGuiTestAccess::ProjectSelectorRoot(host) == "selected");
  const std::array selector_shortcut{
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::LeftControl),
                                  1,
                                  Nexora::Window::KeyModifiers::Control},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::O),
                                  1,
                                  Nexora::Window::KeyModifiers::Control}};
  host.ProcessEvents(selector_shortcut);
  host.BeginFrame();
  host.DrawProjectSelector(&recent_projects);
  static_cast<void>(host.EndFrame());
  const auto keyboard_selector_request = host.TakeProjectSelectorRequest();
  assert(keyboard_selector_request);
  assert(keyboard_selector_request->action == nexora::editor::imgui::ProjectSelectorAction::Open);
  assert(keyboard_selector_request->root == std::filesystem::path("selected"));
  assert(keyboard_selector_request->access == nexora::editor::ProjectAccess::ReadWrite);
  const std::array selector_key_release{
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::O),
                                  0,
                                  Nexora::Window::KeyModifiers::Control},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::LeftControl),
                                  0}};
  host.ProcessEvents(selector_key_release);
  host.SetProjectSelectorError("project could not be opened");
  assert(host.ProjectSelectorError() == "project could not be opened");
  const auto selector_root = content_root / "selected";
  EditorImGuiTestAccess::QueueProjectSelection(
      host, {nexora::editor::imgui::ProjectSelectorAction::Create, selector_root, "Selected",
             nexora::editor::ProjectAccess::ReadWrite});
  const auto selector_request = host.TakeProjectSelectorRequest();
  assert(selector_request &&
         selector_request->action == nexora::editor::imgui::ProjectSelectorAction::Create &&
         selector_request->root == selector_root && selector_request->name == "Selected" &&
         selector_request->access == nexora::editor::ProjectAccess::ReadWrite);
  assert(!host.TakeProjectSelectorRequest());
  host.SetProjectSelectorStatus("Importing project content", true);
  EditorImGuiTestAccess::QueueProjectImportCancellation(host);
  assert(host.TakeProjectSelectorCancel());
  assert(!host.TakeProjectSelectorCancel());
  host.SetProjectSelectorStatus({}, false);
  nexora::editor::ProductShell shell;
  std::atomic_bool release_import{false};
  const auto import_blocker =
      import_jobs.Submit({[&release_import](const nexora::core::CancellationToken &) {
                            while (!release_import.load(std::memory_order_acquire))
                              std::this_thread::yield();
                          },
                          nexora::core::JobPriority::High,
                          {},
                          "Editor ImGui import barrier"});
  assert(content.BeginReimport(imports, mesh->id, &content_error));
  // Dear ImGui's Shortcut()/SetShortcutRouting() arbitrate routing one frame ahead: a route
  // registered during a frame only "wins" starting the *next* frame (see RoutingNext/RoutingCurr
  // in imgui.cpp's UpdateKeyRoutingTable()/SetShortcutRouting()). Draw one frame with no key event
  // queued so the Ctrl+S route is primed before the simulated keypress below; DrawProductShell()
  // registers the shortcut unconditionally regardless of key state, and this warm-up frame never
  // calls Render(), so it does not perturb the renderer-metrics assertions further down.
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  assert(shell.LastCommand().empty());
  static_cast<void>(host.EndFrame());
  const auto importing_state = EditorImGuiTestAccess::Inspect(host);
  assert(importing_state.content_import_active);
  assert(importing_state.content_import_state == nexora::editor::ImportOperationState::Running ||
         importing_state.content_import_state == nexora::editor::ImportOperationState::Cancelling);
  assert(content.CancelReimport());
  release_import.store(true, std::memory_order_release);
  import_jobs.Wait(import_blocker);
  const auto import_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!content.PollReimport(&content_error) &&
         std::chrono::steady_clock::now() < import_deadline)
    std::this_thread::yield();
  assert(content.ReimportStatus() &&
         content.ReimportStatus()->state == nexora::editor::ImportOperationState::Cancelled);
  const std::array events{
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::Pointer, 0, 0, 0, 1.0F, 320, 240},
      Nexora::Window::WindowEvent{{}, Nexora::Window::WindowEventType::Text, 0, 0, 0, 1.0F, 'N', 0},
      Nexora::Window::WindowEvent{
          {}, Nexora::Window::WindowEventType::DpiChanged, 0, 0, 0, 1.5F, 0, 0},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::LeftControl),
                                  1,
                                  Nexora::Window::KeyModifiers::Control},
      Nexora::Window::WindowEvent{{},
                                  Nexora::Window::WindowEventType::Key,
                                  0,
                                  0,
                                  0,
                                  1.0F,
                                  static_cast<std::int32_t>(Nexora::Window::Key::S),
                                  1,
                                  Nexora::Window::KeyModifiers::Control},
  };
  host.ProcessEvents(events);
  host.SetDisplay(1600.0F, 900.0F, 1.5F);
  const auto display_state = EditorImGuiTestAccess::Inspect(host);
  assert(display_state.display_width == 1600.0F);
  assert(display_state.display_height == 900.0F);
  assert(display_state.framebuffer_scale == 1.5F);
  assert(display_state.font_global_scale > 0.66F && display_state.font_global_scale < 0.67F);
  host.BeginFrame();
  host.DrawProductShell(shell, &scene, &content_workspace, &content, &recent_projects, &imports);
  assert(shell.LastCommand() == "editor.scene.save");
  const auto metrics = host.EndFrame();
  assert(metrics.command_lists > 0);
  assert(metrics.vertices > 0);
  assert(metrics.indices > 0);
  const auto content_state = EditorImGuiTestAccess::Inspect(host);
  assert(content_state.content_visible_items == 2);
  assert(content_state.content_visible_folders == 0);
  assert(content_state.content_selection == 1);
  assert(content_state.content_forward_dependencies == 1);
  assert(content_state.content_reverse_dependencies == 0);
  assert(!content_state.content_import_active);
  assert(content_state.content_import_state == nexora::editor::ImportOperationState::Cancelled);
  assert(content_state.content_import_diagnostics > 0);
  assert(content_state.project_writable);
  assert(!content_state.project_upgrade_required);
  assert(content_state.recent_projects == 1);
  auto device = nexora::rhi::CreateValidationDevice();
  const auto target =
      device->CreateTexture({1280, 720, nexora::rhi::TextureFormat::Rgba8Unorm,
                             nexora::rhi::ResourceState::Undefined, "Editor ImGui offscreen"});
  const auto user_texture =
      device->CreateTexture({1, 1, nexora::rhi::TextureFormat::Rgba8Unorm,
                             nexora::rhi::ResourceState::ShaderRead, "Editor user texture"});
  const auto texture_id = host.RegisterTexture(*device, user_texture);
  assert(texture_id != 0);
  assert(EditorImGuiTestAccess::OverrideDrawTexture(host, texture_id) > 0);
  const auto draws =
      host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::Undefined, false);
  auto repeated_draws = draws;
  for (int frame = 0; frame < 3; ++frame)
    repeated_draws =
        host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::ShaderRead, false);
  const auto retained = host.GetRendererMetrics();
  assert(repeated_draws == draws && retained.frames == 4 && retained.buffer_reallocations == 6 &&
         retained.font_rebuilds == 1 && retained.completion_waits == 0 &&
         retained.rejected_textures == 0);
  const auto diagnostics = device->Diagnostics();
  assert(draws > 0 && diagnostics.draw_calls == draws * 4 && diagnostics.validation_errors == 0);
  assert(host.UnregisterTexture(texture_id));
  assert(!host.UnregisterTexture(texture_id));
  assert(EditorImGuiTestAccess::OverrideDrawTexture(host, texture_id) > 0);
  assert(host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::ShaderRead, false) ==
         draws);
  assert(host.GetRendererMetrics().rejected_textures > 0);
  const auto next_texture_id = host.RegisterTexture(*device, user_texture);
  assert(next_texture_id != texture_id && host.UnregisterTexture(next_texture_id));

  // Exercise every DPI bucket and a bounded long-running layout/render loop. Each bucket switch
  // rebuilds the atlas once; subsequent stable frames reuse allocations and reject no callbacks.
  constexpr std::array dpi_scales{1.25F, 1.5F, 2.0F, 1.0F};
  for (const float dpi : dpi_scales) {
    host.SetDisplay(1280.0F / dpi, 720.0F / dpi, dpi);
    host.BeginFrame();
    host.DrawProductShell(shell, &scene, nullptr, &content);
    static_cast<void>(host.EndFrame());
    assert(host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::ShaderRead, false) >
           0);
  }
  // Let all three upload slots observe the complete docked Content layout before taking the soak
  // baseline. A newly added panel may be selected only after docking settles, but must not cause
  // any allocation growth once every slot has rendered that layout.
  for (int frame = 0; frame < 6; ++frame) {
    host.BeginFrame();
    host.DrawProductShell(shell, &scene, nullptr, &content);
    static_cast<void>(host.EndFrame());
    assert(host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::ShaderRead, false) >
           0);
  }
  const auto reallocations_before_soak = host.GetRendererMetrics().buffer_reallocations;
  for (int frame = 0; frame < 512; ++frame) {
    host.BeginFrame();
    host.DrawProductShell(shell, &scene, nullptr, &content);
    static_cast<void>(host.EndFrame());
    assert(host.Render(*device, target, 1280, 720, nexora::rhi::ResourceState::ShaderRead, false) >
           0);
  }
  const auto soaked = host.GetRendererMetrics();
  assert(soaked.font_rebuilds == 5 && soaked.buffer_reallocations == reallocations_before_soak);
  const auto layout = host.SaveLayout();
  assert(!layout.empty() && host.LoadLayout(layout));
  assert(!host.LoadLayout("not an ImGui layout"));

  const auto recovery_root =
      std::filesystem::temp_directory_path() /
      ("nexora-imgui-recovery-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  nexora::editor::ProjectWorkspace workspace;
  std::string error;
  assert(workspace.Create(recovery_root, "Recovery", &error));
  std::ofstream(recovery_root / ".nexora/workspace.recovery") << "schema=1\ninvalid\n";
  assert(!host.ApplyRecoveryChoice(workspace, nexora::editor::imgui::RecoveryChoice::Recover));
  assert(workspace.HasRecoveryJournal() && !host.RecoveryError().empty());
  assert(host.ApplyRecoveryChoice(workspace, nexora::editor::imgui::RecoveryChoice::Discard));
  assert(host.TakeRecoveryChoice() == nexora::editor::imgui::RecoveryChoice::Discard);
  assert(host.TakeRecoveryChoice() == nexora::editor::imgui::RecoveryChoice::None);
  workspace = {};
  std::filesystem::remove_all(recovery_root);
  content_workspace = {};
  std::filesystem::remove_all(content_root);
  host.ReleaseRenderer(*device);
  device->DestroyTexture(user_texture);
  device->DestroyTexture(target);
}
