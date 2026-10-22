#pragma once
// Test-only, owning observation of actual UI/model state. Never performs an action or IO.
#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/MeshImport.h"
#include "Nexora/Editor/ProjectContent.h"
#include <sstream>
namespace nexora::editor::test {
inline std::string ProjectAssetWorkflowEvidence(const imgui::EditorImGuiHost &ui,
                                                const ProjectWorkspace *workspace,
                                                const ProjectContentSession *content,
                                                const SceneDocument *scene, float dpi) {
  using Access = imgui::EditorImGuiTestAccess;
  const auto state = Access::Inspect(ui);
  std::ostringstream out;
  out << "project asset workflow project=" << bool(workspace) << " focused=" << state.app_focused
      << " selector=" << state.project_selector_visible;
  const auto point = [&](const char *name, std::optional<std::array<float, 2>> position) {
    if (position)
      out << ' ' << name << '=' << (*position)[0] * dpi << ',' << (*position)[1] * dpi;
  };
  if (!workspace) {
    point("root", Access::ProjectUpgradePosition(ui, 0));
    point("name", Access::ProjectUpgradePosition(ui, 3));
    point("readonly", Access::ProjectUpgradePosition(ui, 2));
  } else if (content) {
    const auto &browser = content->Browser();
    out << " writable=" << workspace->Writable() << " generation=" << browser.ProjectGeneration()
        << " revision=" << browser.Revision() << " assets=" << browser.Items().size()
        << " visible=" << state.content_visible_items << " selected=" << browser.Selection().size()
        << " can_undo=" << content->CanUndo() << " busy=" << content->ReimportBusy();
    if (const auto status = content->ReimportStatus())
      out << " reimport_state=" << static_cast<unsigned>(status->state);
    point("search", Access::ContentSearchPosition(ui));
    point("home", Access::ContentPathPosition(ui, "Content", true));
    point("moved", Access::ContentPathPosition(ui, "Content/Moved"));
    point("addmesh", Access::ContentAddMeshPosition(ui));
    point("undo", Access::ContentWorkflowPosition(ui, false));
    point("reimport", Access::ContentWorkflowPosition(ui, true));
    const auto hex = [&](std::string_view value) {
      constexpr char digits[] = "0123456789abcdef";
      for (const unsigned char byte : value)
        out << digits[byte >> 4] << digits[byte & 15];
    };
    for (const auto &item : browser.Items()) {
      if (item.path.extension() != ".scene")
        continue;
      const auto path = item.path.generic_u8string();
      out << " scene_asset=" << item.id.ToString() << " scene_path_hex=";
      hex(std::string(path.begin(), path.end()));
      point("scene_point", Access::ContentAssetPosition(ui, item.id));
      break;
    }
    point("context_rename", Access::ContentContextPosition(ui, 0));
    point("context_delete", Access::ContentContextPosition(ui, 1));
    point("rename_field", Access::ContentRenamePosition(ui, 0));
    point("rename_apply", Access::ContentRenamePosition(ui, 1));
    point("rename_cancel", Access::ContentRenamePosition(ui, 2));
    out << " rename_text_hex=";
    hex(Access::ContentRenameText(ui));
    for (const auto &item : browser.Items()) {
      if (item.path.filename() != "WorkflowHero.obj")
        continue;
      out << " hero=" << item.id.ToString()
          << " moved_path=" << (item.path.parent_path() == "Content/Moved")
          << " stamp=" << item.artifact_hash << " mesh=" << bool(item.mesh);
      if (item.mesh)
        out << " vertices=" << item.mesh->vertices.size() << " max_x=" << item.mesh->maximum[0];
      point("hero_point", Access::ContentAssetPosition(ui, item.id));
      break;
    }
    if (scene)
      out << " nodes=" << scene->Nodes().size() << " dirty=" << scene->Dirty();
  }
  return out.str();
}
} // namespace nexora::editor::test
