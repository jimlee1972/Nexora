#include "Nexora/EditorImGui/EditorImGui.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace nexora::editor::imgui {
struct EditorImGuiHost::State final {
  ImGuiContext *context = nullptr;
  Nexora::Presentation::RenderSurface *surface = nullptr;
  float dpi_scale = 1.0F;
  RecoveryChoice recovery_choice = RecoveryChoice::None;
  bool recovery_prompt_opened = false;
  std::string recovery_error;

  static void SetImeData(ImGuiContext *context, ImGuiViewport *, ImGuiPlatformImeData *data) {
    ImGui::SetCurrentContext(context);
    auto *state = static_cast<State *>(ImGui::GetIO().BackendPlatformUserData);
    if (state == nullptr || state->surface == nullptr || !data->WantVisible)
      return;
    static_cast<void>(state->surface->SetImeCandidatePosition(
        static_cast<std::int32_t>(data->InputPos.x), static_cast<std::int32_t>(data->InputPos.y)));
  }
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
ImGuiKey ToImGuiKey(std::int32_t key) {
  if (key >= 'A' && key <= 'Z')
    return static_cast<ImGuiKey>(ImGuiKey_A + key - 'A');
  if (key >= '0' && key <= '9')
    return static_cast<ImGuiKey>(ImGuiKey_0 + key - '0');
  switch (key) {
  case 8:
    return ImGuiKey_Backspace;
  case 9:
    return ImGuiKey_Tab;
  case 13:
    return ImGuiKey_Enter;
  case 27:
    return ImGuiKey_Escape;
  case 32:
    return ImGuiKey_Space;
  default:
    return ImGuiKey_None;
  }
}
} // namespace

