#include "GameEntry.h"
#include "Nexora/Core/Diagnostics.h"
#include "TwoDPlayer.h"
#include <cstdint>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
// clang-format off: shellapi requires Windows declarations first.
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif

namespace {
int RunImpl(const std::filesystem::path &executable, std::vector<std::string> arguments) {
  auto strings =
      nexora::player::GameArguments(executable, std::move(arguments), NEXORA_2D_GAME_PROJECT);
  std::vector<char *> pointers;
  for (auto &value : strings)
    pointers.push_back(value.data());
  return nexora::player::RunTwoDPlayer(static_cast<int>(pointers.size()), pointers.data());
}
int Run(const std::filesystem::path &executable, std::vector<std::string> arguments) {
  return nexora::core::RunDiagnosed("NexoraGame",
                                    [&] { return RunImpl(executable, std::move(arguments)); });
}
} // namespace

#if defined(_WIN32)
int WINAPI GameWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
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
    nexora::core::WriteDiagnostic(nexora::core::LogLevel::Error, "Game", error.what());
    std::cerr << error.what() << '\n';
  }
  if (result && !diagnostics) {
    const auto message = L"The game could not start. Diagnostic logs:\n" +
                         nexora::core::DiagnosticSession::Directory().wstring();
    MessageBoxW(nullptr, message.c_str(), L"Game startup failed", MB_OK | MB_ICONERROR);
  }
  return result;
}
#else
int GameMain(int argc, char **argv) try {
#if defined(__linux__)
  const auto executable = std::filesystem::read_symlink("/proc/self/exe");
#elif defined(__APPLE__)
  std::string path(1024, '\0');
  auto size = static_cast<std::uint32_t>(path.size());
  if (_NSGetExecutablePath(path.data(), &size) != 0) {
    path.resize(size);
    if (_NSGetExecutablePath(path.data(), &size) != 0)
      throw std::runtime_error("executable path unavailable");
  }
  path.resize(std::char_traits<char>::length(path.c_str()));
  const auto executable = std::filesystem::canonical(path);
#else
  const auto executable = std::filesystem::absolute(argv[0]);
#endif
  return Run(executable, {argv, argv + argc});
} catch (const std::exception &error) {
  nexora::core::WriteDiagnostic(nexora::core::LogLevel::Error, "Game", error.what());
  std::cerr << error.what() << '\n';
  return 2;
}
#endif

#if defined(_WIN32)
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command, int show) {
  return nexora::core::RunDiagnosed("NexoraGame",
                                    [&] { return GameWinMain(instance, previous, command, show); });
}
#else
int main(int argc, char **argv) {
  return nexora::core::RunDiagnosed("NexoraGame", [&] { return GameMain(argc, argv); });
}
#endif
