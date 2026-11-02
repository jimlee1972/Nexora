#pragma once
#include "GameScriptCompiler.h"
#include "Nexora/Editor/ContentBrowser.h"
#include <utility>

namespace nexora::editor::preview {
// Main-thread Play coordinator. Both scanning and compilation own their worker inputs; only
// this owner accepts current project/Play/source generations at a gameplay dispatch boundary.
class LiveGameScript final {
public:
  using Clock = std::chrono::steady_clock;
  using Key = GameScriptCompiler::Key;
  explicit LiveGameScript(GameScriptCompiler::Producer producer = PrepareGameScript)
      : compiler_(std::move(producer)) {}
  ~LiveGameScript() { Stop(); }
  bool Start(Key key, GameScriptBuildRequest request, std::vector<GameBuildSource> scope) {
    Stop();
    if (!key.project || !key.play)
      return false;
    key_ = {key.project, key.play, 1};
    request_ = std::move(request);
    scope_ = std::move(scope);
    active_ = true;
    next_scan_ = {};
    notice_ = "Observing script sources before background compilation.";
    return true;
  }
  [[nodiscard]] bool Active() const noexcept { return active_; }
  [[nodiscard]] Key Current() const noexcept { return key_; }
  [[nodiscard]] const std::filesystem::path &Root() const noexcept { return request_.project_root; }
  [[nodiscard]] bool ScopeCurrent(std::string *error) const {
    return GameBuildSourcesCurrent(request_.project_root, scope_, error);
  }
  void Tick(Clock::time_point now) try {
    if (!active_)
      return;
    if (scan_.valid() && scan_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      auto observation = scan_.get();
      const bool changed = observation.sources != sources_ || observation.error != error_;
      if (changed) {
        compiler_.Cancel();
        result_.reset();
        sources_ = std::move(observation.sources);
        error_ = std::move(observation.error);
        if (key_.source == std::numeric_limits<std::uint64_t>::max()) {
          active_ = false;
          notice_ = "Script observation generation exhausted; restart Play.";
          return;
        }
        ++key_.source;
        debouncer_ = WatcherDebouncer(std::chrono::milliseconds(250));
        if (sources_) {
          debouncer_.Push({request_.script_manifest, now, false});
          notice_ = "Script source changed; waiting for edits to settle.";
        } else {
          notice_ = "Script reload suspended: " + error_;
        }
      }
      next_scan_ = now + std::chrono::milliseconds(500);
    }
    if (!scan_.valid() && now >= next_scan_) {
      scan_cancel_ = core::CancellationSource{};
      scan_ = std::async(std::launch::async, [request = request_, scope = scope_,
                                              token = scan_cancel_.Token()] {
        Observation observation;
        try {
          if (!GameBuildSourcesCurrent(request.project_root, scope, &observation.error)) {
            observation.error =
                "Project identity or Build Settings changed; restart Play. " + observation.error;
          } else {
            observation.sources = ObserveGameScriptSources(request, token, &observation.error);
          }
        } catch (const std::exception &error) {
          observation.error = error.what();
        } catch (...) {
          observation.error = "Script observation failed.";
        }
        return observation;
      });
    }
    if (!debouncer_.Flush(now).empty() && sources_) {
      if (compiler_.Request(key_, request_))
        notice_ = "Compiling scripts in background; the current gameplay generation continues.";
      else
        notice_ = "Could not start script compilation; restart Play.";
    }
    if (auto result = compiler_.Poll(key_))
      result_ = std::move(result);
  } catch (const std::exception &error) {
    Stop();
    notice_ = "Automatic script reload stopped; restart Play. " + std::string(error.what());
  } catch (...) {
    Stop();
    notice_ = "Automatic script reload stopped; restart Play.";
  }
  std::optional<GameBuildProgress> PollProgress() { return compiler_.PollProgress(key_); }
  std::optional<std::string> TakeNotice() { return std::exchange(notice_, {}); }
  std::optional<GameScriptCompiler::Result> TakeResult() { return std::exchange(result_, {}); }
  void Stop() noexcept {
    active_ = false;
    scan_cancel_.Cancel();
    // Cancel both independent workers before draining either. No module/World lifetime is
    // released while tools or the observer can still deliver a borrowed/stale result.
    compiler_.Cancel();
    if (scan_.valid()) {
      try {
        static_cast<void>(scan_.get());
      } catch (...) {
      }
    }
    compiler_.Shutdown();
    result_.reset();
    sources_.reset();
    scope_.clear();
    error_.clear();
    notice_.reset();
    key_ = {};
    debouncer_ = WatcherDebouncer(std::chrono::milliseconds(250));
  }

private:
  struct Observation final {
    std::optional<GameScriptSources> sources;
    std::string error;
  };
  bool active_{};
  Key key_{};
  GameScriptBuildRequest request_;
  std::vector<GameBuildSource> scope_;
  std::optional<GameScriptSources> sources_;
  std::string error_;
  std::optional<std::string> notice_;
  std::optional<GameScriptCompiler::Result> result_;
  GameScriptCompiler compiler_;
  WatcherDebouncer debouncer_{std::chrono::milliseconds(250)};
  core::CancellationSource scan_cancel_;
  std::future<Observation> scan_;
  Clock::time_point next_scan_{};
};
} // namespace nexora::editor::preview
