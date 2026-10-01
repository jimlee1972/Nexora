#include "EditorImGuiTestAccess.h"
#include "Nexora/EditorImGui/EditorImGui.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imm.h>

#include <cassert>

namespace {
POINT CandidatePosition(HIMC context) {
  CANDIDATEFORM candidate{};
  assert(ImmGetCandidateWindow(context, 0, &candidate));
  assert(candidate.dwStyle == CFS_CANDIDATEPOS);
  return candidate.ptCurrentPos;
}
} // namespace

int main() {
  using nexora::editor::imgui::EditorImGuiHost;
  using nexora::editor::imgui::EditorImGuiTestAccess;
  using namespace Nexora;

  constexpr wchar_t title[] = L"Nexora Editor DPI IME contract";
  auto created = Presentation::CreateRenderSurface(
      {"Nexora Editor DPI IME contract", 640, 360, true, Presentation::SurfaceBackend::Dx12,
       Presentation::PresentMode::Immediate, Presentation::ColorSpace::Srgb});
  assert(created);
  auto window = FindWindowW(L"Nexora.Window", title);
  assert(window);
  DWORD windowProcess = 0;
  GetWindowThreadProcessId(window, &windowProcess);
  assert(windowProcess == GetCurrentProcessId());

  auto inputContext = ImmGetContext(window);
  HIMC ownedInputContext = nullptr;
  HIMC previousInputContext = nullptr;
  if (!inputContext) {
    ownedInputContext = ImmCreateContext();
    assert(ownedInputContext);
    previousInputContext = ImmAssociateContext(window, ownedInputContext);
    inputContext = ImmGetContext(window);
  }
  assert(inputContext);

  {
    EditorImGuiHost host;
    host.SetDisplay(640.0F, 360.0F, 1.0F);
    host.BeginFrame();
    host.UpdateImeCandidate(*created.surface);
    EditorImGuiTestAccess::InvokeImeCallback(host, 40.0F, 60.0F, true);
    auto candidate = CandidatePosition(inputContext);
    assert(candidate.x == 40 && candidate.y == 60);
    static_cast<void>(host.EndFrame());

    host.SetDisplay(640.0F / 1.5F, 360.0F / 1.5F, 1.5F);
    host.BeginFrame();
    host.UpdateImeCandidate(*created.surface);
    EditorImGuiTestAccess::InvokeImeCallback(host, 40.0F, 60.0F, true);
    candidate = CandidatePosition(inputContext);
    assert(candidate.x == 60 && candidate.y == 90);

    EditorImGuiTestAccess::InvokeImeCallback(host, 10.0F, 20.0F, false);
    candidate = CandidatePosition(inputContext);
    assert(candidate.x == 60 && candidate.y == 90);
    static_cast<void>(host.EndFrame());

    EditorImGuiTestAccess::InvokeImeCallback(host, 80.0F, 80.0F, true);
    candidate = CandidatePosition(inputContext);
    assert(candidate.x == 60 && candidate.y == 90);
  }

  ImmReleaseContext(window, inputContext);
  if (ownedInputContext) {
    ImmAssociateContext(window, previousInputContext);
    assert(ImmDestroyContext(ownedInputContext));
  }
  assert(created.surface->DrainAndDestroy() == Presentation::SurfaceStatus::Ready);
}
