#include "StartupSplash.h"
// clang-format off
#include <windows.h>
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
void Capture(HWND hwnd) {
  RECT r{};
  GetClientRect(hwnd, &r);
  auto dc = GetDC(hwnd);
  auto memory = CreateCompatibleDC(dc);
  auto bitmap = CreateCompatibleBitmap(dc, r.right, r.bottom);
  auto previous = SelectObject(memory, bitmap);
  Require(BitBlt(memory, 0, 0, r.right, r.bottom, dc, 0, 0, SRCCOPY) != 0, "Splash capture failed");
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
  ReleaseDC(hwnd, dc);
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
  std::ofstream output("editor-startup-splash.bmp", std::ios::binary);
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
  const auto main_window =
      CreateWindowExW(0, L"STATIC", L"Nexora Editor", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr,
                      nullptr, GetModuleHandleW(nullptr), nullptr);
  Require(main_window != nullptr, "Main fixture failed");
  StartupSplash startup(true);
  startup.PrepareMainWindow();
  Require(GetWindow(FindSplash(), GW_OWNER) == main_window, "Splash not owned above main window");
  Require(SendMessageW(main_window, WM_GETICON, ICON_SMALL, 0) != 0 &&
              SendMessageW(main_window, WM_GETICON, ICON_BIG, 0) != 0,
          "Window/taskbar icons missing");
  startup.Ready();
  DestroyWindow(main_window);
  std::cout
      << "Native splash pixels, DPI, lifecycle, exception cleanup and main-window icons passed\n";
  return 0;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
