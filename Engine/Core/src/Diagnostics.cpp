#include "Nexora/Core/Diagnostics.h"
#include "Nexora/Foundation/BuildInfo.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// clang-format off: platform helper headers require Windows declarations first.
#include <Windows.h>
#include <DbgHelp.h>
#include <TlHelp32.h>
// clang-format on
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <ucontext.h>
#include <unistd.h>
#endif

namespace nexora::core {
namespace {
constexpr std::size_t kFileLimit = 4 * 1024 * 1024;
thread_local char current_context[256]{};
#if defined(_WIN32)
using File = HANDLE;
const File kInvalidFile = INVALID_HANDLE_VALUE;
File crash_file = kInvalidFile;
File dump_file = kInvalidFile;
using DumpWriter = BOOL(WINAPI *)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
                                  PMINIDUMP_EXCEPTION_INFORMATION,
                                  PMINIDUMP_USER_STREAM_INFORMATION,
                                  PMINIDUMP_CALLBACK_INFORMATION);
DumpWriter dump_writer{};
HMODULE dbghelp{};
LPTOP_LEVEL_EXCEPTION_FILTER previous_filter{};
bool installed_filter{};
constexpr std::array kSignals{SIGABRT, SIGSEGV, SIGILL, SIGFPE};
using SignalHandler = void (*)(int);
std::array<SignalHandler, kSignals.size()> previous_signals{};
std::size_t installed_signals{};
File Open(const std::filesystem::path &path) {
  return CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                     FILE_ATTRIBUTE_NORMAL, nullptr);
}
void Close(File file) {
  if (file != kInvalidFile)
    CloseHandle(file);
}
bool Write(File file, const char *bytes, std::size_t count) noexcept {
  while (count) {
    DWORD written{};
    if (!WriteFile(file, bytes, static_cast<DWORD>(count), &written, nullptr) || !written)
      return false;
    bytes += written;
    count -= written;
  }
  return true;
}
void Sync(File file) noexcept {
  if (file != kInvalidFile)
    FlushFileBuffers(file);
}
#else
using File = int;
constexpr File kInvalidFile = -1;
volatile std::sig_atomic_t crash_file = -1;
constexpr std::array kSignals{SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE};
std::array<struct sigaction, kSignals.size()> previous_signals{};
std::size_t installed_signals{};
std::array<char, 64 * 1024> alternate_stack{};
stack_t previous_stack{};
bool installed_stack{};
File Open(const std::filesystem::path &path) {
  return open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR);
}
void Close(File file) {
  if (file != kInvalidFile)
    close(file);
}
bool Write(File file, const char *bytes, std::size_t count) noexcept {
  while (count) {
    const auto written = write(file, bytes, count);
    if (written < 0 && errno == EINTR)
      continue;
    if (written <= 0)
      return false;
    bytes += written;
    count -= static_cast<std::size_t>(written);
  }
  return true;
}
void Sync(File file) noexcept {
  if (file != kInvalidFile)
    (void)fsync(file);
}
#endif

// Only raw OS writes and stack-owned formatting are used on the fatal path.
void Emergency(const char *kind, std::uintptr_t code, std::uintptr_t address,
               std::uintptr_t instruction) noexcept {
  char output[512];
  std::size_t size{};
  const auto append = [&](const char *s) {
    while (*s && size < sizeof(output))
      output[size++] = *s++;
  };
  const auto hex = [&](std::uintptr_t value) {
    append("0x");
    char digits[2 * sizeof(value)];
    std::size_t n{};
    do {
      digits[n++] = "0123456789abcdef"[value & 15];
      value >>= 4;
    } while (value);
    while (n)
      output[size++] = digits[--n];
  };
  append("fatal kind=");
  append(kind);
  append(" code=");
  hex(code);
  append(" address=");
  hex(address);
  append(" instruction=");
  hex(instruction);
  append(" context=");
  for (std::size_t i = 0;
       i < sizeof(current_context) && current_context[i] && size + 1 < sizeof(output); ++i)
    output[size++] = current_context[i];
  append("\n");
  const File file = crash_file;
  if (file != kInvalidFile) {
    (void)Write(file, output, size);
    Sync(file);
  }
  // GUI applications may have no stderr; the preopened crash file is authoritative.
#if defined(_WIN32)
  const auto error = GetStdHandle(STD_ERROR_HANDLE);
#else
  const auto error = STDERR_FILENO;
#endif
  (void)Write(error, output, size);
}

