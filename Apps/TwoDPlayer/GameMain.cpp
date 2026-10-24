#include "GameEntry.h"
#include "TwoDPlayer.h"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
// clang-format off: shellapi requires Windows declarations first.
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif

namespace {
int Run(const std::filesystem::path &executable, std::vector<std::string> arguments) {
  auto strings =
      nexora::player::GameArguments(executable, std::move(arguments), NEXORA_2D_GAME_PROJECT);
  std::vector<char *> pointers;
  for (auto &value : strings)
    pointers.push_back(value.data());
  return nexora::player::RunTwoDPlayer(static_cast<int>(pointers.size()), pointers.data());
}
} // namespace

#if defined(_WIN32)
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  bool diagnostics = false;
  int result = 2;
  try {
    std::wstring executable(32768, L'\0');
    const auto size =
        GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!size || size >= executable.size())
      throw std::runtime_error("executable path unavailable");
    executable.resize(size);
    int count{};
    auto **wide = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!wide)
      throw std::runtime_error("command line unavailable");
    std::vector<std::string> arguments;
    try {
      for (int i = 0; i < count; ++i) {
        const auto utf8 = std::filesystem::path(wide[i]).u8string();
        arguments.emplace_back(utf8.begin(), utf8.end());
        diagnostics |= arguments.back() == "--frames" || arguments.back() == "--report";
      }
    } catch (...) {
      LocalFree(wide);
      throw;
    }
    LocalFree(wide);
    result = Run(std::filesystem::path(executable), std::move(arguments));
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
  }
  if (result && !diagnostics)
    MessageBoxW(nullptr, L"The game could not start. Keep the complete game folder together.",
                L"Game startup failed", MB_OK | MB_ICONERROR);
  return result;
}
#else
int main(int argc, char **argv) try {
  return Run(std::filesystem::absolute(argv[0]), {argv, argv + argc});
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 2;
}
#endif
