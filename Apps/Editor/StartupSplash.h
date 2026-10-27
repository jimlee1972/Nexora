#pragma once
#include <memory>
namespace nexora::editor::app {
enum class StartupStage { Settings, Graphics, Project, Scene, Workspace };
// Owner-thread initialization reports actual stages. Native UI owns only artwork/window state.
class StartupSplash final {
public:
  explicit StartupSplash(bool enabled);
  ~StartupSplash();
  StartupSplash(const StartupSplash &) = delete;
  StartupSplash &operator=(const StartupSplash &) = delete;
  void Stage(StartupStage stage);
  void PrepareMainWindow();
  void Ready();

private:
  struct Native;
  std::unique_ptr<Native> native_;
};
} // namespace nexora::editor::app
