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
  struct Renderer final {
    struct UploadSlot final {
      nexora::rhi::BufferHandle vertices;
      nexora::rhi::BufferHandle indices;
      std::uint64_t vertex_capacity = 0;
      std::uint64_t index_capacity = 0;
      std::uint64_t completion = 0;
    };
    struct TextureSlot final {
      nexora::rhi::TextureHandle texture;
      std::uint32_t generation = 1;
      bool live = false;
    };
    struct RetiredTexture final {
      nexora::rhi::TextureHandle texture;
      std::uint64_t completion = 0;
    };
    nexora::rhi::Device *device = nullptr;
    nexora::rhi::PipelineHandle pipeline;
    nexora::rhi::TextureHandle font_texture;
    std::array<UploadSlot, 3> upload_slots;
    std::vector<TextureSlot> textures{{}};
    std::vector<RetiredTexture> retired_textures;
    std::size_t upload_slot = 0;
    std::uint32_t font_generation = 0;
  } renderer;
  ImGuiContext *context = nullptr;
  Nexora::Presentation::RenderSurface *surface = nullptr;
  float dpi_scale = 1.0F;
  float dpi_bucket = 1.0F;
  std::uint32_t font_generation = 1;
  std::uint32_t surface_font_generation = 0;
  RendererMetrics renderer_metrics;
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
        static_cast<std::int32_t>(std::lround(data->InputPos.x * state->dpi_scale)),
        static_cast<std::int32_t>(std::lround(data->InputPos.y * state->dpi_scale))));
  }
};

