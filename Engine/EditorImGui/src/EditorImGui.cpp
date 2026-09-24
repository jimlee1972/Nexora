#include "Nexora/EditorImGui/EditorImGui.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
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
  bool initial_dock_layout_built = false;
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

std::string PanelWindowName(std::string_view id) {
  const auto panels = ProductShell::Panels();
  const auto panel = std::ranges::find(panels, id, &PanelDescriptor::id);
  if (panel == panels.end())
    return std::string(id);
  return std::string(panel->title) + "###" + std::string(panel->id);
}

void BuildInitialDockLayout(ImGuiID dockspace, const ImGuiViewport &viewport) {
  ImGui::DockBuilderRemoveNode(dockspace);
  ImGui::DockBuilderAddNode(dockspace,
                            ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
  ImGui::DockBuilderSetNodeSize(dockspace, viewport.Size);

  ImGuiID center = dockspace;
  const ImGuiID hierarchy =
      ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.22F, nullptr, &center);
  const ImGuiID console =
      ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25F, nullptr, &center);
  ImGui::DockBuilderDockWindow(PanelWindowName("nexora.hierarchy").c_str(), hierarchy);
  ImGui::DockBuilderDockWindow(PanelWindowName("nexora.console").c_str(), console);
  ImGui::DockBuilderFinish(dockspace);
}

ImGuiKey ToImGuiKey(Nexora::Window::Key key) {
  using Key = Nexora::Window::Key;
  if (key >= Key::Digit0 && key <= Key::Digit9)
    return static_cast<ImGuiKey>(ImGuiKey_0 + static_cast<int>(key) -
                                 static_cast<int>(Key::Digit0));
  if (key >= Key::A && key <= Key::Z)
    return static_cast<ImGuiKey>(ImGuiKey_A + static_cast<int>(key) - static_cast<int>(Key::A));
  if (key >= Key::F1 && key <= Key::F12)
    return static_cast<ImGuiKey>(ImGuiKey_F1 + static_cast<int>(key) - static_cast<int>(Key::F1));
  if (key >= Key::Keypad0 && key <= Key::Keypad9)
    return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + static_cast<int>(key) -
                                 static_cast<int>(Key::Keypad0));
#define NEXORA_KEY(native, imgui)                                                                  \
  case Key::native:                                                                                \
    return ImGuiKey_##imgui
  switch (key) {
    NEXORA_KEY(Tab, Tab);
    NEXORA_KEY(LeftArrow, LeftArrow);
    NEXORA_KEY(RightArrow, RightArrow);
    NEXORA_KEY(UpArrow, UpArrow);
    NEXORA_KEY(DownArrow, DownArrow);
    NEXORA_KEY(PageUp, PageUp);
    NEXORA_KEY(PageDown, PageDown);
    NEXORA_KEY(Home, Home);
    NEXORA_KEY(End, End);
    NEXORA_KEY(Insert, Insert);
    NEXORA_KEY(Delete, Delete);
    NEXORA_KEY(Backspace, Backspace);
    NEXORA_KEY(Space, Space);
    NEXORA_KEY(Enter, Enter);
    NEXORA_KEY(Escape, Escape);
    NEXORA_KEY(Apostrophe, Apostrophe);
    NEXORA_KEY(Comma, Comma);
    NEXORA_KEY(Minus, Minus);
    NEXORA_KEY(Period, Period);
    NEXORA_KEY(Slash, Slash);
    NEXORA_KEY(Semicolon, Semicolon);
    NEXORA_KEY(Equal, Equal);
    NEXORA_KEY(LeftBracket, LeftBracket);
    NEXORA_KEY(Backslash, Backslash);
    NEXORA_KEY(RightBracket, RightBracket);
    NEXORA_KEY(GraveAccent, GraveAccent);
    NEXORA_KEY(CapsLock, CapsLock);
    NEXORA_KEY(ScrollLock, ScrollLock);
    NEXORA_KEY(NumLock, NumLock);
    NEXORA_KEY(PrintScreen, PrintScreen);
    NEXORA_KEY(Pause, Pause);
    NEXORA_KEY(KeypadDecimal, KeypadDecimal);
    NEXORA_KEY(KeypadDivide, KeypadDivide);
    NEXORA_KEY(KeypadMultiply, KeypadMultiply);
    NEXORA_KEY(KeypadSubtract, KeypadSubtract);
    NEXORA_KEY(KeypadAdd, KeypadAdd);
    NEXORA_KEY(KeypadEnter, KeypadEnter);
    NEXORA_KEY(KeypadEqual, KeypadEqual);
    NEXORA_KEY(LeftShift, LeftShift);
    NEXORA_KEY(LeftControl, LeftCtrl);
    NEXORA_KEY(LeftAlt, LeftAlt);
    NEXORA_KEY(LeftSuper, LeftSuper);
    NEXORA_KEY(RightShift, RightShift);
    NEXORA_KEY(RightControl, RightCtrl);
    NEXORA_KEY(RightAlt, RightAlt);
    NEXORA_KEY(RightSuper, RightSuper);
    NEXORA_KEY(Menu, Menu);
  default:
    return ImGuiKey_None;
  }