#if defined(_WIN32)
LONG WINAPI OnException(EXCEPTION_POINTERS *exception) noexcept {
  Emergency("seh", exception->ExceptionRecord->ExceptionCode,
            reinterpret_cast<std::uintptr_t>(exception->ExceptionRecord->ExceptionAddress),
            reinterpret_cast<std::uintptr_t>(exception->ExceptionRecord->ExceptionAddress));
  if (dump_writer && dump_file != kInvalidFile) {
    MINIDUMP_EXCEPTION_INFORMATION info{GetCurrentThreadId(), exception, FALSE};
    const auto ok = dump_writer(GetCurrentProcess(), GetCurrentProcessId(), dump_file,
                                MiniDumpNormal, &info, nullptr, nullptr);
    const char *status = ok ? "minidump=written\n" : "minidump=failed\n";
    (void)Write(crash_file, status, std::strlen(status));
    Sync(dump_file);
    Sync(crash_file);
  }
  if (previous_filter)
    return previous_filter(exception);
  return EXCEPTION_EXECUTE_HANDLER;
}
void OnCrtSignal(int signal) noexcept {
  Emergency("crt-signal", static_cast<std::uintptr_t>(signal), 0, 0);
  if (dump_writer && dump_file != kInvalidFile) {
    CONTEXT context{};
    RtlCaptureContext(&context);
    EXCEPTION_RECORD record{};
    record.ExceptionCode = 0x40000015; // STATUS_FATAL_APP_EXIT
    EXCEPTION_POINTERS pointers{&record, &context};
    MINIDUMP_EXCEPTION_INFORMATION info{GetCurrentThreadId(), &pointers, FALSE};
    (void)dump_writer(GetCurrentProcess(), GetCurrentProcessId(), dump_file, MiniDumpNormal, &info,
                      nullptr, nullptr);
    Sync(dump_file);
  }
  std::_Exit(128 + signal);
}
#else
void OnSignal(int signal, siginfo_t *info, void *context) noexcept {
  std::uintptr_t instruction{};
#if defined(__linux__) && defined(__x86_64__)
  instruction =
      static_cast<std::uintptr_t>(static_cast<ucontext_t *>(context)->uc_mcontext.gregs[REG_RIP]);
#elif defined(__linux__) && defined(__aarch64__)
  instruction = static_cast<ucontext_t *>(context)->uc_mcontext.pc;
#elif defined(__APPLE__) && defined(__x86_64__)
  instruction = static_cast<ucontext_t *>(context)->uc_mcontext->__ss.__rip;
#elif defined(__APPLE__) && defined(__aarch64__)
  instruction = static_cast<ucontext_t *>(context)->uc_mcontext->__ss.__pc;
#else
  (void)context;
#endif
  Emergency("signal", static_cast<std::uintptr_t>(signal),
            info && info->si_code > 0 ? reinterpret_cast<std::uintptr_t>(info->si_addr) : 0,
            instruction);
  // Preserve abnormal termination and the OS core/native crash reporting policy.
  struct sigaction action{};
  action.sa_handler = SIG_DFL;
  sigemptyset(&action.sa_mask);
  sigaction(signal, &action, nullptr);
  sigset_t unblocked;
  sigemptyset(&unblocked);
  sigaddset(&unblocked, signal);
  sigprocmask(SIG_UNBLOCK, &unblocked, nullptr);
  raise(signal);
  _exit(128 + signal);
}
#endif

[[noreturn]] void OnTerminate() noexcept {
  Emergency("terminate", 0, 0, 0);
  std::abort();
}

struct State final {
  std::mutex mutex;
  std::size_t owners{}, bytes{}, crash_bytes{};
  File log{kInvalidFile};
  std::filesystem::path directory;
  std::map<std::string, std::string> metadata;
  std::terminate_handler previous_terminate{};
};
State &Global() {
  static State state;
  return state;
}

