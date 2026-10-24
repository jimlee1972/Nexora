#pragma once
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <shobjidl.h>
#include <windows.h>
#include <wrl/client.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace nexora::editor::app {
inline std::optional<std::filesystem::path> PickProjectLocation(bool create, std::string &error) {
  error.clear();
  struct Apartment final {
    HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    ~Apartment() {
      if (SUCCEEDED(result))
        CoUninitialize();
    }
  } apartment;
  if (FAILED(apartment.result)) {
    error = "The Windows project chooser could not initialize its COM apartment.";
    return std::nullopt;
  }
  Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&dialog)))) {
    error = "The Windows project chooser is unavailable.";
    return std::nullopt;
  }
  FILEOPENDIALOGOPTIONS options{};
  HRESULT configured = dialog->GetOptions(&options);
  if (SUCCEEDED(configured))
    configured =
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR |
                           (create ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));
  if (SUCCEEDED(configured))
    configured = dialog->SetTitle(create ? L"Select project folder" : L"Open Nexora project");
  if (!create && SUCCEEDED(configured)) {
    const COMDLG_FILTERSPEC filter{L"Nexora project (project.nexora)", L"project.nexora"};
    configured = dialog->SetFileTypes(1, &filter);
    if (SUCCEEDED(configured))
      configured = dialog->SetFileName(L"project.nexora");
  }
  if (FAILED(configured)) {
    error = "The Windows project chooser could not configure its selection.";
    return std::nullopt;
  }
  HWND owner = GetActiveWindow();
  DWORD owner_process{};
  if (owner)
    GetWindowThreadProcessId(owner, &owner_process);
  if (owner_process != GetCurrentProcessId())
    owner = nullptr;
  const HRESULT shown = dialog->Show(owner);
  if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED))
    return std::nullopt;
  Microsoft::WRL::ComPtr<IShellItem> item;
  PWSTR raw{};
  if (FAILED(shown) || FAILED(dialog->GetResult(&item)) ||
      FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw))) {
    error = "The Windows project chooser could not return a filesystem selection.";
    return std::nullopt;
  }
  const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> storage(raw, &CoTaskMemFree);
  const std::filesystem::path selected(storage.get());
  if (create)
    return selected;
  if (_wcsicmp(selected.filename().c_str(), L"project.nexora") != 0) {
    error = "Select the project's project.nexora file.";
    return std::nullopt;
  }
  return selected.parent_path();
}
} // namespace nexora::editor::app
#endif
