#include "Nexora/Core/Diagnostics.h"
#include "Nexora/Core/Engine.h"
#include "Nexora/Runtime/GameplayModuleHost.h"
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif
#if !defined(_WIN32)
#include <sys/resource.h>
#endif

namespace {
#if defined(__linux__)
// Replace only one already-open sink; keep the other descriptor healthy.
void FailSink(const std::filesystem::path &sink) {
  const int full = ::open("/dev/full", O_WRONLY | O_CLOEXEC);
  if (full < 0)
    throw std::runtime_error("cannot open fault-injection device");
  struct Close final {
    int fd;
    ~Close() { ::close(fd); }
  } close{full};
  for (const auto &entry : std::filesystem::directory_iterator("/proc/self/fd")) {
    std::error_code error;
    const auto target = std::filesystem::read_symlink(entry.path(), error);
    if (!error && target == sink) {
      const auto fd = std::stoi(entry.path().filename().string());
      if (::dup2(full, fd) < 0)
        throw std::runtime_error("cannot inject sink failure");
      return;
    }
  }
  throw std::runtime_error("diagnostic sink descriptor not found");
}
#endif
void ExhaustStack(unsigned depth);
void (*recurse)(unsigned) = ExhaustStack;
void ExhaustStack(unsigned depth) {
  volatile unsigned char buffer[4096];
  buffer[depth % sizeof(buffer)] = static_cast<unsigned char>(depth);
  recurse(depth + 1);
  if (buffer[depth % sizeof(buffer)] == 255)
    std::abort();
}
} // namespace

int main(int argc, char **argv) {
#if !defined(_WIN32)
  // Exercise fatal termination without producing large core files in CI.
  const rlimit disabled{0, 0};
  (void)setrlimit(RLIMIT_CORE, &disabled);
#endif
  if (argc != 3)
    return 2;
  const std::string mode = argv[1];
  return nexora::core::RunDiagnosed("CrashDiagnosticsFixture", [&]() -> int {
    using namespace nexora;
    using namespace std::string_view_literals;
    core::DiagnosticScope scope("Fixture.main");
    core::SetDiagnosticMetadata("game.project", "diagnostic fixture");
    core::SetDiagnosticMetadata("game.primary_scene", std::uint64_t{41});
    core::SetDiagnosticMetadata("game.primary_scene", std::uint64_t{42});
    const auto directory = core::DiagnosticSession::Directory();
    core::Engine engine;
    engine.Initialize();
    if (directory != core::DiagnosticSession::Directory())
      throw std::runtime_error("nested session changed directory");
    engine.Services().log->Write(core::LogLevel::Info, "Game", "accepted immediately before crash");
#if defined(__linux__)
    if (mode == "game-write-failure" || mode == "crash-write-failure") {
      FailSink(directory / (mode == "game-write-failure" ? "game.log" : "crash.log"));
      core::WriteDiagnostic(core::LogLevel::Error, "Gameplay",
                            "retained error despite sink failure");
      core::WriteDiagnostic(core::LogLevel::Fatal, "Gameplay",
                            "retained fatal despite sink failure");
      return 0;
    }
#endif
    if (mode == "rotation-failure") {
      while (std::filesystem::file_size(directory / "game.log") < 4 * 1024 * 1024 - 512)
        core::WriteDiagnostic(core::LogLevel::Info, "Fill", std::string(256, 'a'));
      std::filesystem::create_directories(directory / "game.3.log" / "obstruction");
      std::ofstream(directory / "game.2.log") << "cannot replace a nonempty directory";
      core::WriteDiagnostic(core::LogLevel::Fatal, "Gameplay",
                            "retained fatal despite rotation failure " + std::string(4096, 'x'));
      return 0;
    }
    if (mode == "exception")
      throw std::runtime_error("game exception details");
    if (mode == "unknown-exception")
      throw 7;
    if (mode == "terminate")
      std::terminate();
    if (mode == "abort")
      std::abort();
    if (mode == "stack-overflow") {
      core::DiagnosticScope stack_scope("Gameplay.stack");
      ExhaustStack(0);
      return 2;
    }
    if (mode == "worker-crash") {
      std::thread worker([] {
        core::DiagnosticScope worker_scope("Gameplay.worker-遊戲");
        std::abort();
      });
      worker.join();
    }
    if (mode == "rotation") {
      for (int i = 0; i < 1200; ++i)
        core::WriteDiagnostic(core::LogLevel::Info, "Rotation", std::string(4096, 'a'));
    }
    if (mode == "job-exception") {
      auto job = engine.Services().jobs->Submit(
          {[](const core::CancellationToken &) { throw std::runtime_error("game job exception"); },
           core::JobPriority::Normal,
           {},
           "game job"});
      try {
        engine.Services().jobs->Wait(job);
      } catch (const std::runtime_error &) {
        return 0;
      }
      return 2;
    }
    if (mode == "job-crash") {
      auto job =
          engine.Services().jobs->Submit({[](const core::CancellationToken &) { std::abort(); },
                                          core::JobPriority::Normal,
                                          {},
                                          "game job"});
      engine.Services().jobs->Wait(job);
      return 2;
    }
    if (mode == "normal") {
      std::vector<std::thread> writers;
      for (int i = 0; i < 4; ++i)
        writers.emplace_back([i] {
          for (int j = 0; j < 16; ++j)
            core::WriteDiagnostic(core::LogLevel::Info, "Concurrent",
                                  "writer=" + std::to_string(i) + " record=" + std::to_string(j));
        });
      for (auto &writer : writers)
        writer.join();
      core::WriteDiagnostic(core::LogLevel::Info, "Escape", "first\nforged\0tail"sv);
      core::WriteGameplayDiagnostic(2, "invalid", UINT32_MAX);
      core::WriteDiagnostic(core::LogLevel::Error, "Game", "recoverable game failure");
      return 0;
    }
    if (mode == "rotation")
      return 0;
    NexoraGameplayHostV3 host{};
    host.struct_size = sizeof(host);
    host.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
    host.context = const_cast<char *>(mode.c_str());
    host.log = [](void *, std::uint32_t level, const char *text, std::uint32_t size) {
      core::WriteGameplayDiagnostic(level, text, size);
    };
    runtime::GameplayModuleHost module(host);
    if (!module.Load(std::filesystem::path(reinterpret_cast<const char8_t *>(argv[2]))))
      throw std::runtime_error("module load failed");
    if (mode == "stop-crash" || mode == "destroy-crash") {
      module.Unload();
      return 2;
    }
    if (mode == "fixed-crash" || mode == "fixed-error") {
      if (module.FixedUpdate(1.0 / 60.0))
        return 2;
    } else if (module.Update(1.0 / 60.0))
      return 2;
    return 0;
  });
}
