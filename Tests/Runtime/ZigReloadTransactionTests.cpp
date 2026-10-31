#include "GameplayReloadFixture.h"

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
using namespace nexora;
using namespace nexora::test::reload;
using Path = std::filesystem::path;
using JobCall = void (*)();

struct Directory final {
  Path parent{std::filesystem::canonical(std::filesystem::current_path())};
  Path base{parent / ("reload-generations-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  Path path{base / Path{u8"模組 copies"}};
  Directory() {
    assert(std::filesystem::create_directory(base));
    assert(std::filesystem::create_directory(path));
  }
  ~Directory() {
    const auto physical = std::filesystem::canonical(base);
    assert(physical.parent_path() == parent && physical == base);
    std::filesystem::remove_all(physical); // Only this newly created, verified fixture root.
  }
};

JobCall BorrowJob(const Path &library) {
#if defined(_WIN32)
  const auto handle = LoadLibraryW(library.c_str());
  assert(handle);
  auto *symbol = reinterpret_cast<void *>(GetProcAddress(handle, "NexoraReloadFixtureJob"));
#else
  auto *handle = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
  assert(handle);
  auto *symbol = dlsym(handle, "NexoraReloadFixtureJob");
#endif
  assert(symbol);
  JobCall result{};
  static_assert(sizeof(result) == sizeof(symbol));
  std::memcpy(&result, &symbol, sizeof(result));
#if defined(_WIN32)
  assert(FreeLibrary(handle));
#else
  assert(dlclose(handle) == 0);
#endif
  // The caller already owns a work lease. No probe handle keeps the retired DLL alive.
  return result;
}

bool Loaded(const Path &library) {
#if defined(_WIN32)
  return GetModuleHandleW(library.c_str()) != nullptr;
#else
  auto *handle = dlopen(library.c_str(), RTLD_NOW | RTLD_NOLOAD);
  if (!handle)
    return false;
  assert(dlclose(handle) == 0);
  return true;
#endif
}

Path Copy(const Path &source, const Path &directory, std::string_view name) {
  const auto path = directory / (std::string(name) + source.extension().string());
  assert(std::filesystem::copy_file(source, path));
  return path;
}

void Verify(const Owner &owner, std::uint64_t expected, runtime::Id scene, runtime::Id entity) {
  const auto *value = owner.world.FindEntity(entity);
  assert(value && value->transform.x == static_cast<double>(expected));
  assert(owner.scene == scene && owner.entity == entity && owner.world.FindScene(scene));
  assert(owner.world.ActiveSceneCount() == 1 && owner.world.ActiveEntityCount() == 1);
  assert(owner.blocks.size() == 1 && owner.allocations - owner.frees == 1);
}
} // namespace

int main(int argc, char **argv) {
  assert(argc == 9);
  Directory directory;
  const Path normal{argv[1]};
  std::array<Path, 7> failures;
  for (std::size_t index = 0; index < failures.size(); ++index)
    failures[index] =
        Copy(Path{argv[index + 2]}, directory.path, "rejected-" + std::to_string(index));
  const std::array reasons{
      Host::ReloadFailure::RestoreState,    Host::ReloadFailure::Initialization,
      Host::ReloadFailure::Initialization,  Host::ReloadFailure::RestoreState,
      Host::ReloadFailure::RestartRequired, Host::ReloadFailure::InvalidCandidate,
      Host::ReloadFailure::RestoreState};

  Owner owner(false);
  Host host(owner.Api(), owner.Barrier(), owner.Transaction());
  auto current = Copy(normal, directory.path, "generation-1");
  const bool inspected = Host::Inspect(current);
  const bool loaded = host.Load(current);
  if (!loaded)
    std::cerr << "Initial module failed: inspected=" << inspected
              << " allocations=" << owner.allocations << " frees=" << owner.frees
              << " blocks=" << owner.blocks.size() << " entity=" << owner.entity
              << " active=" << owner.world.ActiveEntityCount() << '\n';
  assert(loaded);
  const auto scene = owner.scene, entity = owner.entity;
  assert(entity != 0 && host.Generation() == 1);
  std::uint64_t counter{1};
  assert(host.Update(0.1));
  Verify(owner, counter, scene, entity);
  assert(!host.Reload(current) && host.GetReloadFailure() == Host::ReloadFailure::RestartRequired);
  Verify(owner, counter, scene, entity);
  for (unsigned index = 0; index < 32; ++index) {
    const auto generation = host.Generation();
    for (std::size_t failure = 0; failure < failures.size(); ++failure) {
      assert(!host.Reload(failures[failure]));
      assert(host.GetReloadFailure() == reasons[failure]);
      assert(host.Generation() == generation && !Loaded(failures[failure]));
      Verify(owner, counter, scene, entity);
      // The old Zig implementation remains callable after every rejected candidate.
      assert(host.Update(0.1));
      Verify(owner, ++counter, scene, entity);
    }
    assert(owner.finalizers == index + 1);
    const auto next = Copy(normal, directory.path, "generation-" + std::to_string(index + 2));
    if (index == 15) {
      auto lease = host.AcquireWork(generation);
      assert(lease);
      auto call = BorrowJob(current);
      {
        std::scoped_lock lock(owner.mutex);
        owner.barrier_entered = false;
        owner.release_job = false;
      }
      owner.job_finished = false;
      std::thread job([&, ticket = std::move(lease)]() mutable {
        {
          std::unique_lock lock(owner.mutex);
          owner.changed.wait(lock, [&] { return owner.release_job; });
        }
        call(); // Actual old-generation Zig code mutates its live game state.
        call = nullptr;
        owner.job_finished = true;
        ticket = {}; // All borrowed module code/captures have finished before retirement.
      });
      std::thread release([&] {
        std::unique_lock lock(owner.mutex);
        owner.changed.wait(lock, [&] { return owner.barrier_entered; });
        assert(!host.AcquireWork(generation));
        owner.release_job = true;
        owner.changed.notify_all();
      });
      assert(host.Reload(next)); // World hooks remain on the owning thread.
      job.join();
      release.join();
      assert(owner.job_finished && call == nullptr);
      counter += 100; // Capture before the real job barrier would lose these 100 ticks.
    } else {
      assert(host.Reload(next));
    }
    assert(host.Generation() == generation + 1 && !Loaded(current));
    assert(!host.AcquireWork(generation) && host.GetReloadFailure() == Host::ReloadFailure::None);
    Verify(owner, counter, scene, entity);
    assert(host.Update(0.1));
    Verify(owner, ++counter, scene, entity);
    current = next;
  }
  assert(host.Generation() == 33 && host.GetReloadStats().successful_reloads == 32);
  assert(host.GetReloadStats().migrated_bytes == 40);
  host.Unload();
  assert(owner.blocks.empty() && owner.allocations == owner.frees);
  assert(!owner.world.FindEntity(entity) && !Loaded(current) && !host.AcquireWork(33));

  // A transaction may release the original embedding context. Subsequent Load must use the
  // latest surviving host table, and generation 1 must never become valid again on this owner.
  owner.world = runtime::World{runtime::WorldKind::Play};
  owner.scene = owner.world.LoadScene("Reload", true);
  owner.entity = 0;
  assert(host.Load(current) && host.Generation() == 34);
  assert(!host.AcquireWork(1) && !host.AcquireWork(33));
  assert(host.Update(0.1));
  Verify(owner, 1, scene, owner.entity);
  host.Unload();
  assert(owner.blocks.empty() && owner.allocations == owner.frees && !Loaded(current));
  std::cout << "Real Zig: 32 successful unique-DLL reloads, 224 rejected candidates, retained "
               "counter/scene/entity, registered old-code job barrier, safe retirement and "
               "failed-candidate DLL finalizers and unload/reload generations verified.\n";
}
