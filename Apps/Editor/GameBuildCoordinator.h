#pragma once
#include "InstalledGameBuildTools.h"
#include <future>

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
  core::CancellationSource cancel_;
  std::future<Result> future_;
};
} // namespace nexora::editor::preview
