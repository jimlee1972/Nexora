#pragma once
#include "Nexora/Editor/GameScriptBuild.h"
#include <algorithm>
#include <future>
#include <limits>
#include <mutex>

namespace nexora::editor::preview {
// Owner-thread coordinator. Every worker capture owns its request/result; no workspace,
// document, Play World or active module crosses this boundary.
class GameScriptCompiler final {
public:
  struct Key final {
    std::uint64_t project{}, play{}, source{};
    friend bool operator==(const Key &, const Key &) = default;
  };
  struct Result final {
    Key key;
    std::uint64_t request{};
    std::optional<PreparedGameScript> prepared;
    std::string error;
  };
  using Producer = std::function<std::optional<PreparedGameScript>(
      GameScriptBuildRequest, const core::CancellationToken &, std::string *)>;

  explicit GameScriptCompiler(Producer producer = PrepareGameScript)
      : producer_(std::move(producer)) {}
  ~GameScriptCompiler() { Shutdown(); }
  GameScriptCompiler(const GameScriptCompiler &) = delete;
  GameScriptCompiler &operator=(const GameScriptCompiler &) = delete;

  // Latest request wins. Cancellation drains before the next toolchain starts, including
  // repeated requests with equal external keys. Epoch zero never identifies a live Play.
  bool Request(Key key, GameScriptBuildRequest request) {
    if (!key.project || !key.play || !key.source || !producer_ ||
        serial_ == std::numeric_limits<std::uint64_t>::max())
      return false;
    Pending next{key, ++serial_, std::move(request)};
    latest_ = Identity{key, next.serial};
    if (future_.valid()) {
      cancel_.Cancel();
      pending_ = std::move(next);
    } else {
      Start(std::move(next));
    }
    return true;
  }
  [[nodiscard]] bool Busy() const noexcept { return future_.valid(); }

  // Poll after processing source/project/Play changes. Obsolete successes AND errors are
  // discarded. The consumer still owns source/artifact admission and the Runtime transaction.
  std::optional<Result> Poll(Key current) {
    if (!future_.valid() || future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
      return {};
    const bool cancelled = cancel_.Token().IsCancellationRequested();
    auto result = future_.get();
    const bool accepted = !cancelled && latest_ && latest_->key == current &&
                          latest_->key == result.key && latest_->serial == result.request;
    if (pending_) {
      auto next = std::move(*pending_);
      pending_.reset();
      if (latest_ && next.key == current && next.serial == latest_->serial)
        Start(std::move(next));
    }
    if (!accepted)
      return {};
    return result;
  }
  std::optional<GameBuildProgress> PollProgress(Key current) {
    if (!progress_ || !latest_ || latest_->key != current || running_.key != current ||
        running_.serial != latest_->serial || cancel_.Token().IsCancellationRequested())
      return {};
    std::scoped_lock lock(progress_->mutex);
    if (observed_ == progress_->revision)
      return {};
    observed_ = progress_->revision;
    return progress_->latest;
  }
  void Cancel() noexcept {
    cancel_.Cancel();
    pending_.reset();
    latest_.reset();
  }
  void Shutdown() noexcept {
    Cancel();
    if (future_.valid()) {
      try {
        static_cast<void>(future_.get());
      } catch (...) {
      }
    }
    progress_.reset();
  }

private:
  struct Identity final {
    Key key;
    std::uint64_t serial{};
  };
  struct Pending final {
    Key key;
    std::uint64_t serial{};
    GameScriptBuildRequest request;
  };
  struct Progress final {
    std::mutex mutex;
    GameBuildProgress latest;
    std::uint64_t revision{};
  };
  void Start(Pending next) {
    cancel_ = core::CancellationSource{};
    progress_ = std::make_shared<Progress>();
    observed_ = 0;
    running_ = {next.key, next.serial};
    next.request.progress = [state = progress_,
                             sink = std::move(next.request.progress)](GameBuildProgress progress) {
      if (sink)
        sink(progress);
      std::scoped_lock lock(state->mutex);
      auto diagnostics = std::move(state->latest.diagnostics);
      for (auto &value : progress.diagnostics)
        if (std::ranges::find(diagnostics, value) == diagnostics.end())
          diagnostics.push_back(std::move(value));
      progress.diagnostics_truncated = progress.diagnostics_truncated ||
                                       state->latest.diagnostics_truncated ||
                                       diagnostics.size() > GameBuildProgress::kMaximumDiagnostics;
      if (diagnostics.size() > GameBuildProgress::kMaximumDiagnostics)
        diagnostics.erase(diagnostics.begin(),
                          diagnostics.end() - GameBuildProgress::kMaximumDiagnostics);
      progress.diagnostics = std::move(diagnostics);
      if (progress.output.empty()) {
        progress.output = std::move(state->latest.output);
        progress.output_truncated = state->latest.output_truncated;
      }
      state->latest = std::move(progress);
      ++state->revision;
    };
    future_ = std::async(std::launch::async, [next = std::move(next), token = cancel_.Token(),
                                              produce = producer_]() mutable {
      Result result{next.key, next.serial, {}, {}};
      try {
        result.prepared = produce(std::move(next.request), token, &result.error);
      } catch (const std::exception &error) {
        result.error = error.what();
      } catch (...) {
        result.error = "Script compilation failed with an unknown error.";
      }
      return result;
    });
  }
  Producer producer_;
  std::uint64_t serial_{}, observed_{};
  Identity running_{};
  std::optional<Identity> latest_;
  std::optional<Pending> pending_;
  core::CancellationSource cancel_;
  std::future<Result> future_;
  std::shared_ptr<Progress> progress_;
};
} // namespace nexora::editor::preview
