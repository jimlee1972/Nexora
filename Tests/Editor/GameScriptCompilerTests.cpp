#include "GameScriptCompiler.h"
#include "LiveGameScript.h"
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using Compiler = editor::preview::GameScriptCompiler;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Work final {
  std::mutex mutex;
  std::condition_variable changed;
  std::vector<std::string> calls;
  std::set<std::string> started, released, cancelled;
  unsigned active{}, peak{};
  void Wait(std::string name) {
    std::unique_lock lock(mutex);
    Require(changed.wait_for(lock, std::chrono::seconds(5), [&] { return started.contains(name); }),
            "worker did not start");
  }
  void Release(std::string name) {
    std::scoped_lock lock(mutex);
    released.insert(std::move(name));
    changed.notify_all();
  }
  std::optional<editor::PreparedGameScript> Produce(editor::GameScriptBuildRequest request,
                                                    const core::CancellationToken &token,
                                                    std::string *error) {
    const auto name = request.script_manifest.string();
    if (request.progress) {
      editor::GameBuildProgress progress;
      progress.stage = editor::GameBuildStage::CompileAndTest;
      progress.output = name;
      request.progress(std::move(progress));
    }
    std::unique_lock lock(mutex);
    ++active;
    peak = std::max(peak, active);
    calls.push_back(name);
    started.insert(name);
    changed.notify_all();
    if (name == "A" || name == "D" || name == "F")
      changed.wait(lock, [&] { return released.contains(name); });
    if (name == "Stop")
      while (!token.IsCancellationRequested())
        changed.wait_for(lock, std::chrono::milliseconds(1));
    if (token.IsCancellationRequested())
      cancelled.insert(name);
    --active;
    *error = name;
    return {};
  }
};
editor::GameScriptBuildRequest Request(std::string name) {
  editor::GameScriptBuildRequest value;
  value.script_manifest = std::move(name);
  return value;
}
Compiler::Result Await(Compiler &compiler, Compiler::Key key) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    if (auto value = compiler.Poll(key))
      return std::move(*value);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  throw std::runtime_error("latest worker result was not delivered");
}
void Drain(Compiler &compiler, Compiler::Key key) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (compiler.Busy() && std::chrono::steady_clock::now() < deadline) {
    Require(!compiler.Poll(key), "obsolete result escaped");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Require(!compiler.Busy(), "obsolete worker did not drain");
}
void Run() {
  Work work;
  Compiler compiler([&](auto request, const auto &token, auto *error) {
    return work.Produce(std::move(request), token, error);
  });
  const Compiler::Key a{1, 1, 1}, b{1, 1, 2}, c{1, 1, 3};
  Require(!compiler.Request({}, Request("invalid")), "zero Play epoch admitted");
  Require(compiler.Request(a, Request("A")), "first request refused");
  work.Wait("A");
  Require(compiler.PollProgress(a).has_value(), "current progress missing");
  Require(compiler.Request(b, Request("B")) && compiler.Request(c, Request("C")),
          "latest request refused");
  Require(!compiler.PollProgress(c), "obsolete progress escaped");
  // Poll must return without waiting for the genuinely blocked asynchronous producer.
  for (unsigned tick = 0; tick < 32; ++tick) {
    Require(!compiler.Poll(c), "blocked worker produced a result");
  }
  work.Release("A");
  const auto latest = Await(compiler, c);
  Require(latest.key == c && latest.error == "C", "latest failure was lost");
  const std::vector<std::string> expected_calls{"A", "C"};
  Require(work.calls == expected_calls && work.cancelled.contains("A"),
          "superseded queued compiler started");

  Require(compiler.Request(c, Request("D")), "same-key first request refused");
  work.Wait("D");
  compiler.Cancel();
  Require(compiler.Request(c, Request("E")), "same-key replacement refused");
  work.Release("D");
  const auto replacement = Await(compiler, c);
  Require(replacement.error == "E" && replacement.request > latest.request,
          "same-key cancelled result was resurrected");
  Require(compiler.Request(c, Request("F")), "epoch test request refused");
  work.Wait("F");
  work.Release("F");
  Drain(compiler, {2, 1, 3});

  Require(compiler.Request(c, Request("Stop")), "stop test request refused");
  work.Wait("Stop");
  compiler.Shutdown();
  Require(!compiler.Busy() && work.active == 0 && work.peak == 1 && work.cancelled.contains("Stop"),
          "shutdown retained or overlapped compiler work");
  Require(compiler.Request({2, 2, 1}, Request("Restart")), "new Play request refused");
  Require(Await(compiler, {2, 2, 1}).error == "Restart", "new Play received an obsolete result");
}
void RunLiveWatcher() {
  using Live = editor::preview::LiveGameScript;
  const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
  const auto root = parent / ("nexora-live-script-" +
                              std::to_string(Live::Clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root / "Scripts");
  const auto write = [](const auto &path, std::string_view text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    out.close();
    Require(static_cast<bool>(out), "watcher fixture write failed");
  };
  write(root / "Scripts/project.json", "{}");
  write(root / "Scripts/game.zig", "a");
  write(root / "project.nexora", "project-a");
  auto source = editor::ReadGameBuildSource(root, "project.nexora", 1024, false);
  Require(source.has_value(), "watcher scope capture failed");
  std::vector<editor::GameBuildSource> scope;
  scope.push_back(std::move(*source));
  editor::GameScriptBuildRequest request;
  request.project_root = root;
  request.script_manifest = "Scripts/project.json";
  std::atomic_uint calls{};
  Live live([&](auto, const auto &, auto *error) -> std::optional<editor::PreparedGameScript> {
    *error = "compile diagnostic " + std::to_string(++calls);
    return {};
  });
  const auto await = [&] {
    const auto deadline = Live::Clock::now() + std::chrono::seconds(5);
    while (Live::Clock::now() < deadline) {
      live.Tick(Live::Clock::now());
      if (auto result = live.TakeResult())
        return std::move(*result);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("live watcher did not deliver a current compilation");
  };
  Require(live.Start({1, 1, 1}, request, scope), "live Play did not start");
  const auto first = await();
  Require(first.error == "compile diagnostic 1", "first live diagnostic missing");
  const auto timestamp = std::filesystem::last_write_time(root / "Scripts/game.zig");
  write(root / "Scripts/game.zig", "b");
  std::filesystem::last_write_time(root / "Scripts/game.zig", timestamp);
  const auto second = await();
  Require(second.key.source > first.key.source && second.error == "compile diagnostic 2",
          "same-size/time edit did not recover from a compiler failure");
  const auto unchanged_until = Live::Clock::now() + std::chrono::milliseconds(600);
  while (Live::Clock::now() < unchanged_until) {
    live.Tick(Live::Clock::now());
    Require(!live.TakeResult(), "unchanged source retried a failed compiler indefinitely");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  write(root / "project.nexora", "project-b");
  bool suspended{};
  const auto deadline = Live::Clock::now() + std::chrono::seconds(5);
  while (!suspended && Live::Clock::now() < deadline) {
    live.Tick(Live::Clock::now());
    if (auto notice = live.TakeNotice())
      suspended = notice->find("restart Play") != std::string::npos;
    Require(!live.TakeResult(), "changed project identity admitted a compiler result");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Require(suspended && calls == 2, "changed project scope compiled or failed to suspend");
  write(root / "project.nexora", "project-a");
  Require(await().error == "compile diagnostic 3",
          "restored scope failed to observe latest source");
  live.Stop();
  Require(!live.Active() && !live.TakeResult(), "Stop retained a live result");
  Require(live.Start({2, 2, 1}, request, scope), "new project/Play failed to start");
  const auto next = await();
  Require(next.key.project == 2 && next.key.play == 2 && next.error == "compile diagnostic 4",
          "previous project/Play result escaped into a new session");
  live.Stop();
  Require(root.parent_path() == parent && std::filesystem::canonical(root) == root,
          "watcher fixture cleanup escaped its private directory");
  std::filesystem::remove_all(root);
}
} // namespace
int main() {
  try {
    Run();
    RunLiveWatcher();
    std::cout << "Latest project/Play/source admission, real async drain and cancellation passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
