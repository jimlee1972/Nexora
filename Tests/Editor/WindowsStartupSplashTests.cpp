#include "StartupSplash.h"
// clang-format off
#include <windows.h>
#include <dwmapi.h>
// clang-format on
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>
namespace {
void Require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
HWND FindSplash() {
  HWND found{};
  EnumWindows(
      [](HWND hwnd, LPARAM result) -> BOOL {
        DWORD process{};
        GetWindowThreadProcessId(hwnd, &process);
        wchar_t name[64]{};
        GetClassNameW(hwnd, name, 64);
        if (process == GetCurrentProcessId() && std::wstring_view(name) == L"NexoraEditorStartup")
          *reinterpret_cast<HWND *>(result) = hwnd;
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&found));
  return found;
}
HWND probe_main{};
bool probe_received{};
LRESULT CALLBACK ProbeProcedure(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
  if (message == WM_APP + 101) {
    probe_received = true;
    return 0;
  }
  return DefWindowProcW(hwnd, message, wp, lp);
}
LRESULT CALLBACK TeardownProbe(int code, WPARAM wp, LPARAM lp) {
  if (code >= 0 && reinterpret_cast<CWPSTRUCT *>(lp)->message == WM_DESTROY)
    SendMessageW(probe_main, WM_APP + 101, 0, 0);
  return CallNextHookEx(nullptr, code, wp, lp);
}
void Capture(HWND hwnd, const char *filename = "editor-startup-splash.bmp") {
  Require(SUCCEEDED(DwmFlush()), "Desktop composition flush failed");
  RECT r{};
  GetClientRect(hwnd, &r);
  POINT origin{};
  ClientToScreen(hwnd, &origin);
  // Screen pixels prove visibility above other windows, rather than offscreen painting.
  auto dc = GetDC(nullptr);
  auto memory = CreateCompatibleDC(dc);
  auto bitmap = CreateCompatibleBitmap(dc, r.right, r.bottom);
  auto previous = SelectObject(memory, bitmap);
  Require(BitBlt(memory, 0, 0, r.right, r.bottom, dc, origin.x, origin.y, SRCCOPY) != 0,
          "Splash capture failed");
  SelectObject(memory, previous);
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = r.right;
  info.bmiHeader.biHeight = -r.bottom;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  std::vector<unsigned char> pixels(static_cast<std::size_t>(r.right) *
                                    static_cast<std::size_t>(r.bottom) * 4);
  Require(GetDIBits(memory, bitmap, 0, static_cast<UINT>(r.bottom), pixels.data(), &info,
                    DIB_RGB_COLORS) != 0,
          "No splash pixels");
  DeleteObject(bitmap);
  DeleteDC(memory);
  ReleaseDC(nullptr, dc);
  unsigned cyan{}, white{};
  for (std::size_t i = 0; i < pixels.size(); i += 4) {
    if (pixels[i] > 200 && pixels[i + 1] > 160 && pixels[i + 2] < 120)
      ++cyan;
    if (pixels[i] > 230 && pixels[i + 1] > 230 && pixels[i + 2] > 230)
      ++white;
  }
  Require(cyan > 100 && white > 100, "Actual logo colors missing");
  Require(pixels[0] == 0 && pixels[1] == 0 && pixels[2] == 0, "Background not black");
  BITMAPFILEHEADER file{};
  file.bfType = 0x4D42;
  file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
  file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size());
  std::ofstream output(filename, std::ios::binary);
  output.write(reinterpret_cast<const char *>(&file), sizeof(file));
  output.write(reinterpret_cast<const char *>(&info.bmiHeader), sizeof(info.bmiHeader));
  output.write(reinterpret_cast<const char *>(pixels.data()),
               static_cast<std::streamsize>(pixels.size()));
  Require(static_cast<bool>(output), "Screenshot not retained");
}
} // namespace
int main() try {
  using namespace nexora::editor::app;
  Require(!FindSplash(), "Unexpected initial splash");
  {
    StartupSplash cli(false);
    cli.Stage(StartupStage::Settings);
    Require(!FindSplash(), "CLI splash");
  }
  {
    StartupSplash splash(true);
    const auto hwnd = FindSplash();
    Require(hwnd && IsWindowVisible(hwnd), "Native splash missing");
    Require((GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0,
            "Splash adds taskbar entry");
    Require(GetDpiForWindow(hwnd) >= 96, "Invalid DPI");
    Require(AreDpiAwarenessContextsEqual(GetWindowDpiAwarenessContext(hwnd),
                                         DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2),
            "Splash DPI awareness lost");
    Require(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),
                                         DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2),
            "Main owner DPI awareness lost");
    for (auto stage : {StartupStage::Settings, StartupStage::Project, StartupStage::Graphics,
                       StartupStage::Scene, StartupStage::Workspace})
      splash.Stage(stage);
    Capture(hwnd);
    splash.Ready();
    Require(!IsWindow(hwnd) && !FindSplash(), "Ready did not destroy/join splash");
    splash.Ready();
    splash.Stage(StartupStage::Settings);
    Require(!FindSplash(), "Splash reopened");
  }
  bool failure_ready = false;
  try {
    StartupSplash failed(true);
    Require(FindSplash() != nullptr, "Failure fixture missing");
    failure_ready = true;
    throw std::runtime_error("expected failure");
  } catch (const std::runtime_error &) {
  }
  Require(failure_ready && !FindSplash(), "Exception left orphan splash");
  const auto foreground = GetForegroundWindow();
  StartupSplash startup(true);
  const auto splash = FindSplash();
  Require(GetForegroundWindow() == foreground, "Splash stole foreground focus");
  Require((GetWindowLongPtrW(splash, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0,
          "Splash must not be globally topmost");
  Require(GetWindow(splash, GW_OWNER) == nullptr, "Splash has a cross-thread owner");
  startup.Stage(StartupStage::Graphics);
  RECT bounds{};
  GetWindowRect(splash, &bounds);
  const auto main_window = CreateWindowExW(
      0, L"STATIC", L"Nexora Editor", WS_OVERLAPPEDWINDOW, bounds.left - 100, bounds.top - 100,
      bounds.right - bounds.left + 200, bounds.bottom - bounds.top + 200, nullptr, nullptr,
      GetModuleHandleW(nullptr), nullptr);
  Require(main_window != nullptr && !IsWindowVisible(main_window), "Hidden main fixture failed");
  startup.PrepareMainWindow();
  Require(!IsWindowVisible(main_window) && GetWindow(splash, GW_OWNER) == nullptr,
          "Preparation showed the main or installed a cross-thread owner");
  Require(GetForegroundWindow() == foreground, "Preparation stole foreground");
  Capture(splash, "editor-startup-splash-hidden-main.bmp");
  Require(SendMessageW(main_window, WM_GETICON, ICON_SMALL, 0) != 0 &&
              SendMessageW(main_window, WM_GETICON, ICON_BIG, 0) != 0,
          "Window/taskbar icons missing");
  // Force a synchronous notification during splash destruction. An unconditional
  // main-thread join deadlocks here, even without cross-thread HWND ownership.
  const auto old_proc = reinterpret_cast<WNDPROC>(
      SetWindowLongPtrW(main_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ProbeProcedure)));
  probe_main = main_window;
  probe_received = false;
  const auto hook = SetWindowsHookExW(WH_CALLWNDPROC, TeardownProbe, nullptr,
                                      GetWindowThreadProcessId(splash, nullptr));
  Require(hook != nullptr, "Could not install synchronous teardown probe");
  startup.Ready();
  UnhookWindowsHookEx(hook);
  SetWindowLongPtrW(main_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(old_proc));
  Require(probe_received && !FindSplash(),
          "Teardown did not service synchronous main notification");
  failure_ready = false;
  probe_received = false;
  const auto failure_old_proc = reinterpret_cast<WNDPROC>(
      SetWindowLongPtrW(main_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ProbeProcedure)));
  HHOOK failure_hook{};
  try {
    StartupSplash failed_after_prepare(true);
    failed_after_prepare.PrepareMainWindow();
    Require(!IsWindowVisible(main_window) && GetWindow(FindSplash(), GW_OWNER) == nullptr,
            "Prepared failure exposed main or installed an owner");
    failure_hook = SetWindowsHookExW(WH_CALLWNDPROC, TeardownProbe, nullptr,
                                     GetWindowThreadProcessId(FindSplash(), nullptr));
    Require(failure_hook != nullptr, "Could not install failure teardown probe");
    failure_ready = true;
    throw std::runtime_error("expected prepared failure");
  } catch (const std::runtime_error &) {
  }
  if (failure_hook)
    UnhookWindowsHookEx(failure_hook);
  SetWindowLongPtrW(main_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(failure_old_proc));
  Require(failure_ready && probe_received && !FindSplash(),
          "Prepared failure did not service notification or left an orphan splash");
  DestroyWindow(main_window);
  std::cout << "Native screen pixels, hidden main, synchronous teardown, DPI, lifecycle and "
               "icons passed\n";
  return 0;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