EditorImGuiHost::EditorImGuiHost() : state_(std::make_unique<State>()) {
  state_->context = ImGui::CreateContext();
  Activate(state_->context);
  auto &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.IniFilename = nullptr;
  io.BackendPlatformUserData = state_.get();
  ImGui::GetPlatformIO().Platform_SetImeDataFn = &State::SetImeData;
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
    ImGui::GetIO().FontGlobalScale = dpi_scale;
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
      io.AddMouseWheelEvent(static_cast<float>(event.value0) / 120.0F,
                            static_cast<float>(event.value1) / 120.0F);
      break;
    case Nexora::Window::WindowEventType::PointerButton:
      if (event.value0 >= 0 && event.value0 < ImGuiMouseButton_COUNT)
        io.AddMouseButtonEvent(event.value0, event.value1 != 0);
      break;
    case Nexora::Window::WindowEventType::Key: {
      const auto key = ToImGuiKey(event.value0);
      if (key != ImGuiKey_None)
        io.AddKeyEvent(key, event.value1 != 0);
      break;
    }
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

void EditorImGuiHost::DrawProductShell(const ProductShell &shell, SceneDocument *scene,
                                       ProjectWorkspace *workspace) {
  Activate(state_->context);
  ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
  if (ImGui::Begin("Hierarchy###nexora.hierarchy")) {
    if (scene != nullptr) {
      const auto selected = scene->Selection();
      for (const auto &node : scene->Nodes()) {
        const bool is_selected = std::ranges::find(selected, node.id) != selected.end();
        const auto label = std::string(node.name) + "##" + std::to_string(node.id);
        if (ImGui::Selectable(label.c_str(), is_selected)) {
          const std::array selection{node.id};
          scene->Select(selection);
        }
      }
    } else {
      ImGui::TextUnformatted("No scene is open.");
    }
  }
  ImGui::End();
  if (ImGui::Begin("Console###nexora.console"))
    ImGui::Text("Last command: %.*s", static_cast<int>(shell.LastCommand().size()),
                shell.LastCommand().data());
  ImGui::End();

  const bool recovery_available = workspace != nullptr && workspace->HasRecoveryJournal();
  if (recovery_available && !state_->recovery_prompt_opened) {
    ImGui::OpenPopup("Recover workspace###editor.recovery");
    state_->recovery_prompt_opened = true;
  }
  if (!recovery_available)
    state_->recovery_prompt_opened = false;
  if (ImGui::BeginPopupModal("Recover workspace###editor.recovery", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("A recovery journal is available.");
    if (!state_->recovery_error.empty())
      ImGui::TextWrapped("%s", state_->recovery_error.c_str());
    if (ImGui::Button("Recover")) {
      std::string error;
      if (workspace->RecoverWorkspace(&error)) {
        state_->recovery_choice = RecoveryChoice::Recover;
        state_->recovery_error.clear();
        ImGui::CloseCurrentPopup();
      } else {
        state_->recovery_error = std::move(error);
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard")) {
      std::string error;
      if (workspace->DiscardRecovery(&error)) {
        state_->recovery_choice = RecoveryChoice::Discard;
        state_->recovery_error.clear();
        ImGui::CloseCurrentPopup();
      } else {
        state_->recovery_error = std::move(error);
      }
    }
    ImGui::EndPopup();
  }
}

std::uint32_t EditorImGuiHost::Render(nexora::rhi::Device &device,
                                      nexora::rhi::TextureHandle target, std::uint32_t width,
                                      std::uint32_t height, nexora::rhi::ResourceState before,
                                      bool prepare_for_present) {
  Activate(state_->context);
  const auto *draw = ImGui::GetDrawData();
  if (draw == nullptr || draw->CmdListsCount == 0 || width == 0 || height == 0)
    return 0;
  const auto pipeline =
      device.CreatePipeline({0x494d4755494c4159ULL, 0x494d475549534844ULL,
                             nexora::rhi::TextureFormat::Rgba8Unorm, "Editor ImGui"});
  const auto font_texture =
      device.CreateTexture({1, 1, nexora::rhi::TextureFormat::Rgba8Unorm,
                            nexora::rhi::ResourceState::ShaderRead, "Editor ImGui font atlas"});
  auto commands = device.CreateCommandList(nexora::rhi::QueueType::Graphics);
  commands->Transition({target, before, nexora::rhi::ResourceState::RenderTarget});
  commands->BeginRendering({target, width, height});
  commands->BindPipeline(pipeline);
  std::vector<nexora::rhi::BufferHandle> transient_buffers;
  std::uint32_t submitted = 0;
  for (int list_index = 0; list_index < draw->CmdListsCount; ++list_index) {
    const auto *list = draw->CmdLists[list_index];
    if (list->VtxBuffer.empty() || list->IdxBuffer.empty())
      continue;
    const auto vertex_bytes = std::as_bytes(
        std::span{list->VtxBuffer.Data, static_cast<std::size_t>(list->VtxBuffer.Size)});
    const auto index_bytes = std::as_bytes(
        std::span{list->IdxBuffer.Data, static_cast<std::size_t>(list->IdxBuffer.Size)});
    const auto vertex_buffer = device.CreateBuffer({vertex_bytes.size(), "Editor ImGui vertices"});
    const auto index_buffer = device.CreateBuffer({index_bytes.size(), "Editor ImGui indices"});
    transient_buffers.push_back(vertex_buffer);
    transient_buffers.push_back(index_buffer);
    device.WriteBuffer(vertex_buffer, 0, vertex_bytes);
    device.WriteBuffer(index_buffer, 0, index_bytes);
    commands->BindVertexBuffer(vertex_buffer);
    commands->BindIndexBuffer(index_buffer, sizeof(ImDrawIdx) == 2
                                                ? nexora::rhi::IndexFormat::Uint16
                                                : nexora::rhi::IndexFormat::Uint32);
    commands->BindTexture(0, font_texture);
    for (const auto &draw_command : list->CmdBuffer) {
      if (draw_command.UserCallback == nullptr && draw_command.ElemCount > 0) {
        const auto left = std::max(0.0F, (draw_command.ClipRect.x - draw->DisplayPos.x) *
                                             draw->FramebufferScale.x);
        const auto top = std::max(0.0F, (draw_command.ClipRect.y - draw->DisplayPos.y) *
                                            draw->FramebufferScale.y);
        const auto right =
            std::min(static_cast<float>(width),
                     (draw_command.ClipRect.z - draw->DisplayPos.x) * draw->FramebufferScale.x);
        const auto bottom =
            std::min(static_cast<float>(height),
                     (draw_command.ClipRect.w - draw->DisplayPos.y) * draw->FramebufferScale.y);
        if (right <= left || bottom <= top)
          continue;
        commands->SetScissor({static_cast<std::int32_t>(left), static_cast<std::int32_t>(top),
                              static_cast<std::uint32_t>(right - left),
                              static_cast<std::uint32_t>(bottom - top)});
        commands->DrawIndexed(draw_command.ElemCount, 1, draw_command.IdxOffset,
                              static_cast<std::int32_t>(draw_command.VtxOffset));
        ++submitted;
      }
    }
  }
  commands->EndRendering();
  commands->Transition({target, nexora::rhi::ResourceState::RenderTarget,
                        prepare_for_present ? nexora::rhi::ResourceState::Present
                                            : nexora::rhi::ResourceState::ShaderRead});
  device.Submit(*commands);
  for (const auto buffer : transient_buffers)
    device.DestroyBuffer(buffer);
  device.DestroyTexture(font_texture);
  device.DestroyPipeline(pipeline);
  return submitted;
}

void EditorImGuiHost::UpdateImeCandidate(Nexora::Presentation::RenderSurface &surface) {
  Activate(state_->context);
  state_->surface = &surface;
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
