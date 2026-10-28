#include "StartupSplash.h"
#include <cstdlib>
#include <iostream>
#if defined(_WIN32) && defined(NEXORA_EDITOR_GRAPHICAL_SHELL)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <future>
#include <string_view>
#include <thread>
// clang-format off
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
// clang-format on
namespace nexora::editor::app {
namespace {
constexpr UINT kStageMessage = WM_APP + 17, kCloseMessage = WM_APP + 18;
constexpr wchar_t kClass[] = L"NexoraEditorStartup";
const wchar_t *Text(StartupStage stage) {
  switch (stage) {
  case StartupStage::Settings:
    return L"Loading settings";
  case StartupStage::Graphics:
    return L"Creating graphics device";
  case StartupStage::Project:
    return L"Opening project and indexing content";
  case StartupStage::Scene:
    return L"Preparing scene";
  case StartupStage::Workspace:
    return L"Preparing workspace";
  }
  return L"Starting Editor";
}
bool EvidenceEnabled() noexcept {
  return GetEnvironmentVariableW(L"NEXORA_EDITOR_STARTUP_EVIDENCE", nullptr, 0) != 0;
}
void Evidence(const char *event) {
  if (EvidenceEnabled()) {
    std::cerr << "editor splash: " << event << '\n';
    std::cerr << "editor splash-clock: tick=" << static_cast<DWORD>(GetTickCount64())
              << " event=" << event << '\n';
  }
}
} // namespace
struct StartupSplash::Native final {
  HWND window{};
  std::thread thread;
  HANDLE stopped = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  StartupStage stage = StartupStage::Settings;
  std::unique_ptr<Gdiplus::Bitmap> logo;
  IStream *stream{};
  void Paint(HWND hwnd) {
    PAINTSTRUCT paint{};
    auto dc = BeginPaint(hwnd, &paint);
    RECT rect{};
    GetClientRect(hwnd, &rect);
    Gdiplus::Bitmap buffer(rect.right, rect.bottom, PixelFormat32bppPARGB);
    Gdiplus::Graphics canvas(&buffer);
    canvas.Clear(Gdiplus::Color(255, 0, 0, 0));
    canvas.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    const float scale = static_cast<float>(rect.right) / 640.0F;
    canvas.DrawImage(logo.get(),
                     Gdiplus::RectF(64 * scale, 92 * scale, 512 * scale, 512 * scale * 250 / 1160));
    Gdiplus::FontFamily family(L"Segoe UI");
    Gdiplus::Font font(&family, 14 * scale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush brush(Gdiplus::Color(255, 178, 186, 194));
    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    canvas.DrawString(Text(stage), -1, &font,
                      Gdiplus::RectF(20 * scale, 258 * scale, 600 * scale, 48 * scale), &format,
                      &brush);
    canvas.Flush(Gdiplus::FlushIntentionSync);
    {
      Gdiplus::Graphics target(dc);
      target.DrawImage(&buffer, 0, 0);
    }
    EndPaint(hwnd, &paint);
  }
  static LRESULT CALLBACK Procedure(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    auto self = reinterpret_cast<Native *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      self = static_cast<Native *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self)
      return DefWindowProcW(hwnd, message, wp, lp);
    switch (message) {
    case WM_PAINT:
      self->Paint(hwnd);
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case kStageMessage:
      self->stage = static_cast<StartupStage>(wp);
      InvalidateRect(hwnd, nullptr, FALSE);
      UpdateWindow(hwnd);
      return 0;
    case WM_DPICHANGED: {
      auto rect = reinterpret_cast<const RECT *>(lp);
      SetWindowPos(hwnd, nullptr, rect->left, rect->top, rect->right - rect->left,
                   rect->bottom - rect->top, SWP_NOACTIVATE | SWP_NOZORDER);
      return 0;
    }
    case WM_CLOSE:
      return 0;
    case kCloseMessage:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(hwnd, message, wp, lp);
    }
  }
  void Run(std::promise<HWND> signal) {
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    ULONG_PTR token{};
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) {
      signal.set_value(nullptr);
      return;
    }
    HWND hwnd{};
    try {
      const auto module = GetModuleHandleW(nullptr);
      const auto resource = FindResourceW(module, MAKEINTRESOURCEW(102), MAKEINTRESOURCEW(10));
      const auto size = resource ? SizeofResource(module, resource) : 0;
      const auto data = resource ? LockResource(LoadResource(module, resource)) : nullptr;
      if (data && size && CreateStreamOnHGlobal(nullptr, TRUE, &stream) == S_OK) {
        ULONG written{};
        if (stream->Write(data, size, &written) == S_OK && written == size) {
          LARGE_INTEGER zero{};
          stream->Seek(zero, STREAM_SEEK_SET, nullptr);
          logo.reset(Gdiplus::Bitmap::FromStream(stream));
        }
      }
      if (logo && logo->GetLastStatus() == Gdiplus::Ok) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = Procedure;
        wc.hInstance = module;
        wc.lpszClassName = kClass;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        if (RegisterClassW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
          POINT pointer{};
          GetCursorPos(&pointer);
          MONITORINFO monitor{sizeof(MONITORINFO)};
          GetMonitorInfoW(MonitorFromPoint(pointer, MONITOR_DEFAULTTOPRIMARY), &monitor);
          // The main host remains hidden through initialization. This window never
          // owns or is owned by another thread's HWND, and never activates.
          hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kClass,
                                 L"Nexora Editor Startup", WS_POPUP, monitor.rcWork.left,
                                 monitor.rcWork.top, 640, 360, nullptr, nullptr, module, this);
          if (hwnd) {
            const int dpi = static_cast<int>(GetDpiForWindow(hwnd));
            const int width = std::max(
                1, std::min(MulDiv(640, dpi, 96),
                            std::min(static_cast<int>(monitor.rcWork.right - monitor.rcWork.left),
                                     static_cast<int>(monitor.rcWork.bottom - monitor.rcWork.top) *
                                         16 / 9)));
            const int height = width * 9 / 16;
            SetWindowPos(
                hwnd, nullptr,
                monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - width) / 2,
                monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2,
                width, height, SWP_NOZORDER | SWP_NOACTIVATE);
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
            UpdateWindow(hwnd);
          }
        }
      }
    } catch (...) {
      if (hwnd) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
      }
    }
    signal.set_value(hwnd);
    if (hwnd) {
      MSG message{};
      while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
      }
      if (IsWindow(hwnd))
        DestroyWindow(hwnd);
    }
    logo.reset();
    if (stream) {
      stream->Release();
      stream = nullptr;
    }
    Gdiplus::GdiplusShutdown(token);
  }
  ~Native() {
    if (window)
      PostMessageW(window, kCloseMessage, 0, 0);
    if (thread.joinable()) {
      // Win32 teardown/activation can synchronously notify another UI thread.
      // Service only sent messages while waiting: do not dispatch posted Editor
      // commands or reenter its initialization/render loop during RAII teardown.
      const HANDLE handle = stopped;
      while (MsgWaitForMultipleObjectsEx(1, &handle, INFINITE, QS_SENDMESSAGE,
                                         MWMO_INPUTAVAILABLE) == WAIT_OBJECT_0 + 1) {
        MSG message{};
        PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
      }
      thread.join();
    }
    if (stopped)
      CloseHandle(stopped);
    if (window)
      Evidence("closed");
  }
};
StartupSplash::StartupSplash(bool enabled) {
  if (!enabled)
    return;
  // Match the existing Window adapter before the process creates its first HWND.
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  native_ = std::make_unique<Native>();
  if (!native_->stopped) {
    std::cerr << "startup splash event unavailable; continuing with normal Editor diagnostics\n";
    native_.reset();
    return;
  }
  std::promise<HWND> signal;
  auto ready = signal.get_future();
  native_->thread = std::thread([owner = native_.get(), signal = std::move(signal)]() mutable {
    owner->Run(std::move(signal));
    SetEvent(owner->stopped);
  });
  native_->window = ready.get();
  if (native_->window)
    Evidence("opened");
  else {
    std::cerr << "startup splash unavailable; continuing with normal Editor diagnostics\n";
    native_.reset();
  }
}
StartupSplash::~StartupSplash() = default;
void StartupSplash::Stage(StartupStage stage) {
  if (!native_)
    return;
  PostMessageW(native_->window, kStageMessage, static_cast<WPARAM>(stage), 0);
  if (EvidenceEnabled())
    std::cerr << "editor splash: stage=" << static_cast<unsigned>(stage) << '\n';
}
void StartupSplash::Ready() {
  if (native_) {
    Evidence("main-frame-ready");
    native_.reset();
  }
}
void StartupSplash::PrepareMainWindow() {
  EnumThreadWindows(
      GetCurrentThreadId(),
      [](HWND hwnd, LPARAM) -> BOOL {
        wchar_t title[64]{};
        GetWindowTextW(hwnd, title, 64);
        if (std::wstring_view(title) != L"Nexora Editor")
          return TRUE;
        // No cross-thread HWND ownership: DestroyWindow must not wait for a
        // main owner whose Ready()/failure cleanup is waiting for this thread.
        if (!IsWindowVisible(hwnd))
          Evidence("main-window-hidden");
        for (const auto type : {ICON_SMALL, ICON_BIG}) {
          const int size = GetSystemMetricsForDpi(type == ICON_SMALL ? SM_CXSMICON : SM_CXICON,
                                                  GetDpiForWindow(hwnd));
          auto icon = LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON, size,
                                 size, LR_SHARED);
          SendMessageW(hwnd, WM_SETICON, static_cast<WPARAM>(type), reinterpret_cast<LPARAM>(icon));
        }
        return TRUE;
      },
      0);
}
} // namespace nexora::editor::app
#else
namespace nexora::editor::app {
struct StartupSplash::Native {};
StartupSplash::StartupSplash(bool) {}
StartupSplash::~StartupSplash() = default;
void StartupSplash::Stage(StartupStage) {}
void StartupSplash::PrepareMainWindow() {}
void StartupSplash::Ready() {}
} // namespace nexora::editor::app
#endif
