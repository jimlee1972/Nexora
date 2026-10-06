#include "Nexora/EditorImGui/EditorImGui.h"

#include <imgui.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <utility>

namespace {
void *Allocate(std::size_t size, void *user) {
  auto *memory = std::malloc(size);
  if (memory != nullptr)
    ++*static_cast<std::size_t *>(user);
  return memory;
}
void Free(void *memory, void *user) {
  if (memory != nullptr) {
    auto &allocations = *static_cast<std::size_t *>(user);
    assert(allocations > 0);
    --allocations;
    std::free(memory);
  }
}
void Draw(nexora::editor::imgui::EditorImGuiHost &host) {
  host.BeginFrame();
  ImGui::Begin("Context lifetime###context.lifetime");
  ImGui::TextUnformatted("Moved context remains usable");
  ImGui::End();
  static_cast<void>(host.EndFrame());
}
} // namespace

int main() {
  using nexora::editor::imgui::EditorImGuiHost;
  assert(ImGui::GetCurrentContext() == nullptr);
  ImGuiMemAllocFunc original_allocate = nullptr;
  ImGuiMemFreeFunc original_free = nullptr;
  void *original_user = nullptr;
  ImGui::GetAllocatorFunctions(&original_allocate, &original_free, &original_user);
  std::size_t allocations = 0;
  ImGui::SetAllocatorFunctions(Allocate, Free, &allocations);
  {
    EditorImGuiHost destination;
    destination.SetDisplay(800, 600, 1);
    Draw(destination);
    {
      EditorImGuiHost source;
      source.SetDisplay(800, 600, 1.5F);
      Draw(source);
      auto *source_context = ImGui::GetCurrentContext();
      auto *source_platform = ImGui::GetIO().BackendPlatformUserData;
      destination = std::move(source);
      assert(ImGui::GetCurrentContext() == source_context);
      assert(ImGui::GetIO().BackendPlatformUserData == source_platform);
    }
    Draw(destination);
    assert(ImGui::GetIO().DisplayFramebufferScale.x == 1.5F);
    auto *retained_context = ImGui::GetCurrentContext();
    auto &alias = *std::addressof(destination);
    destination = std::move(alias);
    Draw(destination);
    assert(ImGui::GetCurrentContext() == retained_context);
    {
      EditorImGuiHost moved(std::move(destination));
      Draw(moved);
      assert(ImGui::GetCurrentContext() == retained_context);
      {
        EditorImGuiHost other;
        other.SetDisplay(640, 480, 2);
        Draw(moved);
        // Destroy a different host while this context is current.
      }
      assert(ImGui::GetCurrentContext() == retained_context);
      Draw(moved);
    }
    assert(ImGui::GetCurrentContext() == nullptr);
  }
  assert(ImGui::GetCurrentContext() == nullptr);
  std::printf("Outstanding ImGui allocations after context moves: %zu\n", allocations);
  assert(allocations == 0);
  ImGui::SetAllocatorFunctions(original_allocate, original_free, original_user);
}