namespace {
void Activate(ImGuiContext *context) { ImGui::SetCurrentContext(context); }

void ApplyTheme() {
  ImGui::StyleColorsDark();
  auto &style = ImGui::GetStyle();
  style.WindowRounding = 4.0F;
  style.FrameRounding = 3.0F;
}

float DpiBucket(float scale) {
  constexpr std::array buckets{1.0F, 1.25F, 1.5F, 2.0F};
  return *std::ranges::min_element(buckets, {},
                                   [scale](float bucket) { return std::abs(bucket - scale); });
}

std::uint64_t GrownCapacity(std::uint64_t required) {
  std::uint64_t capacity = 4096;
  while (capacity < required)
    capacity *= 2;
  return capacity;
}

std::uint64_t TextureId(std::uint32_t index, std::uint32_t generation) {
  return static_cast<std::uint64_t>(generation) << 32U | index;
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
  // ImGuiDockNodeFlags_DockSpace is ImGuiDockNodeFlagsPrivate_, a different enum type from the
  // public ImGuiDockNodeFlags_ that ImGuiDockNodeFlags_PassthruCentralNode belongs to; OR-ing them
  // directly triggers -Wdeprecated-enum-enum-conversion, so combine them as plain ints first.
  ImGui::DockBuilderAddNode(dockspace, static_cast<ImGuiDockNodeFlags>(
                                           static_cast<int>(ImGuiDockNodeFlags_DockSpace) |
                                           static_cast<int>(ImGuiDockNodeFlags_PassthruCentralNode)));
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
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;
  io.BackendPlatformUserData = state_.get();
  io.Fonts->SetTexID(static_cast<ImTextureID>(TextureId(0, 1)));
  ImGui::GetPlatformIO().Platform_SetImeDataFn = &State::SetImeData;
  ApplyTheme();
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
  ImGui::GetIO().DisplayFramebufferScale = {dpi_scale, dpi_scale};
  const float bucket = DpiBucket(dpi_scale);
  if (bucket != state_->dpi_bucket) {
    state_->dpi_bucket = bucket;
    auto &fonts = *ImGui::GetIO().Fonts;
    fonts.Clear();
    ImFontConfig config;
    config.SizePixels = 13.0F * bucket;
    fonts.AddFontDefault(&config);
    fonts.Build();
    fonts.SetTexID(static_cast<ImTextureID>(TextureId(0, 1)));
    ImGui::GetIO().FontGlobalScale = 1.0F / bucket;
    ++state_->font_generation;
  }
  state_->dpi_scale = dpi_scale;
  ApplyTheme();
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
      if (event.value0 > 0 && event.value0 <= 0x10ffff &&
          !(event.value0 >= 0xd800 && event.value0 <= 0xdfff))
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

void EditorImGuiHost::DrawProductShell(ProductShell &shell, SceneDocument *scene,
                                       ProjectWorkspace *workspace) {
  Activate(state_->context);
  if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
    static_cast<void>(shell.RouteCommand("editor.scene.save"));
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
      if (ApplyRecoveryChoice(*workspace, RecoveryChoice::Recover))
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard recovery data")) {
      if (ApplyRecoveryChoice(*workspace, RecoveryChoice::Discard))
        ImGui::CloseCurrentPopup();
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
  auto &renderer = state_->renderer;
  if (renderer.device != nullptr && renderer.device != &device)
    throw std::logic_error("Editor ImGui renderer is already bound to another device");
  renderer.device = &device;
  const auto completed = device.CompletedSubmissionValue();
  std::erase_if(renderer.retired_textures, [&](const auto &retired) {
    if (retired.completion > completed)
      return false;
    device.DestroyTexture(retired.texture);
    return true;
  });
  if (!renderer.pipeline.IsValid())
    renderer.pipeline =
        device.CreatePipeline({0x494d4755494c4159ULL, 0x494d475549534844ULL,
                               nexora::rhi::TextureFormat::Rgba8Unorm, "Editor ImGui"});
  if (!renderer.font_texture.IsValid() || renderer.font_generation != state_->font_generation) {
    if (renderer.font_texture.IsValid()) {
      std::uint64_t last_use = 0;
      for (const auto &slot : renderer.upload_slots)
        last_use = std::max(last_use, slot.completion);
      renderer.retired_textures.push_back({renderer.font_texture, last_use});
    }
    unsigned char *pixels = nullptr;
    int atlas_width = 0, atlas_height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &atlas_width, &atlas_height);
    renderer.font_texture = device.CreateTexture(
        {static_cast<std::uint32_t>(atlas_width), static_cast<std::uint32_t>(atlas_height),
         nexora::rhi::TextureFormat::Rgba8Unorm, nexora::rhi::ResourceState::ShaderRead,
         "Editor ImGui font atlas"});
    device.WriteTextureRgba8(renderer.font_texture,
                             std::span{reinterpret_cast<const std::byte *>(pixels),
                                       static_cast<std::size_t>(atlas_width) * atlas_height * 4U},
                             static_cast<std::uint32_t>(atlas_width) * 4U);
    renderer.textures[0].texture = renderer.font_texture;
    renderer.textures[0].live = true;
    ImGui::GetIO().Fonts->SetTexID(static_cast<ImTextureID>(TextureId(0, 1)));
    renderer.font_generation = state_->font_generation;
    ++state_->renderer_metrics.font_rebuilds;
  }
  auto &upload = renderer.upload_slots[renderer.upload_slot];
  renderer.upload_slot = (renderer.upload_slot + 1) % renderer.upload_slots.size();
  if (upload.completion > device.CompletedSubmissionValue()) {
    device.WaitForSubmission(upload.completion);
    ++state_->renderer_metrics.completion_waits;
  }
  std::size_t total_vertex_bytes = 0, total_index_bytes = 0;
  for (int index = 0; index < draw->CmdListsCount; ++index) {
    total_vertex_bytes +=
        static_cast<std::size_t>(draw->CmdLists[index]->VtxBuffer.Size) * sizeof(ImDrawVert);
    total_index_bytes +=
        static_cast<std::size_t>(draw->CmdLists[index]->IdxBuffer.Size) * sizeof(ImDrawIdx);
  }
  const auto ensure_buffer = [&](nexora::rhi::BufferHandle &buffer, std::uint64_t &capacity,
                                 std::uint64_t required, std::string_view name) {
    if (capacity >= required)
      return;
    if (buffer.IsValid())
      device.DestroyBuffer(buffer);
    capacity = GrownCapacity(required);
    buffer = device.CreateBuffer({capacity, std::string(name)});
    ++state_->renderer_metrics.buffer_reallocations;
  };
  ensure_buffer(upload.vertices, upload.vertex_capacity, total_vertex_bytes,
                "Editor ImGui vertex ring");
  ensure_buffer(upload.indices, upload.index_capacity, total_index_bytes,
                "Editor ImGui index ring");
  auto commands = device.CreateCommandList(nexora::rhi::QueueType::Graphics);
  commands->Transition({target, before, nexora::rhi::ResourceState::RenderTarget});
  commands->BeginRendering({target, width, height});
  commands->BindPipeline(renderer.pipeline);
  std::uint32_t submitted = 0;
  std::uint64_t vertex_offset_bytes = 0, index_offset_bytes = 0;
  for (int list_index = 0; list_index < draw->CmdListsCount; ++list_index) {
    const auto *list = draw->CmdLists[list_index];
    if (list->VtxBuffer.empty() || list->IdxBuffer.empty())
      continue;
    const auto vertex_bytes = std::as_bytes(
        std::span{list->VtxBuffer.Data, static_cast<std::size_t>(list->VtxBuffer.Size)});
    const auto index_bytes = std::as_bytes(
        std::span{list->IdxBuffer.Data, static_cast<std::size_t>(list->IdxBuffer.Size)});
    device.WriteBuffer(upload.vertices, vertex_offset_bytes, vertex_bytes);
    device.WriteBuffer(upload.indices, index_offset_bytes, index_bytes);
    commands->BindVertexBuffer(upload.vertices, vertex_offset_bytes);
    commands->BindIndexBuffer(upload.indices,
                              sizeof(ImDrawIdx) == 2 ? nexora::rhi::IndexFormat::Uint16
                                                     : nexora::rhi::IndexFormat::Uint32,
                              index_offset_bytes);
    for (const auto &draw_command : list->CmdBuffer) {
      if (draw_command.UserCallback == nullptr && draw_command.ElemCount > 0) {
        const auto texture_id = static_cast<std::uint64_t>(draw_command.GetTexID());
        const auto texture_index = static_cast<std::uint32_t>(texture_id);
        const auto texture_generation = static_cast<std::uint32_t>(texture_id >> 32U);
        nexora::rhi::TextureHandle texture = renderer.font_texture;
        if (texture_index < renderer.textures.size() && renderer.textures[texture_index].live &&
            renderer.textures[texture_index].generation == texture_generation) {
          texture = renderer.textures[texture_index].texture;
        } else {
          ++state_->renderer_metrics.rejected_textures;
        }
        commands->BindTexture(0, texture);
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
    vertex_offset_bytes += vertex_bytes.size();
    index_offset_bytes += index_bytes.size();
  }
  commands->EndRendering();
  commands->Transition({target, nexora::rhi::ResourceState::RenderTarget,
                        prepare_for_present ? nexora::rhi::ResourceState::Present
                                            : nexora::rhi::ResourceState::ShaderRead});
  upload.completion = device.Submit(*commands);
  ++state_->renderer_metrics.frames;
  state_->renderer_metrics.draw_calls += submitted;
  return submitted;
}

void EditorImGuiHost::ReleaseRenderer(nexora::rhi::Device &device) {
  auto &renderer = state_->renderer;
  if (renderer.device == nullptr)
    return;
  if (renderer.device != &device)
    throw std::logic_error("Editor ImGui renderer belongs to another device");
  device.WaitIdle();
  for (auto &upload : renderer.upload_slots) {
    if (upload.vertices.IsValid())
      device.DestroyBuffer(upload.vertices);
    if (upload.indices.IsValid())
      device.DestroyBuffer(upload.indices);
  }
  for (const auto &retired : renderer.retired_textures)
    device.DestroyTexture(retired.texture);
  if (renderer.font_texture.IsValid())
    device.DestroyTexture(renderer.font_texture);
  if (renderer.pipeline.IsValid())
    device.DestroyPipeline(renderer.pipeline);
  renderer = {};
}

RendererMetrics EditorImGuiHost::GetRendererMetrics() const noexcept {
  return state_->renderer_metrics;
}

std::uint64_t EditorImGuiHost::RegisterTexture(nexora::rhi::Device &device,
                                               nexora::rhi::TextureHandle texture) {
  auto &renderer = state_->renderer;
  if (!texture.IsValid() || (renderer.device != nullptr && renderer.device != &device))
    return 0;
  renderer.device = &device;
  for (std::uint32_t index = 1; index < renderer.textures.size(); ++index) {
    auto &slot = renderer.textures[index];
    if (!slot.live) {
      slot.texture = texture;
      slot.live = true;
      return TextureId(index, slot.generation);
    }
  }
  renderer.textures.push_back({texture, 1, true});
  return TextureId(static_cast<std::uint32_t>(renderer.textures.size() - 1), 1);
}

bool EditorImGuiHost::UnregisterTexture(std::uint64_t texture_id) noexcept {
  const auto index = static_cast<std::uint32_t>(texture_id);
  const auto generation = static_cast<std::uint32_t>(texture_id >> 32U);
  auto &textures = state_->renderer.textures;
  if (index == 0 || index >= textures.size() || !textures[index].live ||
      textures[index].generation != generation)
    return false;
  textures[index].live = false;
  textures[index].texture = {};
  ++textures[index].generation;
  if (textures[index].generation == 0)
    textures[index].generation = 1;
  return true;
}

std::string EditorImGuiHost::SaveLayout() const {
  Activate(state_->context);
  std::size_t size = 0;
  const char *contents = ImGui::SaveIniSettingsToMemory(&size);
  return {contents, size};
}

bool EditorImGuiHost::LoadLayout(std::string_view layout) {
  if (layout.empty() || layout.find("[Window]") == std::string_view::npos)
    return false;
  Activate(state_->context);
  ImGui::LoadIniSettingsFromMemory(layout.data(), layout.size());
  state_->initial_dock_layout_built = true;
  return true;
}

Nexora::Presentation::SurfaceStatus
EditorImGuiHost::Render(Nexora::Presentation::RenderSurface &surface, std::uint32_t width,
                        std::uint32_t height) {
  Activate(state_->context);
  const auto *draw = ImGui::GetDrawData();
  if (draw == nullptr || width == 0 || height == 0)
    return Nexora::Presentation::SurfaceStatus::InvalidDescriptor;
  std::vector<Nexora::Presentation::UiVertex> vertices;
  std::vector<std::byte> indices;
  std::vector<Nexora::Presentation::UiDrawCommand> commands;
  vertices.reserve(static_cast<std::size_t>(draw->TotalVtxCount));
  indices.reserve(static_cast<std::size_t>(draw->TotalIdxCount) * sizeof(ImDrawIdx));
  std::uint32_t vertex_base = 0;
  std::uint32_t index_base = 0;
  for (int list_index = 0; list_index < draw->CmdListsCount; ++list_index) {
    const auto &list = *draw->CmdLists[list_index];
    for (const auto &vertex : list.VtxBuffer)
      vertices.push_back({{(vertex.pos.x - draw->DisplayPos.x) * draw->FramebufferScale.x,
                           (vertex.pos.y - draw->DisplayPos.y) * draw->FramebufferScale.y},
                          {vertex.uv.x, vertex.uv.y},
                          vertex.col});
    const auto index_bytes = std::as_bytes(
        std::span{list.IdxBuffer.Data, static_cast<std::size_t>(list.IdxBuffer.Size)});
    indices.insert(indices.end(), index_bytes.begin(), index_bytes.end());
    for (const auto &command : list.CmdBuffer) {
      if (command.UserCallback != nullptr || command.ElemCount == 0)
        continue;
      const auto left =
          std::max(0.0F, (command.ClipRect.x - draw->DisplayPos.x) * draw->FramebufferScale.x);
      const auto top =
          std::max(0.0F, (command.ClipRect.y - draw->DisplayPos.y) * draw->FramebufferScale.y);
      const auto right =
          std::min(static_cast<float>(width),
                   (command.ClipRect.z - draw->DisplayPos.x) * draw->FramebufferScale.x);
      const auto bottom =
          std::min(static_cast<float>(height),
                   (command.ClipRect.w - draw->DisplayPos.y) * draw->FramebufferScale.y);
      if (right <= left || bottom <= top)
        continue;
      commands.push_back({static_cast<std::int32_t>(left), static_cast<std::int32_t>(top),
                          static_cast<std::uint32_t>(right - left),
                          static_cast<std::uint32_t>(bottom - top),
                          static_cast<std::uint64_t>(command.GetTexID()), command.ElemCount,
                          index_base + command.IdxOffset,
                          static_cast<std::int32_t>(vertex_base + command.VtxOffset)});
    }
    vertex_base += static_cast<std::uint32_t>(list.VtxBuffer.Size);
    index_base += static_cast<std::uint32_t>(list.IdxBuffer.Size);
  }
  std::vector<Nexora::Presentation::UiTextureUpload> uploads;
  if (state_->surface_font_generation != state_->font_generation) {
    unsigned char *atlas = nullptr;
    int atlas_width = 0, atlas_height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&atlas, &atlas_width, &atlas_height);
    uploads.push_back({TextureId(0, 1), static_cast<std::uint32_t>(atlas_width),
                       static_cast<std::uint32_t>(atlas_height),
                       static_cast<std::uint32_t>(atlas_width) * 4U,
                       std::span{reinterpret_cast<const std::byte *>(atlas),
                                 static_cast<std::size_t>(atlas_width) * atlas_height * 4U}});
  }
  const auto status = surface.RenderUi(
      {vertices, indices, commands, uploads, sizeof(ImDrawIdx) == sizeof(std::uint32_t)});
  if (status == Nexora::Presentation::SurfaceStatus::Ready)
    state_->surface_font_generation = state_->font_generation;
  return status;
}

void EditorImGuiHost::UpdateImeCandidate(Nexora::Presentation::RenderSurface &surface) {
  Activate(state_->context);
  state_->surface = &surface;
}

FrameMetrics EditorImGuiHost::EndFrame() {
  Activate(state_->context);
  ImGui::Render();
  state_->surface = nullptr;
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

bool EditorImGuiHost::ApplyRecoveryChoice(ProjectWorkspace &workspace, RecoveryChoice choice) {
  if (choice == RecoveryChoice::None || state_->recovery_choice != RecoveryChoice::None)
    return false;
  std::string error;
  const bool succeeded = choice == RecoveryChoice::Recover ? workspace.RecoverWorkspace(&error)
                                                           : workspace.DiscardRecovery(&error);
  if (!succeeded) {
    state_->recovery_error = error.empty() ? "recovery operation failed" : std::move(error);
    return false;
  }
  state_->recovery_error.clear();
  state_->recovery_choice = choice;
  return true;
}

std::string_view EditorImGuiHost::RecoveryError() const noexcept { return state_->recovery_error; }
} // namespace nexora::editor::imgui
