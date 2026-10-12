#include <cerrno>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
#if !defined(_WIN32)
volatile std::sig_atomic_t stopped{};
void Stop(int) { stopped = 1; }
#endif
int Run(const std::vector<std::string> &arguments) {
  if (arguments.size() < 2)
    return 2;
  const auto &mode = arguments[1];
  if (mode == "--args") {
    for (std::size_t i = 2; i < arguments.size(); ++i)
      std::cout << arguments[i].size() << ':' << arguments[i] << '\n';
    const auto directory = std::filesystem::current_path().generic_u8string();
    std::cout << "CWD=" << std::string(directory.begin(), directory.end()) << '\n';
    std::cerr << "STDERR_END\n";
    return 0;
  }
#if !defined(_WIN32)
  if (mode == "--fd" && arguments.size() == 3) {
    const auto fd = std::stoi(arguments[2]);
    if (fcntl(fd, F_GETFD) != -1 || errno != EBADF)
      return 9;
    std::cout << "FD_CLOSED\n";
    return 0;
  }
#endif
  if (mode == "--binary-output") {
    constexpr char bytes[]{'A', '\0', static_cast<char>(0xff), '\x1b', '[', '3', '1', 'm', '\n'};
    std::cout.write(bytes, sizeof(bytes));
    return 0;
  }
  if (mode == "--fail") {
    std::cerr << "ACTUAL_NONZERO_EXIT\n";
    return 7;
  }
  if (mode == "--flood") {
    const std::string block(4096, 'x');
    for (int i = 0; i < 512; ++i)
      std::cout << block;
    std::cout.flush();
    std::cerr << "FLOOD_END\n";
    return 0;
  }
  if (mode == "--sleep") {
    std::cout << "READY\n" << std::flush;
    std::this_thread::sleep_for(std::chrono::seconds(20));
    return 0;
  }
  if (mode == "--native-failure") {
#if defined(_WIN32)
    ExitProcess(0xc0000005u);
#else
    std::raise(SIGTERM);
    return 4;
#endif
  }
  if (mode == "--tree") {
#if defined(_WIN32)
    std::wstring executable(32768, L'\0');
    const auto length =
        GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size())
      return 3;
    executable.resize(length);
    auto command = L"\"" + executable + L"\" --sleep";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION child{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child))
      return 3;
    std::cout << "CHILD_PID=" << child.dwProcessId << '\n' << std::flush;
    WaitForSingleObject(child.hProcess, INFINITE);
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    return 0;
#else
    const auto child = fork();
    if (child < 0)
      return 3;
    if (!child) {
      execl(arguments[0].c_str(), arguments[0].c_str(), "--sleep", static_cast<char *>(nullptr));
      _exit(3);
    }
    std::signal(SIGTERM, Stop);
    std::cout << "CHILD_PID=" << child << '\n' << std::flush;
    while (!stopped)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    int status{};
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
    return 0;
#endif
  }
  return 2;
}
} // namespace
#if defined(_WIN32)
int wmain(int argc, wchar_t **argv) {
  // Define exact fixture bytes independently of the Windows CRT's default CRLF text mode.
  if (_setmode(_fileno(stdout), _O_BINARY) == -1 || _setmode(_fileno(stderr), _O_BINARY) == -1)
    return 4;
  std::vector<std::string> arguments;
  for (int i = 0; i < argc; ++i) {
    const auto size = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
    if (!size)
      return 4;
    std::string value(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, value.data(), size, nullptr, nullptr);
    value.pop_back();
    arguments.push_back(std::move(value));
  }
  return Run(arguments);
}
#else
int main(int argc, char **argv) { return Run({argv, argv + argc}); }
#endif
