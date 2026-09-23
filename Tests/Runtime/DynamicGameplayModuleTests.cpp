#include "Nexora/Runtime/GameplayModuleHost.h"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace {

struct Barrier final {
  std::mutex mutex;
  std::condition_variable changed;
  bool job_running{};
  std::uint64_t waited_generation{};
};

void WaitForJobs(void *opaque, std::uint64_t generation) {
  auto &barrier = *static_cast<Barrier *>(opaque);
  std::unique_lock lock(barrier.mutex);
  barrier.waited_generation = generation;
  barrier.changed.notify_all();
  barrier.changed.wait(lock, [&barrier] { return !barrier.job_running; });
}

NexoraGameplayHostV3 MakeHost() {
  NexoraGameplayHostV3 host{};
  host.struct_size = sizeof(host);
  host.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  return host;
}

} // namespace

int main(int argc, char **argv) {
  assert(argc == 4);
  const std::filesystem::path first = argv[1];
  const std::filesystem::path second = argv[2];
  const std::filesystem::path failing = argv[3];

  const auto discovered = nexora::runtime::GameplayModuleHost::Discover(
      first.parent_path(), first.stem().string().starts_with("lib")
                               ? first.stem().string().substr(3)
                               : first.stem().string());
  assert(std::filesystem::equivalent(discovered, first));
  assert(nexora::runtime::GameplayModuleHost::Discover(first.parent_path(), "missing").empty());

  Barrier barrier;
  nexora::runtime::GameplayModuleHost host(MakeHost(), {&barrier, &WaitForJobs});
  assert(host.Load(first));
  assert(host.Generation() == 1);
  assert(host.Update(0.016));

  assert(!host.Reload(first.parent_path() / "missing-module"));
  assert(host.Generation() == 1);
  assert(host.Update(0.016));
  assert(!host.Reload(failing));
  assert(host.Generation() == 1);
  assert(host.Update(0.016));

  {
    std::scoped_lock lock(barrier.mutex);
    barrier.job_running = true;
  }
  std::atomic_bool reload_finished{};
  std::thread reload([&] {
    assert(host.Reload(second));
    reload_finished = true;
  });
  {
    std::unique_lock lock(barrier.mutex);
    barrier.changed.wait(lock, [&barrier] { return barrier.waited_generation == 1; });
    assert(!reload_finished);
    barrier.job_running = false;
  }
  barrier.changed.notify_all();
  reload.join();
  assert(host.Generation() == 2);

  for (int index = 0; index < 16; ++index) {
    assert(host.Reload(index % 2 == 0 ? first : second));
    assert(host.Update(0.016));
  }
  assert(host.Generation() == 18);

  {
    std::scoped_lock lock(barrier.mutex);
    barrier.job_running = true;
    barrier.waited_generation = 0;
  }
  std::atomic_bool shutdown_finished{};
  std::thread shutdown([&] {
    host.Unload();
    shutdown_finished = true;
  });
  {
    std::unique_lock lock(barrier.mutex);
    barrier.changed.wait(lock, [&barrier] { return barrier.waited_generation == 18; });
    assert(!shutdown_finished);
    barrier.job_running = false;
  }
  barrier.changed.notify_all();
  shutdown.join();
  assert(!host.IsLoaded());
}