#if defined(_WIN32)
std::filesystem::path EnvironmentPath(const wchar_t *name) {
  const auto size = GetEnvironmentVariableW(name, nullptr, 0);
  if (!size)
    return {};
  std::wstring value(size, L'\0');
  const auto written = GetEnvironmentVariableW(name, value.data(), size);
  if (!written || written >= size)
    return {};
  value.resize(written);
  return std::filesystem::path(value);
}
#endif
std::filesystem::path Root() {
#if defined(_WIN32)
  if (const auto override_root = EnvironmentPath(L"NEXORA_LOG_DIR"); !override_root.empty())
    return override_root;
#else
  if (const auto *override_root = std::getenv("NEXORA_LOG_DIR"); override_root && *override_root)
    return std::filesystem::path(reinterpret_cast<const char8_t *>(override_root));
#endif
#if defined(_WIN32)
  if (const auto local = EnvironmentPath(L"LOCALAPPDATA"); !local.empty())
    return local / "Nexora" / "Logs";
#else
#if defined(__APPLE__)
  if (const auto *home = std::getenv("HOME"); home && *home)
    return std::filesystem::path(home) / "Library" / "Logs" / "Nexora";
#else
  if (const auto *state = std::getenv("XDG_STATE_HOME"); state && *state)
    if (std::filesystem::path(state).is_absolute())
      return std::filesystem::path(state) / "Nexora" / "Logs";
  if (const auto *home = std::getenv("HOME"); home && *home)
    return std::filesystem::path(home) / ".local" / "state" / "Nexora" / "Logs";
#endif
#endif
  return std::filesystem::temp_directory_path() / "Nexora" / "Logs";
}

std::string Escaped(std::string_view input, std::size_t limit) {
  std::string result;
  constexpr char digits[] = "0123456789abcdef";
  for (unsigned char c : input.substr(0, limit)) {
    if (c < 32 || c == 127 || c == '\\') {
      result += "\\x";
      result += digits[c >> 4];
      result += digits[c & 15];
    } else
      result += static_cast<char>(c);
  }
  if (input.size() > limit)
    result += " [truncated]";
  return result;
}

void Restore() noexcept {
#if defined(_WIN32)
  if (installed_filter)
    SetUnhandledExceptionFilter(previous_filter);
  installed_filter = false;
  while (installed_signals) {
    --installed_signals;
    (void)std::signal(kSignals[installed_signals], previous_signals[installed_signals]);
  }
  dump_writer = nullptr;
  Close(dump_file);
  dump_file = kInvalidFile;
  if (dbghelp)
    FreeLibrary(dbghelp);
  dbghelp = nullptr;
#else
  while (installed_signals) {
    --installed_signals;
    (void)sigaction(kSignals[installed_signals], &previous_signals[installed_signals], nullptr);
  }
  if (installed_stack)
    (void)sigaltstack(&previous_stack, nullptr);
  installed_stack = false;
#endif
}

void Rotate(State &state) {
  Close(state.log);
  state.log = kInvalidFile;
  std::error_code error;
  std::filesystem::remove(state.directory / "game.3.log", error);
  for (int i = 2; i >= 0; --i) {
    const auto from = state.directory / (i ? "game." + std::to_string(i) + ".log" : "game.log");
    const auto to = state.directory / ("game." + std::to_string(i + 1) + ".log");
    error.clear();
    if (std::filesystem::exists(from))
      std::filesystem::rename(from, to);
  }
  state.log = Open(state.directory / "game.log");
  if (state.log == kInvalidFile)
    throw std::runtime_error("cannot rotate game.log");
  state.bytes = 0;
}
} // namespace

