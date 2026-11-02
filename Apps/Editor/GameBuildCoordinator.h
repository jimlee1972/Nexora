#pragma once
#include "InstalledGameBuildTools.h"
#include <algorithm>
#include <future>
#include <mutex>

namespace nexora::editor::preview {
// Owns a complete worker request/result. No workspace/document/UI reference crosses the worker.
// The host takes the result on its authoring thread and calls the common guarded publisher.
class GameBuildCoordinator final {
public:
  struct Result {
    std::optional<PreparedGameBuild> prepared;
    std::string error;
  };
  ~GameBuildCoordinator() { Shutdown(); }
  bool Busy() const noexcept { return future_.valid(); }
  bool Start(GameBuildRequest request) {
    if (Busy())
      return false;
    cancel_ = core::CancellationSource{};
    progress_ = std::make_shared<ProgressState>();
    observed_revision_ = 0;
    request.progress = [state = progress_,
                        sink = std::move(request.progress)](GameBuildProgress progress) {
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
    future_ = std::async(
        std::launch::async, [request = std::move(request), token = cancel_.Token()]() mutable {
          Result result;
          try {
            result.prepared = PrepareGameBuild(std::move(request), token, &result.error);
          } catch (const std::exception &error) {
            result.error = error.what();
          } catch (...) {
            result.error = "Game build failed with an unknown error.";
          }
          return result;
        });
    return true;
  }
  std::optional<GameBuildProgress> PollProgress() {
    if (!progress_)
      return {};
    std::scoped_lock lock(progress_->mutex);
    if (observed_revision_ == progress_->revision)
      return {};
    observed_revision_ = progress_->revision;
    return progress_->latest;
  }
  GameBuildProgress Progress() const {
    if (!progress_)
      return {};
    std::scoped_lock lock(progress_->mutex);
    return progress_->latest;
  }
  void Cancel() noexcept { cancel_.Cancel(); }
  const core::CancellationToken Token() const noexcept { return cancel_.Token(); }
  std::optional<Result> Poll() {
    if (!future_.valid() || future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
      return {};
    return future_.get();
  }
  void Shutdown() noexcept {
    Cancel();
    if (future_.valid()) {
      try {
        static_cast<void>(future_.get());
      } catch (...) {
      }
    }
  }

private:
  struct ProgressState {
    std::mutex mutex;
    GameBuildProgress latest;
    std::uint64_t revision{};
  };
  std::shared_ptr<ProgressState> progress_;
  std::uint64_t observed_revision_{};
  core::CancellationSource cancel_;
  std::future<Result> future_;
};
} // namespace nexora::editor::preview
