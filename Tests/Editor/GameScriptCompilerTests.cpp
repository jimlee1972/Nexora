#include "GameScriptCompiler.h"
#include <condition_variable>
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
} // namespace
int main() {
  try {
    Run();
    std::cout << "Latest project/Play/source admission, real async drain and cancellation passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