DiagnosticSession::DiagnosticSession(std::string_view application,
                                     const std::filesystem::path &root) {
  auto &state = Global();
  std::lock_guard lock(state.mutex);
  if (state.owners) {
    ++state.owners;
    return;
  }
  const auto base = std::filesystem::absolute(root.empty() ? Root() : root);
  std::filesystem::create_directories(base);
  const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
#if defined(_WIN32)
  const auto pid = GetCurrentProcessId();
#else
  const auto pid = getpid();
#endif
  const auto prefix = "run-" + std::to_string(now) + "-" + std::to_string(pid);
  for (unsigned attempt = 0;; ++attempt) {
    state.directory = base / (prefix + "-" + std::to_string(attempt));
    if (std::filesystem::create_directory(state.directory))
      break;
    if (attempt == 100)
      throw std::runtime_error("cannot allocate diagnostics directory");
  }
#if !defined(_WIN32)
  (void)chmod(state.directory.c_str(), S_IRWXU);
#endif
  bool installed_terminate{};
  try {
    state.log = Open(state.directory / "game.log");
    crash_file = Open(state.directory / "crash.log");
    if (state.log == kInvalidFile || crash_file == kInvalidFile)
      throw std::runtime_error("cannot open game.log/crash.log");
    const auto metadata = "Nexora diagnostics version=1 application=" + Escaped(application, 256) +
                          " build=" + Escaped(foundation::GetBuildId(), 256) +

#if defined(_WIN32)
                          " platform=Windows"
#elif defined(__APPLE__)
                          " platform=Apple"
#elif defined(__ANDROID__)
                          " platform=Android"
#else
                          " platform=Linux"
#endif
                          " pid=" +
                          std::to_string(pid) + " started_ns=" + std::to_string(now) + "\n";
    if (!Write(state.log, metadata.data(), metadata.size()) ||
        !Write(crash_file, metadata.data(), metadata.size()))
      throw std::runtime_error("cannot write diagnostics metadata");
    state.bytes = metadata.size();
    state.crash_bytes = metadata.size();
#if defined(_WIN32)
    dump_file = Open(state.directory / "crash.dmp");
    dbghelp = LoadLibraryExW(L"dbghelp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (dbghelp) {
      const auto function = GetProcAddress(dbghelp, "MiniDumpWriteDump");
      static_assert(sizeof(function) == sizeof(dump_writer));
      std::memcpy(&dump_writer, &function, sizeof(function));
    }
    const auto dump_status =
        dump_writer && dump_file != kInvalidFile ? "minidump=ready\n" : "minidump=unavailable\n";
    (void)Write(crash_file, dump_status, std::strlen(dump_status));
    previous_filter = SetUnhandledExceptionFilter(OnException);
    installed_filter = true;
    for (std::size_t i = 0; i < kSignals.size(); ++i) {
      previous_signals[i] = std::signal(kSignals[i], OnCrtSignal);
      if (previous_signals[i] == SIG_ERR)
        throw std::runtime_error("cannot install CRT crash handler");
      ++installed_signals;
    }
#else
    stack_t stack{};
    stack.ss_sp = alternate_stack.data();
    stack.ss_size = alternate_stack.size();
    if (sigaltstack(&stack, &previous_stack) != 0)
      throw std::runtime_error("cannot install crash signal stack");
    installed_stack = true;
    struct sigaction action{};
    action.sa_sigaction = OnSignal;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    for (const auto signal : kSignals)
      sigaddset(&action.sa_mask, signal);
    for (std::size_t i = 0; i < kSignals.size(); ++i) {
      if (sigaction(kSignals[i], &action, &previous_signals[i]) != 0)
        throw std::runtime_error("cannot install crash signal handler");
      ++installed_signals;
    }
#endif
    state.previous_terminate = std::set_terminate(OnTerminate);
    installed_terminate = true;
    Sync(crash_file);
    state.owners = 1;
    const auto location = state.directory.u8string();
    std::fprintf(stderr, "Nexora logs: %s\n", reinterpret_cast<const char *>(location.c_str()));
  } catch (...) {
    if (installed_terminate)
      std::set_terminate(state.previous_terminate);
    state.owners = 0;
    Restore();
    Close(state.log);
    state.log = kInvalidFile;
    const File file = crash_file;
    crash_file = kInvalidFile;
    Close(file);
    throw;
  }
}

DiagnosticSession::~DiagnosticSession() {
  auto &state = Global();
  std::lock_guard lock(state.mutex);
  if (--state.owners)
    return;
  constexpr char completed[] = "session=closed\n";
  (void)Write(state.log, completed, sizeof(completed) - 1);
  Sync(state.log);
  std::set_terminate(state.previous_terminate);
  Restore();
  Close(state.log);
  state.log = kInvalidFile;
  state.metadata.clear();
  const File file = crash_file;
  crash_file = kInvalidFile;
  Close(file);
}

std::filesystem::path DiagnosticSession::Directory() {
  auto &state = Global();
  std::lock_guard lock(state.mutex);
  return state.owners ? state.directory : std::filesystem::path{};
}

void WriteDiagnostic(LogLevel level, std::string_view category, std::string_view message) noexcept {
  try {
    static constexpr std::array names{"Trace", "Debug", "Info", "Warning", "Error", "Fatal"};
    if (static_cast<unsigned>(level) >= names.size())
      return;
    const auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    const auto record =
        std::to_string(timestamp) + " " + names[static_cast<unsigned>(level)] +
        " thread=" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) +
        " [" + Escaped(category, 256) + "] " + Escaped(message, 16 * 1024) + "\n";
    auto &state = Global();
    std::lock_guard lock(state.mutex);
    if (!state.owners)
      return;
    if (state.bytes + record.size() > kFileLimit)
      Rotate(state);
    if (!Write(state.log, record.data(), record.size()))
      throw std::runtime_error("game.log write failed");
    state.bytes += record.size();
    if (level >= LogLevel::Error) {
      if (state.crash_bytes + record.size() <= kFileLimit) {
        (void)Write(crash_file, record.data(), record.size());
        state.crash_bytes += record.size();
      }
      Sync(crash_file);
      Sync(state.log);
    }
  } catch (...) {
    constexpr char error[] = "Nexora diagnostics: game log persistence failed\n";
    std::fputs(error, stderr);
    const File file = crash_file;
    if (file != kInvalidFile)
      (void)Write(file, error, sizeof(error) - 1);
  }
}

