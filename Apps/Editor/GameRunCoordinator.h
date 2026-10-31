#pragma once
#include "Nexora/Editor/BuildProcess.h"
#include "Nexora/Editor/EditorWorkspace.h"

namespace nexora::editor::preview {
// Owns one directly launched published game and its process group. No project/UI borrow is
// retained. Poll/Start/Stop/Shutdown are owner-thread calls; close and scope changes drain it.
class GameRunCoordinator final {
public:
  GameRunCoordinator() { jobs_.Start(); }
  ~GameRunCoordinator() { Shutdown(); }
  bool Busy() const noexcept { return active_; }
  BuildProcessSnapshot Snapshot() const { return process_.Snapshot(); }
  bool Current(foundation::Uuid project, const std::filesystem::path &root) const {
    return project_ == project && root_ == root;
  }
  bool Start(foundation::Uuid project, std::filesystem::path root,
             const std::filesystem::path &output, std::string *error) {
    if (active_)
      return false;
    BuildProcessRequest request;
    request.scope = ++scope_;
#if defined(_WIN32)
    request.executable = output / "Game.exe";
#else
    request.executable = output / "Game";
#endif
    request.working_directory = output;
    request.presentation = BuildProcessPresentation::Application;
    if (!process_.Start(std::move(request), error))
      return false;
    project_ = project;
    root_ = std::move(root);
    active_ = true;
    return true;
  }
  void Stop() noexcept { static_cast<void>(process_.Cancel()); }
  std::optional<BuildProcessSnapshot> Poll() {
    if (!active_)
      return {};
    process_.Poll(scope_);
    if (process_.Busy())
      return {};
    active_ = false;
    return process_.Snapshot();
  }
  void Shutdown() noexcept {
    process_.Shutdown();
    active_ = false;
    jobs_.Stop();
  }

private:
  core::JobSystem jobs_{1};
  BuildProcess process_{jobs_};
  foundation::Uuid project_;
  std::filesystem::path root_;
  std::uint64_t scope_{};
  bool active_{};
};
} // namespace nexora::editor::preview
