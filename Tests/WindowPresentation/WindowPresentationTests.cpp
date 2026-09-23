#include "Nexora/Presentation/Surface.h"

#include <cassert>
#include <deque>
#include <thread>
#include <vector>

namespace {
using namespace Nexora;

class FakeWindowSystem final : public Window::IWindowSystem {
public:
  std::thread::id OwnerThread() const noexcept override { return owner_; }
  Window::WindowResult Create(const Window::WindowDescriptor &descriptor) override {
    if (std::this_thread::get_id() != owner_)
      return {{}, Window::WindowError::WrongThread};
    if (descriptor.width == 0 || descriptor.height == 0)
      return {{}, Window::WindowError::InvalidDescriptor};
    alive_ = true;
    return {{1}, Window::WindowError::None};
  }
  Window::WindowError Destroy(Window::WindowHandle window) override {
    if (std::this_thread::get_id() != owner_)
      return Window::WindowError::WrongThread;
    if (!alive_ || window.value != 1)
      return Window::WindowError::InvalidHandle;
    alive_ = false;
    pending_.clear();
    pumped_.clear();
    return Window::WindowError::None;
  }
  std::span<const Window::WindowEvent> PumpEvents() override {
    pumped_.clear();
    for (const auto &event : pending_) {
      if (event.type == Window::WindowEventType::Resized && !pumped_.empty() &&
          pumped_.back().type == Window::WindowEventType::Resized &&
          pumped_.back().window == event.window) {
        pumped_.back() = event;
      } else {
        pumped_.push_back(event);
      }
    }
    pending_.clear();
    return pumped_;
  }
  void Resize(std::uint32_t width, std::uint32_t height, std::uint64_t timestamp) {
    pending_.push_back({{1}, Window::WindowEventType::Resized, timestamp, width, height});
  }
  [[nodiscard]] bool Alive() const noexcept { return alive_; }

private:
  std::thread::id owner_ = std::this_thread::get_id();
  bool alive_ = false;
  std::deque<Window::WindowEvent> pending_;
  std::vector<Window::WindowEvent> pumped_;
};

class FakeSurface final : public Presentation::ISurface {
public:
  explicit FakeSurface(const Presentation::SurfaceDescriptor &descriptor)
      : renderThread_(std::this_thread::get_id()), width_(descriptor.width),
        height_(descriptor.height) {}
  std::thread::id RenderThread() const noexcept override { return renderThread_; }
  Presentation::SurfaceStatus NotifyWindowExtent(std::uint32_t width,
                                                 std::uint32_t height) noexcept override {
    width_ = width;
    height_ = height;
    return Status();
  }
  Presentation::SurfaceStatus Acquire() override {
    if (destroyed_)
      return Presentation::SurfaceStatus::SurfaceLost;
    if (nextStatus_ != Presentation::SurfaceStatus::Ready) {
      const auto result = nextStatus_;
      nextStatus_ = Presentation::SurfaceStatus::Ready;
      return result;
    }
    return Status();
  }
  Presentation::SurfaceStatus Present() override {
    return destroyed_ ? Presentation::SurfaceStatus::SurfaceLost : Status();
  }
  Presentation::SurfaceStatus DrainAndDestroy() override {
    if (!destroyed_)
      ++drainCount_;
    destroyed_ = true;
    return Presentation::SurfaceStatus::Ready;
  }
  [[nodiscard]] std::uint32_t DrainCount() const noexcept { return drainCount_; }
  void Inject(Presentation::SurfaceStatus status) noexcept { nextStatus_ = status; }

private:
  Presentation::SurfaceStatus Status() const noexcept {
    return width_ == 0 || height_ == 0 ? Presentation::SurfaceStatus::ZeroExtent
                                       : Presentation::SurfaceStatus::Ready;
  }
  std::thread::id renderThread_;
  std::uint32_t width_;
  std::uint32_t height_;
  std::uint32_t drainCount_ = 0;
  bool destroyed_ = false;
  Presentation::SurfaceStatus nextStatus_ = Presentation::SurfaceStatus::Ready;
};
} // namespace

int main() {
  FakeWindowSystem windows;
  const auto created = windows.Create({"Fake", 640, 480, true, false});
  assert(created);
  Window::WindowResult crossThreadCreate;
  std::thread wrongThread([&] { crossThreadCreate = windows.Create({}); });
  wrongThread.join();
  assert(crossThreadCreate.error == Window::WindowError::WrongThread);

  FakeSurface surface({created.handle, 640, 480, 2});
  assert(surface.Acquire() == Presentation::SurfaceStatus::Ready);
  windows.Resize(800, 600, 1);
  windows.Resize(0, 0, 2);
  const auto events = windows.PumpEvents();
  assert(events.size() == 1 && events[0].width == 0 && events[0].height == 0);
  assert(surface.NotifyWindowExtent(events[0].width, events[0].height) ==
         Presentation::SurfaceStatus::ZeroExtent);
  assert(surface.Acquire() == Presentation::SurfaceStatus::ZeroExtent);
  assert(surface.NotifyWindowExtent(1920, 1080) == Presentation::SurfaceStatus::Ready);
  assert(surface.Present() == Presentation::SurfaceStatus::Ready);
  surface.Inject(Presentation::SurfaceStatus::OutOfDate);
  assert(surface.Acquire() == Presentation::SurfaceStatus::OutOfDate);
  surface.Inject(Presentation::SurfaceStatus::SurfaceLost);
  assert(surface.Acquire() == Presentation::SurfaceStatus::SurfaceLost);
  surface.Inject(Presentation::SurfaceStatus::DeviceLost);
  assert(surface.Acquire() == Presentation::SurfaceStatus::DeviceLost);

  assert(surface.DrainAndDestroy() == Presentation::SurfaceStatus::Ready);
  assert(surface.DrainAndDestroy() == Presentation::SurfaceStatus::Ready);
  assert(surface.DrainCount() == 1);
  assert(windows.Destroy(created.handle) == Window::WindowError::None);
  assert(!windows.Alive());
  return 0;
}