DiagnosticScope::DiagnosticScope(std::string_view context) noexcept {
  std::memcpy(previous_, current_context, sizeof(previous_));
  const auto count = std::min(context.size(), sizeof(current_context) - 1);
  for (std::size_t i = 0; i < count; ++i)
    current_context[i] = context[i] >= 32 && context[i] != 127 ? context[i] : '?';
  current_context[count] = 0;
}
DiagnosticScope::~DiagnosticScope() { std::memcpy(current_context, previous_, sizeof(previous_)); }

void RefreshDiagnosticModules() noexcept {
  try {
    auto &state = Global();
    std::lock_guard lock(state.mutex);
    if (!state.owners)
      return;
    const auto temporary = state.directory / "modules.tmp";
    std::ofstream output(temporary, std::ios::binary);
#if defined(__linux__)
    std::ifstream maps("/proc/self/maps", std::ios::binary);
    output << maps.rdbuf();
#elif defined(_WIN32)
    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
      MODULEENTRY32W entry{};
      entry.dwSize = sizeof(entry);
      if (Module32FirstW(snapshot, &entry))
        do {
          const auto path = std::filesystem::path(entry.szExePath).u8string();
          output << static_cast<const void *>(entry.modBaseAddr) << " size=" << entry.modBaseSize
                 << " " << reinterpret_cast<const char *>(path.c_str()) << '\n';
        } while (Module32NextW(snapshot, &entry));
      CloseHandle(snapshot);
    }
#elif defined(__APPLE__)
    for (std::uint32_t i = 0; i < _dyld_image_count(); ++i)
      output << _dyld_get_image_header(i) << " slide=" << _dyld_get_image_vmaddr_slide(i) << " "
             << _dyld_get_image_name(i) << '\n';
#else
    output << "module enumeration unavailable\n";
#endif
    output.close();
    if (!output)
      return;
    // Replacement is owner-serialized; a previous snapshot survives failed enumeration/write.
#if defined(_WIN32)
    (void)MoveFileExW(temporary.c_str(), (state.directory / "modules.txt").c_str(),
                      MOVEFILE_REPLACE_EXISTING);
#else
    std::filesystem::rename(temporary, state.directory / "modules.txt");
#endif
  } catch (...) {
    WriteDiagnostic(LogLevel::Warning, "Diagnostics", "module snapshot unavailable");
  }
}

void SetDiagnosticMetadata(std::string_view key, std::string_view value) noexcept {
  try {
    if (key.empty() || key.size() > 64 || value.size() > 1024)
      return;
    auto &state = Global();
    std::lock_guard lock(state.mutex);
    if (!state.owners)
      return;
    const std::string owned_key(key);
    if (state.metadata.size() >= 32 && !state.metadata.contains(owned_key))
      return;
    if (const auto old = state.metadata.find(owned_key);
        old != state.metadata.end() && old->second == value)
      return;
    auto next = state.metadata;
    next[owned_key] = value;
    std::string contents;
    for (const auto &[name, text] : next)
      contents += Escaped(name, 64) + "=" + Escaped(text, 1024) + "\n";
    const auto temporary = state.directory / "context.tmp";
    std::ofstream output(temporary, std::ios::binary);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    if (!output)
      throw std::runtime_error("context write failed");
#if defined(_WIN32)
    if (!MoveFileExW(temporary.c_str(), (state.directory / "context.txt").c_str(),
                     MOVEFILE_REPLACE_EXISTING))
      throw std::runtime_error("context replacement failed");
#else
    std::filesystem::rename(temporary, state.directory / "context.txt");
#endif
    state.metadata.swap(next);
  } catch (...) {
    WriteDiagnostic(LogLevel::Warning, "Diagnostics", "context persistence failed");
  }
}
void SetDiagnosticMetadata(std::string_view key, std::uint64_t value) noexcept {
  try {
    SetDiagnosticMetadata(key, std::to_string(value));
  } catch (...) {
  }
}

void WriteGameplayDiagnostic(std::uint32_t level, const char *message,
                             std::uint32_t length) noexcept {
  if (!message || length > 16 * 1024 || level > static_cast<std::uint32_t>(LogLevel::Fatal)) {
    WriteDiagnostic(LogLevel::Warning, "Gameplay", "invalid gameplay log rejected");
    return;
  }
  WriteDiagnostic(static_cast<LogLevel>(level), "Gameplay", {message, length});
}
} // namespace nexora::core
