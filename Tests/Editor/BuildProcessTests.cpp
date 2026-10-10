#include "Nexora/Editor/BuildProcess.h"

#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#endif

namespace {
using namespace nexora;
using Phase = editor::BuildProcessPhase;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Finish(editor::BuildProcess &process, std::uint64_t scope) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!process.Poll(scope)) {
    Require(std::chrono::steady_clock::now() < deadline, "Process owner did not complete");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
void Output(editor::BuildProcess &process, const std::string &text) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (process.Snapshot().output.find(text) == std::string::npos) {
    Require(std::chrono::steady_clock::now() < deadline, "Real process output was not observed");
    Require(process.Snapshot().phase != Phase::Failed, "Actual child launch failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
void Run(const std::filesystem::path &fixture) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-build-process-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto directory = root / std::filesystem::path(u8"程序 µ");
  std::filesystem::create_directories(directory);
  const auto executable = directory / fixture.filename();
  std::filesystem::copy_file(fixture, executable);
  core::JobSystem jobs(1);
  jobs.Start();
  editor::BuildProcess process(jobs);
  editor::BuildProcessRequest request{42,
                                      executable,
                                      directory,
                                      {"--args", "", "Unicode 空 白 µ", "quotes \" and trailing\\",
                                       "$(touch SHELL_WAS_RUN)", "%VARIABLE% & > < | ;"},
                                      4096};
  std::string error;
  Require(process.Start(request, &error) && error.empty() && !process.Start(request),
          "Intake/busy contract failed");
  Finish(process, 42);
  auto snapshot = process.Snapshot();
  Require(snapshot.phase == Phase::Exited && snapshot.exit_code == 0u &&
              !snapshot.dropped_output_bytes,
          "Actual successful exit was not represented correctly");
  for (std::size_t i = 1; i < request.arguments.size(); ++i)
    Require(snapshot.output.find(std::to_string(request.arguments[i].size()) + ':' +
                                 request.arguments[i] + '\n') != std::string::npos,
            "Unicode/empty/metacharacter argument was not reproduced exactly");
  const auto cwd = std::filesystem::canonical(directory).generic_u8string();
  Require(snapshot.output.find("CWD=" + std::string(cwd.begin(), cwd.end())) != std::string::npos &&
              snapshot.output.find("STDERR_END") != std::string::npos &&
              !std::filesystem::exists(directory / "SHELL_WAS_RUN"),
          "Working directory/merged stderr/no-shell contract failed");
  request.arguments = {"--fail"};
  Require(process.Start(request), "Nonzero fixture did not launch");
  Finish(process, 42);
  Require(process.Snapshot().phase == Phase::Failed && process.Snapshot().exit_code == 7u &&
              process.Snapshot().output.find("ACTUAL_NONZERO_EXIT") != std::string::npos,
          "Nonzero process was reported as success");
  request.arguments = {"--native-failure"};
  Require(process.Start(request), "Native failure fixture did not launch");
  Finish(process, 42);
  Require(process.Snapshot().phase == Phase::Failed, "Native failure showed success");
#if defined(_WIN32)
  Require(process.Snapshot().exit_code == 0xc0000005u, "Windows exit status was truncated");
#else
  Require(!process.Snapshot().exit_code, "Signalled process fabricated a normal exit code");
#endif
  request.arguments = {"--flood"};
  Require(process.Start(request), "Flood fixture did not launch");
  Finish(process, 42);
  snapshot = process.Snapshot();
  Require(snapshot.phase == Phase::Exited && snapshot.output.size() == 4096 &&
              snapshot.dropped_output_bytes ==
                  512 * 4096 + std::string("FLOOD_END\n").size() - 4096 &&
              snapshot.output.ends_with("FLOOD_END\n"),
          "Flooded output was unbounded or miscounted");
  request.arguments = {"--args"};
  Require(process.Start(request), "Stale fixture did not launch");
  Finish(process, 43);
  Require(process.Snapshot().phase == Phase::Stale && process.Snapshot().exit_code == 0u,
          "Old scope accepted a successful exit");
  auto missing = request;
  missing.executable = directory / "missing-executable";
  Require(process.Start(missing), "Missing executable should report an actual launch failure");
  Finish(process, 42);
  Require(process.Snapshot().phase == Phase::Failed && !process.Snapshot().exit_code,
          "Launch failure fabricated an exit status");
  request.arguments = {"--sleep"};
  Require(process.Start(request), "Cancellation fixture did not launch");
  Output(process, "READY");
  const auto cancelled_at = std::chrono::steady_clock::now();
  Require(process.Cancel() && !process.Start(request),
          "Running cancellation allowed premature replacement");
  Finish(process, 42);
  Require(process.Snapshot().phase == Phase::Cancelled &&
              std::chrono::steady_clock::now() - cancelled_at < std::chrono::seconds(3),
          "Cancellation waited for the uncooperative sleeper or showed success");
  // Cancel before execution while the actual single worker is occupied.
  std::promise<void> entered, release;
  const auto released = release.get_future().share();
  const auto blocker = jobs.Submit({[&](const core::CancellationToken &) {
                                      entered.set_value();
                                      released.wait();
                                    },
                                    core::JobPriority::Normal,
                                    {},
                                    "build cancellation blocker"});
  entered.get_future().wait();
  const bool queued = process.Start(request) && process.Cancel();
  release.set_value();
  jobs.Wait(blocker);
  Require(queued, "Queued cancellation fixture failed");
  Finish(process, 42);
  Require(process.Snapshot().phase == Phase::Cancelled && process.Snapshot().output.empty(),
          "Cancelled queued process executed");
  request.arguments = {"--tree"};
  Require(process.Start(request), "Descendant fixture did not launch");
  Output(process, "CHILD_PID=");
  Output(process, "READY");
  const auto output = process.Snapshot().output;
  const auto child =
      static_cast<unsigned long>(std::stoul(output.substr(output.find("CHILD_PID=") + 10)));
  Require(process.Cancel(), "Tree cancellation failed");
  Finish(process, 42);
  Require(process.Snapshot().phase == Phase::Cancelled, "Descendant tree showed success");
#if defined(_WIN32)
  const auto handle = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                  static_cast<DWORD>(child));
  if (handle) {
    const auto stopped = WaitForSingleObject(handle, 2000) == WAIT_OBJECT_0;
    CloseHandle(handle);
    Require(stopped, "Managed Windows descendant remained alive");
  } else
    Require(GetLastError() == ERROR_INVALID_PARAMETER,
            "Could not verify Windows descendant termination");
#else
  Require(kill(static_cast<pid_t>(child), 0) < 0 && errno == ESRCH,
          "Managed POSIX descendant remained alive or unreaped");
#endif
  auto invalid = request;
  invalid.arguments.emplace_back("nul\0argument", 12);
  const auto retained = process.Snapshot().operation;
  Require(!process.Start(invalid) && process.Snapshot().operation == retained,
          "Invalid arguments altered retained observation");
  invalid = request;
  invalid.arguments.assign(editor::BuildProcess::kMaximumArguments + 1, "x");
  Require(!process.Start(invalid), "Argument-count overflow admitted");
  invalid = request;
  invalid.output_capacity = editor::BuildProcess::kMaximumOutputBytes + 1;
  Require(!process.Start(invalid), "Output-capacity overflow admitted");
  invalid = request;
  invalid.arguments = {std::string(editor::BuildProcess::kMaximumCommandBytes, 'x')};
  Require(!process.Start(invalid), "Command-byte overflow admitted");
#if defined(_WIN32)
  invalid = request;
  invalid.executable = std::filesystem::path(std::wstring(L"C:\\") + wchar_t(0xd800));
  Require(!process.Start(invalid), "Invalid native Unicode path admitted");
#endif
  request.arguments = {"--sleep"};
  Require(process.Start(request), "Shutdown fixture did not launch");
  Output(process, "READY");
  process.Shutdown();
  Require(!process.Busy() && process.Snapshot().phase == Phase::Cancelled &&
              !process.Start(request),
          "Shutdown retained active work or accepted new intake");
  jobs.Stop();
  std::filesystem::remove_all(root);
}
} // namespace
int main(int argc, char **argv) {
  try {
    Require(argc == 2, "Actual process fixture path is required");
    Run(argv[1]);
    std::cout << "Bounded structured build processes passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
