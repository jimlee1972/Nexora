#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>

namespace nexora::editor::imgui::detail {

inline bool IsFrameModifier(ImGuiKey key) {
  return key == ImGuiMod_Ctrl || key == ImGuiMod_Shift || key == ImGuiMod_Alt ||
         key == ImGuiMod_Super || (key >= ImGuiKey_LeftCtrl && key <= ImGuiKey_RightSuper) ||
         (key >= ImGuiKey_ReservedForModCtrl && key <= ImGuiKey_ReservedForModSuper);
}

inline int ModifierFramePrefix(ImGuiContext &context) {
  const auto &queue = context.InputEventsQueue;
  if (!context.IO.ConfigInputTrickleEventQueue ||
      (context.IO.ConfigFlags & ImGuiConfigFlags_NoKeyboard))
    return queue.Size;
  // Focus loss retains the vendor's immediate cancellation path, never an extra frame lease.
  for (const auto &event : queue)
    if (event.Type == ImGuiInputEventType_Focus && !event.AppFocused.Focused)
      return queue.Size;

  std::array<bool, ImGuiKey_NamedKey_COUNT> down{};
  for (std::size_t i = 0; i < down.size(); ++i)
    down[i] = context.IO.KeysData[i].Down;
  bool pressed = false;
  for (int i = 0; i < queue.Size; ++i) {
    const auto &event = queue[i];
    if (event.Type != ImGuiInputEventType_Key || event.Source != ImGuiInputSource_Keyboard)
      continue;
    const auto index = ImGui::GetKeyData(&context, event.Key.Key) - context.IO.KeysData;
    IM_ASSERT(index >= 0 && index < ImGuiKey_NamedKey_COUNT);
    const bool changed = down[static_cast<std::size_t>(index)] != event.Key.Down;
    const bool modifier = IsFrameModifier(event.Key.Key);
    // NewFrame derives modifiers after consuming the queue. A later transition must not
    // change the meaning of a key-down edge already admitted to this frame, in either direction.
    if (pressed && modifier && changed)
      return i;
    if (!modifier && changed && event.Key.Down)
      pressed = true;
    down[static_cast<std::size_t>(index)] = event.Key.Down;
  }
  return queue.Size;
}

inline void BeginModifierSafeFrame() {
  auto &context = *ImGui::GetCurrentContext();
  auto &queue = context.InputEventsQueue;
  const int prefix = ModifierFramePrefix(context);
  ImVector<ImGuiInputEvent> later;
  if (prefix < queue.Size) {
    later.resize(queue.Size - prefix);
    std::copy(queue.begin() + prefix, queue.end(), later.begin());
    queue.resize(prefix);
  }
  ImGui::NewFrame();
  // Vendor trickling may leave part of the prefix queued. Keep that remainder ahead of the
  // untouched tail, retaining event IDs, text and mouse ordering in the existing owned queue.
  for (const auto &event : later)
    queue.push_back(event);
}

} // namespace nexora::editor::imgui::detail
