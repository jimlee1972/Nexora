#include "../../Engine/Runtime/src/GameplayAudioQueue.h"
#include <cassert>
#include <limits>
#include <vector>

using nexora::runtime::detail::GameplayAudioQueue;
namespace {
struct Device {
  GameplayAudioQueue queue;
  std::array<bool, GameplayAudioQueue::Slots> pending{};
  std::vector<std::size_t> submitted;
  unsigned waits{};
  bool timeout{}, reject{};
  GameplayAudioQueue::Driver Driver() {
    return {this, [](void *p, std::size_t slot) { return static_cast<Device *>(p)->pending[slot]; },
            [](void *p, std::size_t slot, std::uint32_t frames) {
              auto &self = *static_cast<Device *>(p);
              assert(!self.pending[slot] && frames == 2);
              if (self.reject)
                return false;
              self.pending[slot] = true;
              self.submitted.push_back(slot);
              return true;
            },
            [](void *p, std::uint32_t ms) {
              auto &self = *static_cast<Device *>(p);
              assert(ms > 0 && ms <= 2000);
              ++self.waits;
              if (self.timeout)
                return false;
              // The first block must remain intact until its simulated device completes it.
              assert(self.queue.Buffer(0)[0] == -32768 && self.queue.Buffer(0)[1] == 32767);
              self.pending[0] = false;
              return true;
            }};
  }
};
} // namespace
int main() {
  Device device;
  std::array<float, 4> block{-1.F, 1.F, .5F, -.5F};
  for (unsigned i = 0; i < GameplayAudioQueue::Slots; ++i)
    assert(device.queue.Write(block, 2, device.Driver()));
  assert(device.waits == 0 && device.submitted == std::vector<std::size_t>({0, 1, 2, 3}));
  // All four owned copies can outlive the mixer's borrowed input and remain pending together.
  block.fill(0);
  for (std::size_t i = 0; i < GameplayAudioQueue::Slots; ++i) {
    const auto samples = device.queue.Buffer(i);
    assert(samples[0] == -32768 && samples[1] == 32767 && samples[2] == 16384 &&
           samples[3] == -16384);
  }
  // Backpressure, rather than each submission, waits. Timeout cannot overwrite borrowed PCM.
  device.timeout = true;
  assert(!device.queue.Write(block, 2, device.Driver()) && device.waits == 1);
  assert(device.queue.Buffer(0)[0] == -32768 && device.submitted.size() == 4);
  device.timeout = false;
  assert(device.queue.Write(block, 2, device.Driver()) && device.waits == 2);
  assert(device.submitted.back() == 0 && device.queue.Buffer(0)[0] == 0);
  assert(device.queue.Buffer(1)[0] == -32768); // Another pending block was never overwritten.
  device.pending.fill(false); // The driver resets all borrowers before queue reset/destruction.
  device.queue.Reset();
  block = {-2.F, 2.F, .25F, -.25F};
  device.reject = true;
  assert(!device.queue.Write(block, 2, device.Driver()));
  device.reject = false;
  assert(device.queue.Write(block, 2, device.Driver()) && device.submitted.back() == 0);
  assert(device.queue.Buffer(0)[0] == -32768 && device.queue.Buffer(0)[1] == 32767 &&
         device.queue.Buffer(0)[2] == 8192);
  device.pending.fill(false);
  assert(!device.queue.Write({}, 0, device.Driver()));
  assert(!device.queue.Write(block, 4097, device.Driver()));
  block[0] = std::numeric_limits<float>::quiet_NaN();
  assert(!device.queue.Write(block, 2, device.Driver()));
  assert(device.submitted.size() == 6);
}
