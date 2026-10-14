#pragma once
#include "Nexora/Editor/TwoDDocument.h"
#include "Nexora/EditorImGui/Api.h"
namespace nexora::editor::imgui {
enum class TwoDPanelTab { Scene, Atlas, Animation, Tilemap };
struct TwoDPanelState final {
  std::optional<TwoDPanelTab> requestedTab;
  runtime::Id selected{};
  std::uint32_t region{};
  twod::Camera camera;
  twod::Animator preview;
  std::uint64_t previewRevision{UINT64_MAX};
  std::int32_t tileX{}, tileY{}, tileLayer{};
  bool collision{}, previewPaused{}, saveRequested{}, openRequested{}, importAtlasRequested{},
      importClipRequested{};
  std::string status;
};
// Called inside the host frame. Texture is a host-registered atlas handle. Buttons emit
// import/open/save requests; the project owner revalidates paths, access and revision before IO.
NEXORA_EDITOR_IMGUI_API void DrawTwoDPanel(TwoDDocument &, TwoDPanelState &, bool authoringAllowed,
                                           std::uint64_t texture, float seconds);
} // namespace nexora::editor::imgui
