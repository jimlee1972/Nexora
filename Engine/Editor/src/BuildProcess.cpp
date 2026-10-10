#if !defined(_GNU_SOURCE) && defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#include "Nexora/Editor/BuildProcess.h"
#include "Nexora/Foundation/Types.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <Availability.h>
#include <TargetConditionals.h>
#endif
#if !defined(_WIN32) && !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IPHONE)
#define NEXORA_BUILD_PROCESS_POSIX 1
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <crt_externs.h>
#else
extern char **environ;
#endif
#endif

namespace nexora::editor {
namespace {
std::string Utf8(const std::filesystem::path &path) {
  const auto text = path.generic_u8string();
  return {text.begin(), text.end()};
}
bool Text(std::string_view value) {
  return value.find('\0') == std::string_view::npos && foundation::IsValidUtf8(value);
}
} // namespace
struct BuildProcess::Implementation final {
  struct Operation final {
    mutable std::mutex mutex;
    BuildProcessSnapshot snapshot;
    BuildProcessRequest request;
    core::CancellationSource cancellation;
    core::JobHandle job_handle;
    bool consumed{};
    void Set(BuildProcessPhase phase, std::string message) {
      std::lock_guard lock{mutex};
      snapshot.phase = phase;
      snapshot.message = std::move(message);
    }
    void Append(const char *data, std::size_t count) {
      std::lock_guard lock{mutex};
      const auto dropped = snapshot.output.size() + count > request.output_capacity
                               ? snapshot.output.size() + count - request.output_capacity
                               : 0;
      snapshot.dropped_output_bytes += dropped;
      if (count >= request.output_capacity) {
        snapshot.output.assign(data + count - request.output_capacity, request.output_capacity);
      } else {
        if (dropped)
          snapshot.output.erase(0, dropped);
        snapshot.output.append(data, count);
      }
    }
    void Exit(std::uint32_t code) {
      std::lock_guard lock{mutex};
      snapshot.exit_code = code;
      snapshot.phase = BuildProcessPhase::AwaitingOwner;
      snapshot.message = "Process exited; owner scope and artifacts require verification.";
    }
    void Run(const core::CancellationToken &);
  };
  core::JobSystem &jobs;
  std::shared_ptr<Operation> operation;
  std::uint64_t next{1};
  bool stopped{};
  explicit Implementation(core::JobSystem &value) : jobs(value) {}
};
BuildProcess::BuildProcess(core::JobSystem &jobs)
    : implementation_(std::make_unique<Implementation>(jobs)) {}
BuildProcess::~BuildProcess() { Shutdown(); }
bool BuildProcess::Start(BuildProcessRequest request, std::string *error) {
  auto &impl = *implementation_;
  const auto reject = [&](const char *message) {
    if (error)
      *error = message;
    return false;
  };
  if (impl.stopped || Busy() || impl.next == std::numeric_limits<std::uint64_t>::max())
    return reject("Build process is busy, stopped or exhausted.");
  if (request.executable.native().size() > kMaximumCommandBytes ||
      request.working_directory.native().size() > kMaximumCommandBytes)
    return reject("Build process paths exceed the command budget.");
  std::string executable, directory;
  try {
    executable = Utf8(request.executable);
    directory = Utf8(request.working_directory);
  } catch (const std::exception &) {
    return reject("Build process paths cannot be represented as UTF-8.");
  }
  if (!request.scope || !request.executable.is_absolute() ||
      !request.working_directory.is_absolute() || executable.empty() || directory.empty() ||
      !Text(executable) || !Text(directory) || request.arguments.size() > kMaximumArguments ||
      !request.output_capacity || request.output_capacity > kMaximumOutputBytes ||
      executable.size() > kMaximumCommandBytes || directory.size() > kMaximumCommandBytes)
    return reject(
        "Build process requires a valid scope, absolute paths and bounded arguments/output.");
  std::size_t bytes = executable.size();
  for (const auto &argument : request.arguments) {
    if (!Text(argument) || argument.size() + 1 > kMaximumCommandBytes - bytes)
      return reject("Build process arguments are invalid or exceed 32 KiB.");
    bytes += argument.size() + 1;
  }
  auto op = std::make_shared<Implementation::Operation>();
  op->snapshot.operation = impl.next++;
  op->snapshot.scope = request.scope;
  op->snapshot.phase = BuildProcessPhase::Queued;
  op->snapshot.message = "Build process queued.";
  op->request = std::move(request);
  try {
    op->job_handle = impl.jobs.Submit(
        {[op](const core::CancellationToken &token) {
           try {
             op->Run(token);
           } catch (...) {
             op->Set(BuildProcessPhase::Failed, "Build process worker failed.");
           }
         },
         core::JobPriority::Low, op->cancellation.Token(), "Bounded build process"});
  } catch (...) {
    return reject("Build process could not submit its worker.");
  }
  impl.operation = std::move(op);
  if (error)
    error->clear();
  return true;
}
bool BuildProcess::Poll(std::uint64_t current_scope) {
  auto &impl = *implementation_;
  const auto op = impl.operation;
  if (!op || op->consumed)
    return false;
  const auto status = op->job_handle.Status();
  if (status == core::JobStatus::Queued || status == core::JobStatus::Running)
    return false;
  try {
    impl.jobs.Wait(op->job_handle);
  } catch (...) {
    op->Set(BuildProcessPhase::Failed, "Build process worker failed.");
  }
  if (op->cancellation.Token().IsCancellationRequested() || status == core::JobStatus::Cancelled)
    op->Set(BuildProcessPhase::Cancelled,
            "Build process cancelled; no build success was published.");
  else if (Snapshot().phase == BuildProcessPhase::AwaitingOwner) {
    if (current_scope != op->snapshot.scope)
      op->Set(BuildProcessPhase::Stale,
              "Build process scope changed; no build success was published.");
    else if (op->snapshot.exit_code != 0u)
      op->Set(BuildProcessPhase::Failed, "Build process returned a nonzero exit code.");
    else
      op->Set(BuildProcessPhase::Exited,
              "Process exited with code zero; artifacts are not verified.");
  }
  op->request = {};
  op->consumed = true;
  return true;
}
bool BuildProcess::Cancel() noexcept {
  const auto op = implementation_->operation;
  if (!op || op->consumed)
    return false;
  op->cancellation.Cancel();
  return true;
}
bool BuildProcess::Busy() const {
  const auto op = implementation_->operation;
  return op && !op->consumed;
}
BuildProcessSnapshot BuildProcess::Snapshot() const {
  const auto op = implementation_->operation;
  if (!op)
    return {};
  std::lock_guard lock{op->mutex};
  return op->snapshot;
}
void BuildProcess::Shutdown() noexcept {
  auto &impl = *implementation_;
  impl.stopped = true;
  const auto op = impl.operation;
  if (!op || op->consumed)
    return;
  op->cancellation.Cancel();
  try {
    impl.jobs.Wait(op->job_handle);
  } catch (...) {
  }
  op->request = {};
  op->consumed = true;
  try {
    op->Set(BuildProcessPhase::Cancelled, "Build process stopped and drained.");
  } catch (...) {
  }
}
#if defined(NEXORA_BUILD_PROCESS_POSIX)
namespace {
struct Descriptor final {
  int value{-1};
  ~Descriptor() {
    if (value >= 0)
      close(value);
  }
};
struct SpawnActions final {
  posix_spawn_file_actions_t value;
  SpawnActions() {
    if (posix_spawn_file_actions_init(&value))
      throw std::runtime_error("Could not initialize spawn actions.");
  }
  ~SpawnActions() { posix_spawn_file_actions_destroy(&value); }
};
struct SpawnAttributes final {
  posix_spawnattr_t value;
  SpawnAttributes() {
    if (posix_spawnattr_init(&value))
      throw std::runtime_error("Could not initialize spawn attributes.");
  }
  ~SpawnAttributes() { posix_spawnattr_destroy(&value); }
};
struct Child final {
  pid_t id{};
  ~Child() {
    if (!id)
      return;
    // The direct child remains unreaped until this cleanup, reserving its process-group identity.
    kill(-id, SIGKILL);
    int status{};
    while (waitpid(id, &status, 0) < 0 && errno == EINTR) {
    }
  }
};
void SpawnCheck(int result) {
  if (result)
    throw std::runtime_error("Build process launch setup failed.");
}
} // namespace
void BuildProcess::Implementation::Operation::Run(const core::CancellationToken &token) {
  if (token.IsCancellationRequested()) {
    Set(BuildProcessPhase::Cancelled, "Build process cancelled before launch.");
    return;
  }
  struct sigaction child_signal{};
  if (sigaction(SIGCHLD, nullptr, &child_signal) || child_signal.sa_handler == SIG_IGN ||
      (child_signal.sa_flags & SA_NOCLDWAIT)) {
    Set(BuildProcessPhase::Failed, "Build process requires owned child wait/reap policy.");
    return;
  }
  int descriptors[2];
  if (pipe(descriptors)) {
    Set(BuildProcessPhase::Failed, "Could not create build output pipe.");
    return;
  }
  Descriptor read_end{descriptors[0]}, write_end{descriptors[1]};
  for (auto *descriptor : {&read_end, &write_end}) {
    if (descriptor->value <= STDERR_FILENO) {
      const auto replacement = fcntl(descriptor->value, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
      if (replacement < 0)
        throw std::runtime_error("Could not reserve build pipe descriptors.");
      close(descriptor->value);
      descriptor->value = replacement;
    }
    if (fcntl(descriptor->value, F_SETFD, FD_CLOEXEC) < 0)
      throw std::runtime_error("Could not protect build pipe descriptors.");
  }
  if (fcntl(read_end.value, F_SETFL, O_NONBLOCK) < 0)
    throw std::runtime_error("Could not configure build output polling.");
  SpawnActions actions;
  SpawnCheck(posix_spawn_file_actions_addclose(&actions.value, read_end.value));
  SpawnCheck(
      posix_spawn_file_actions_addopen(&actions.value, STDIN_FILENO, "/dev/null", O_RDONLY, 0));
  SpawnCheck(posix_spawn_file_actions_adddup2(&actions.value, write_end.value, STDOUT_FILENO));
  SpawnCheck(posix_spawn_file_actions_adddup2(&actions.value, write_end.value, STDERR_FILENO));
  SpawnCheck(posix_spawn_file_actions_addclose(&actions.value, write_end.value));
#if defined(__APPLE__) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
  if (__builtin_available(macOS 26.0, *))
    SpawnCheck(
        posix_spawn_file_actions_addchdir(&actions.value, request.working_directory.c_str()));
  else {
    // Retain the older deployment target with only this deprecated compatibility call suppressed.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    SpawnCheck(
        posix_spawn_file_actions_addchdir_np(&actions.value, request.working_directory.c_str()));
#pragma clang diagnostic pop
  }
#else
  SpawnCheck(
      posix_spawn_file_actions_addchdir_np(&actions.value, request.working_directory.c_str()));
#endif
  SpawnAttributes attributes;
  sigset_t empty, defaults;
  sigemptyset(&empty);
  sigemptyset(&defaults);
  sigaddset(&defaults, SIGTERM);
  sigaddset(&defaults, SIGINT);
  sigaddset(&defaults, SIGPIPE);
  SpawnCheck(posix_spawnattr_setsigmask(&attributes.value, &empty));
  SpawnCheck(posix_spawnattr_setsigdefault(&attributes.value, &defaults));
  SpawnCheck(posix_spawnattr_setpgroup(&attributes.value, 0));
  SpawnCheck(posix_spawnattr_setflags(
      &attributes.value, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF));
  const auto executable = Utf8(request.executable);
  std::vector<char *> arguments;
  arguments.reserve(request.arguments.size() + 2);
  arguments.push_back(const_cast<char *>(executable.c_str()));
  for (auto &argument : request.arguments)
    arguments.push_back(argument.data());
  arguments.push_back(nullptr);
#if defined(__APPLE__)
  char **environment = *_NSGetEnviron();
#else
  char **environment = environ;
#endif
  Child child;
  if (posix_spawn(&child.id, executable.c_str(), &actions.value, &attributes.value,
                  arguments.data(), environment)) {
    child.id = 0;
    Set(BuildProcessPhase::Failed, "Could not launch build executable or working directory.");
    return;
  }
  close(write_end.value);
  write_end.value = -1;
  Set(BuildProcessPhase::Running, "Build process running.");
  std::optional<std::chrono::steady_clock::time_point> terminating;
  siginfo_t exit{};
  bool pipe_closed{};
  const auto read_output = [&] {
    std::array<char, 4096> buffer;
    // Continuous producers cannot starve cancellation/wait polling.
    for (int i = 0; i < 16; ++i) {
      const auto count = read(read_end.value, buffer.data(), buffer.size());
      if (count > 0) {
        Append(buffer.data(), static_cast<std::size_t>(count));
        continue;
      }
      if (!count)
        pipe_closed = true;
      else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        throw std::runtime_error("Build output read failed.");
      break;
    }
  };
  for (;;) {
    read_output();
    if (waitid(P_PID, static_cast<id_t>(child.id), &exit, WEXITED | WNOHANG | WNOWAIT)) {
      if (errno == EINTR)
        continue;
      throw std::runtime_error("Build child wait failed.");
    }
    if (exit.si_pid)
      break;
    if (token.IsCancellationRequested()) {
      if (!terminating) {
        kill(-child.id, SIGTERM);
        terminating = std::chrono::steady_clock::now();
      } else if (std::chrono::steady_clock::now() - *terminating >= std::chrono::milliseconds(250))
        kill(-child.id, SIGKILL);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  // Stop any descendants even if the tool exited without waiting for them. Identity is reserved.
  kill(-child.id, SIGKILL);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
  while (!pipe_closed && std::chrono::steady_clock::now() < deadline) {
    read_output();
    if (!pipe_closed)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (token.IsCancellationRequested())
    Set(BuildProcessPhase::Cancelled, "Build process group cancelled and drained.");
  else if (!pipe_closed)
    Set(BuildProcessPhase::Failed, "Build process output did not drain completely.");
  else if (exit.si_code != CLD_EXITED)
    Set(BuildProcessPhase::Failed, "Build process terminated by a signal.");
  else
    Exit(exit.si_status);
}
#elif defined(_WIN32)
namespace {
struct Handle final {
  HANDLE value{};
  ~Handle() {
    if (value && value != INVALID_HANDLE_VALUE)
      CloseHandle(value);
  }
};
struct ChildHandles final {
  Handle &process, &job;
  ~ChildHandles() {
    if (job.value)
      TerminateJobObject(job.value, 137);
    if (process.value) {
      TerminateProcess(process.value, 137);
      WaitForSingleObject(process.value, INFINITE);
    }
  }
};
struct AttributeList final {
  std::vector<std::byte> storage;
  LPPROC_THREAD_ATTRIBUTE_LIST value{};
  AttributeList() {
    SIZE_T size{};
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    if (!size)
      throw std::runtime_error("Could not size process handle attributes.");
    storage.resize(size);
    value = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(value, 1, 0, &size)) {
      value = nullptr;
      throw std::runtime_error("Could not initialize process handle attributes.");
    }
  }
  ~AttributeList() {
    if (value)
      DeleteProcThreadAttributeList(value);
  }
};
std::wstring Wide(std::string_view value) {
  if (value.empty())
    return {};
  const auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                        static_cast<int>(value.size()), nullptr, 0);
  if (!size)
    throw std::runtime_error("Could not decode process argument UTF-8.");
  std::wstring result(static_cast<std::size_t>(size), L'\0');
  if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                           static_cast<int>(value.size()), result.data(), size))
    throw std::runtime_error("Could not decode process argument UTF-8.");
  return result;
}
void Quote(std::wstring &command, std::wstring_view value) {
  if (!command.empty())
    command += L' ';
  command += L'"';
  std::size_t slashes{};
  for (const auto character : value) {
    if (character == L'\\') {
      ++slashes;
      continue;
    }
    command.append(character == L'"' ? slashes * 2 + 1 : slashes, L'\\');
    command += character;
    slashes = 0;
  }
  command.append(slashes * 2, L'\\');
  command += L'"';
}
} // namespace
void BuildProcess::Implementation::Operation::Run(const core::CancellationToken &token) {
  if (token.IsCancellationRequested()) {
    Set(BuildProcessPhase::Cancelled, "Build process cancelled before launch.");
    return;
  }
  std::wstring command;
  Quote(command, request.executable.native());
  for (const auto &argument : request.arguments)
    Quote(command, Wide(argument));
  if (command.size() >= 32767) {
    Set(BuildProcessPhase::Failed, "Quoted build command exceeds the Windows limit.");
    return;
  }
  SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  Handle read_end, write_end, input, job, process, thread;
  if (!CreatePipe(&read_end.value, &write_end.value, &inheritable, 0) ||
      !SetHandleInformation(read_end.value, HANDLE_FLAG_INHERIT, 0))
    throw std::runtime_error("Could not create private build output pipe.");
  input.value = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable,
                            OPEN_EXISTING, 0, nullptr);
  if (input.value == INVALID_HANDLE_VALUE)
    throw std::runtime_error("Could not configure build standard input.");
  job.value = CreateJobObjectW(nullptr, nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!job.value || !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits,
                                             sizeof(limits)))
    throw std::runtime_error("Could not create managed build process job.");
  AttributeList attributes;
  HANDLE handles[]{write_end.value, input.value};
  if (!UpdateProcThreadAttribute(attributes.value, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
                                 sizeof(handles), nullptr, nullptr))
    throw std::runtime_error("Could not restrict inherited build handles.");
  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = input.value;
  startup.StartupInfo.hStdOutput = write_end.value;
  startup.StartupInfo.hStdError = write_end.value;
  startup.lpAttributeList = attributes.value;
  PROCESS_INFORMATION launched{};
  ChildHandles cleanup{process, job};
  if (!CreateProcessW(request.executable.c_str(), command.data(), nullptr, nullptr, TRUE,
                      CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr,
                      request.working_directory.c_str(), &startup.StartupInfo, &launched)) {
    Set(BuildProcessPhase::Failed, "Could not launch build executable or working directory.");
    return;
  }
  process.value = launched.hProcess;
  thread.value = launched.hThread;
  if (!AssignProcessToJobObject(job.value, process.value) ||
      ResumeThread(thread.value) == DWORD(-1))
    throw std::runtime_error("Could not contain and resume build process.");
  CloseHandle(write_end.value);
  write_end.value = nullptr;
  Set(BuildProcessPhase::Running, "Build process running.");
  bool pipe_closed{};
  const auto read_output = [&] {
    std::array<char, 4096> buffer;
    for (int i = 0; i < 16; ++i) {
      DWORD available{}, count{};
      if (!PeekNamedPipe(read_end.value, nullptr, 0, nullptr, &available, nullptr)) {
        if (GetLastError() == ERROR_BROKEN_PIPE) {
          pipe_closed = true;
          break;
        }
        throw std::runtime_error("Build output inspection failed.");
      }
      if (!available)
        break;
      if (!ReadFile(read_end.value, buffer.data(),
                    std::min<DWORD>(available, static_cast<DWORD>(buffer.size())), &count,
                    nullptr)) {
        if (GetLastError() == ERROR_BROKEN_PIPE) {
          pipe_closed = true;
          break;
        }
        throw std::runtime_error("Build output read failed.");
      }
      if (count)
        Append(buffer.data(), count);
    }
  };
  DWORD result{};
  for (;;) {
    read_output();
    const auto wait = WaitForSingleObject(process.value, 0);
    if (wait == WAIT_OBJECT_0)
      break;
    if (wait != WAIT_TIMEOUT)
      throw std::runtime_error("Build process wait failed.");
    if (token.IsCancellationRequested())
      TerminateJobObject(job.value, 137);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (!GetExitCodeProcess(process.value, &result))
    throw std::runtime_error("Build process exit status unavailable.");
  TerminateJobObject(job.value, 137);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
  while (!pipe_closed && std::chrono::steady_clock::now() < deadline) {
    read_output();
    if (!pipe_closed)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (token.IsCancellationRequested())
    Set(BuildProcessPhase::Cancelled, "Build process job cancelled and drained.");
  else if (!pipe_closed)
    Set(BuildProcessPhase::Failed, "Build process output did not drain completely.");
  else
    Exit(result);
}
#else
void BuildProcess::Implementation::Operation::Run(const core::CancellationToken &) {
  Set(BuildProcessPhase::Unsupported, "This platform has no default build process runner.");
}
#endif
} // namespace nexora::editor