#undef NEXORA_KEY
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
      const auto modifiers = static_cast<unsigned>(event.modifiers);
      io.AddKeyEvent(ImGuiMod_Ctrl, (modifiers & static_cast<unsigned>(
                                                     Nexora::Window::KeyModifiers::Control)) != 0);
      io.AddKeyEvent(ImGuiMod_Shift,
                     (modifiers & static_cast<unsigned>(Nexora::Window::KeyModifiers::Shift)) != 0);
      io.AddKeyEvent(ImGuiMod_Alt,
                     (modifiers & static_cast<unsigned>(Nexora::Window::KeyModifiers::Alt)) != 0);
      io.AddKeyEvent(ImGuiMod_Super,
                     (modifiers & static_cast<unsigned>(Nexora::Window::KeyModifiers::Super)) != 0);
      const auto key = ToImGuiKey(static_cast<Nexora::Window::Key>(event.value0));
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
  const auto *viewport = ImGui::GetMainViewport();
  const ImGuiID dockspace =
      ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode);
  if (!state_->initial_dock_layout_built) {
    BuildInitialDockLayout(dockspace, *viewport);
    state_->initial_dock_layout_built = true;
  }
  const auto hierarchy_window = PanelWindowName("nexora.hierarchy");
  if (ImGui::Begin(hierarchy_window.c_str())) {
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
  const auto console_window = PanelWindowName("nexora.console");
  if (ImGui::Begin(console_window.c_str()))
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

Nexora::Presentation::SurfaceStatus
EditorImGuiHost::Render(Nexora::Presentation::RenderSurface &surface, std::uint32_t width,
                        std::uint32_t height) {
  Activate(state_->context);
  const auto *draw = ImGui::GetDrawData();
  if (draw == nullptr || width == 0 || height == 0)
    return Nexora::Presentation::SurfaceStatus::InvalidDescriptor;
  std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width) * height, 0xff29140aU);
  unsigned char *atlas = nullptr;
  int atlas_width = 0;
  int atlas_height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&atlas, &atlas_width, &atlas_height);
  auto blend = [](std::uint32_t destination, std::uint32_t source) {
    const auto alpha = (source >> 24U) & 0xffU;
    std::uint32_t result = 0xff000000U;
    for (unsigned shift : {0U, 8U, 16U}) {
      const auto value = (((source >> shift) & 0xffU) * alpha +
                          ((destination >> shift) & 0xffU) * (255U - alpha)) /
                         255U;
      result |= value << shift;
    }
    return result;
  };
  for (int list_index = 0; list_index < draw->CmdListsCount; ++list_index) {
    const auto &list = *draw->CmdLists[list_index];
    for (const auto &command : list.CmdBuffer) {
      if (command.UserCallback != nullptr)
        continue;
      const int clip_left = std::max(0, static_cast<int>(command.ClipRect.x));
      const int clip_top = std::max(0, static_cast<int>(command.ClipRect.y));
      const int clip_right =
          std::min(static_cast<int>(width), static_cast<int>(command.ClipRect.z));
      const int clip_bottom =
          std::min(static_cast<int>(height), static_cast<int>(command.ClipRect.w));
      for (unsigned index = 0; index + 2 < command.ElemCount; index += 3) {
        const auto vertex = [&](unsigned corner) -> const ImDrawVert & {
          return list.VtxBuffer[static_cast<int>(
              list.IdxBuffer[command.IdxOffset + index + corner] + command.VtxOffset)];
        };
        const auto &a = vertex(0);
        const auto &b = vertex(1);
        const auto &c = vertex(2);
        const float area =
            (b.pos.x - a.pos.x) * (c.pos.y - a.pos.y) - (b.pos.y - a.pos.y) * (c.pos.x - a.pos.x);
        if (std::abs(area) < 0.0001F)
          continue;
        const int left = std::max(
            clip_left, static_cast<int>(std::floor(std::min({a.pos.x, b.pos.x, c.pos.x}))));
        const int top =
            std::max(clip_top, static_cast<int>(std::floor(std::min({a.pos.y, b.pos.y, c.pos.y}))));
        const int right = std::min(
            clip_right, static_cast<int>(std::ceil(std::max({a.pos.x, b.pos.x, c.pos.x}))));
        const int bottom = std::min(
            clip_bottom, static_cast<int>(std::ceil(std::max({a.pos.y, b.pos.y, c.pos.y}))));
        for (int y = top; y < bottom; ++y)
          for (int x = left; x < right; ++x) {
            const float px = static_cast<float>(x) + 0.5F, py = static_cast<float>(y) + 0.5F;
            const float wa =
                ((b.pos.x - px) * (c.pos.y - py) - (b.pos.y - py) * (c.pos.x - px)) / area;
            const float wb =
                ((c.pos.x - px) * (a.pos.y - py) - (c.pos.y - py) * (a.pos.x - px)) / area;
            const float wc = 1.0F - wa - wb;
            if (wa < 0.0F || wb < 0.0F || wc < 0.0F)
              continue;
            const float u = wa * a.uv.x + wb * b.uv.x + wc * c.uv.x;
            const float v = wa * a.uv.y + wb * b.uv.y + wc * c.uv.y;
            const int tx = std::clamp(static_cast<int>(u * atlas_width), 0, atlas_width - 1);
            const int ty = std::clamp(static_cast<int>(v * atlas_height), 0, atlas_height - 1);
            const auto sample = atlas[(ty * atlas_width + tx) * 4 + 3];
            const auto channel = [&](unsigned shift) {
              return static_cast<std::uint32_t>(wa * ((a.col >> shift) & 0xffU) +
                                                wb * ((b.col >> shift) & 0xffU) +
                                                wc * ((c.col >> shift) & 0xffU));
            };
            const std::uint32_t color = channel(0) | (channel(8) << 8U) | (channel(16) << 16U) |
                                        ((channel(24) * sample / 255U) << 24U);
            auto &destination = pixels[static_cast<std::size_t>(y) * width + x];
            destination = blend(destination, color);
          }
      }
    }
  }
  return surface.CompositeRgba8(std::as_bytes(std::span{pixels}), width, height);
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
