#include "Nexora/EditorImGui/EditorImGui.h"

#include <imgui.h>

#include <algorithm>
#include <utility>

namespace nexora::editor::imgui {
struct EditorImGuiHost::State final {
  ImGuiContext *context = nullptr;
  float dpi_scale = 1.0F;
  RecoveryChoice recovery_choice = RecoveryChoice::None;
};

namespace {
void Activate(ImGuiContext *context) { ImGui::SetCurrentContext(context); }

void ApplyTheme(float scale) {
  ImGui::StyleColorsDark();
  auto &style = ImGui::GetStyle();
  style.WindowRounding = 4.0F;
  style.FrameRounding = 3.0F;
  style.ScaleAllSizes(scale);
}
} // namespace

EditorImGuiHost::EditorImGuiHost() : state_(std::make_unique<State>()) {
  state_->context = ImGui::CreateContext();
  Activate(state_->context);
  auto &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.IniFilename = nullptr;
  ApplyTheme(1.0F);
}

EditorImGuiHost::~EditorImGuiHost() {
  if (state_ && state_->context)
    ImGui::DestroyContext(state_->context);
}
EditorImGuiHost::EditorImGuiHost(EditorImGuiHost &&) noexcept = default;
EditorImGuiHost &EditorImGuiHost::operator=(EditorImGuiHost &&) noexcept = default;

void EditorImGuiHost::SetDisplay(float width, float height, float dpi_scale) {
  Activate(state_->context);
  ImGui::GetIO().DisplaySize = {std::max(width, 1.0F), std::max(height, 1.0F)};
  dpi_scale = std::max(dpi_scale, 0.25F);
  if (dpi_scale != state_->dpi_scale) {
    state_->dpi_scale = dpi_scale;
    ApplyTheme(dpi_scale);
  }
}

void EditorImGuiHost::ProcessEvents(std::span<const Nexora::Window::WindowEvent> events) {
  Activate(state_->context);
  auto &io = ImGui::GetIO();
  for (const auto &event : events) {
    switch (event.type) {
    case Nexora::Window::WindowEventType::Pointer:
      io.AddMousePosEvent(static_cast<float>(event.value0), static_cast<float>(event.value1));
      break;
    case Nexora::Window::WindowEventType::Wheel:
      io.AddMouseWheelEvent(static_cast<float>(event.value0), static_cast<float>(event.value1));
      break;
    case Nexora::Window::WindowEventType::Text:
      if (event.value0 > 0)
        io.AddInputCharacter(static_cast<unsigned int>(event.value0));
      break;
    case Nexora::Window::WindowEventType::FocusChanged:
      io.AddFocusEvent(event.value0 != 0);
      break;
    case Nexora::Window::WindowEventType::DpiChanged:
      SetDisplay(io.DisplaySize.x, io.DisplaySize.y, event.scale);
      break;
    default:
      break;
    }
  }
}

void EditorImGuiHost::BeginFrame(float delta_seconds) {
  Activate(state_->context);
  ImGui::GetIO().DeltaTime = std::max(delta_seconds, 0.0001F);
  ImGui::NewFrame();
}

void EditorImGuiHost::DrawProductShell(const ProductShell &, SceneDocument *scene,
                                       bool recovery_available) {
  Activate(state_->context);
  ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
  if (ImGui::Begin("Hierarchy###editor.panel.hierarchy")) {
    if (scene != nullptr) {
      for (const auto entity : scene->Selection())
        ImGui::Text("Selected entity: %llu", static_cast<unsigned long long>(entity));
    } else {
      ImGui::TextUnformatted("No scene is open.");
    }
  }
  ImGui::End();
  if (ImGui::Begin("Console###editor.panel.console"))
    ImGui::TextUnformatted("Nexora Editor is ready.");
  ImGui::End();

  if (recovery_available)
    ImGui::OpenPopup("Recover workspace###editor.recovery");
  if (ImGui::BeginPopupModal("Recover workspace###editor.recovery", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("A recovery journal is available.");
    if (ImGui::Button("Recover")) {
      state_->recovery_choice = RecoveryChoice::Recover;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard")) {
      state_->recovery_choice = RecoveryChoice::Discard;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

FrameMetrics EditorImGuiHost::EndFrame() {
  Activate(state_->context);
  ImGui::Render();
  const auto *draw = ImGui::GetDrawData();
  if (draw == nullptr)
    return {};
  return {static_cast<std::uint32_t>(draw->TotalVtxCount),
          static_cast<std::uint32_t>(draw->TotalIdxCount),
          static_cast<std::uint32_t>(draw->CmdListsCount)};
}

RecoveryChoice EditorImGuiHost::TakeRecoveryChoice() noexcept {
  const auto choice = state_->recovery_choice;
  state_->recovery_choice = RecoveryChoice::None;
  return choice;
}
} // namespace nexora::editor::imgui
