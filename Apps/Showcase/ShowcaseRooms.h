#pragma once
#include "Nexora/Presentation/RenderSurface.h"
#include "ShowcaseProbes.h"
#include <memory>
#include <string>
#include <vector>

namespace nexora::showcase {
// Showcase-owned demo state. Calls are serialized on the game/presentation thread; no borrowed
// Runtime pointers survive mutations. Geometry and report snapshots own their returned storage.
class RoomSession final {
public:
  explicit RoomSession(std::string scene, bool tour = false, bool minimal = false,
                       std::string pluginLibrary = {});
  ~RoomSession();
  RoomSession(const RoomSession &) = delete;
  RoomSession &operator=(const RoomSession &) = delete;
  void Event(const Nexora::Window::WindowEvent &event, std::uint32_t width, std::uint32_t height);
  void Tick(double seconds);
  void Select(std::string_view room);
  void ReplayTour();
  void RerunProbe(std::size_t milestone, ErrorInjection injection = ErrorInjection::None);
  [[nodiscard]] bool Healthy() const;
  [[nodiscard]] std::string_view Selected() const;
  [[nodiscard]] std::vector<ProbeResult> Probes() const;
  [[nodiscard]] std::string Report() const;
  [[nodiscard]] std::string Markdown() const;
  [[nodiscard]] Nexora::Presentation::SceneDrawData Scene(std::uint32_t width,
                                                          std::uint32_t height);
  [[nodiscard]] Nexora::Presentation::UiDrawData
  Overlay(std::uint32_t width, std::uint32_t height, std::string_view backend,
          const Nexora::Presentation::SurfaceDiagnostics &diagnostics, double frameMs);

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace nexora::showcase
