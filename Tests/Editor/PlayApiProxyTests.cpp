#include "Nexora/Runtime/GameplayAudio.h"
#include "Nexora/Runtime/GameplayInput.h"
#include "Nexora/Runtime/GameplayStorage.h"
#include "PlayApiProxy.h"
#include <array>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iterator>
#include <map>
#include <thread>

namespace {
using namespace nexora;
using namespace nexora::editor::preview;
using Phase = PlayServiceGate::Phase;
#define NX_PROXY_BIND(proxy, type, field, writes, external)                                        \
  proxy.Bind<&type::field, offsetof(type, field) + sizeof(type::field), writes, external>()

NexoraGameplayActionStateV1 Action(NexoraGameplayInputApiV1 api) {
  NexoraGameplayActionStateV1 value{};
  value.struct_size = sizeof(value);
  value.abi_version = 1;
  assert(api.read_action(api.context, 0, 1, &value) == 0);
  return value;
}
void Input() {
  runtime::GameplayInput input;
  const auto provider = input.Api();
  const NexoraGameplayInputBindingV1 binding{sizeof(binding), 1, 1, 0, 69, 1, 0, 0};
  assert(provider.bind(provider.context, 0, 1, &binding) == 0 && input.Submit(0, 1, 0, 69, 1));
  PlayServiceGate gate;
  PlayApiProxy proxy(gate, provider);
  NX_PROXY_BIND(proxy, NexoraGameplayInputApiV1, read_action, false, false);
  NX_PROXY_BIND(proxy, NexoraGameplayInputApiV1, clear_bindings, true, false);
  const auto cached = proxy.Get();
  assert(cached.bind == nullptr && Action(cached).held && gate.SetPhase(Phase::Capture));
  assert(cached.clear_bindings(cached.context, 0, 1) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
         !gate.Valid() && Action(cached).held);
  assert(gate.SetPhase(Phase::Live));
  std::thread wrong([&] {
    NexoraGameplayActionStateV1 value{sizeof(value), 1, 99, .5F, 7, 8, 9};
    assert(cached.read_action(cached.context, 0, 1, &value) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
           value.sequence == 99 && value.value == .5F && value.held == 7);
  });
  wrong.join();
  assert(!gate.Valid() && gate.SetPhase(Phase::Live));
  auto short_provider = provider;
  short_provider.struct_size = offsetof(NexoraGameplayInputApiV1, bind);
  assert(proxy.Redirect(short_provider));
  assert(cached.clear_bindings(cached.context, 0, 1) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED &&
         Action(cached).held);
  assert(proxy.Redirect(provider));
  auto branch = input.ForkForReload();
  PlayServiceGate next_gate(Phase::Candidate);
  PlayApiProxy next_proxy(next_gate, branch->Api());
  NX_PROXY_BIND(next_proxy, NexoraGameplayInputApiV1, read_action, false, false);
  assert(gate.SetPhase(Phase::Retired) && cached.clear_bindings(cached.context, 0, 1) == 0 &&
         Action(next_proxy.Get()).held && next_gate.Valid());
}

void Storage() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-proxy-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove_all(path, error);
    }
  } cleanup{root};
  runtime::GameplayStorage storage(root, "reload-proxy");
  NexoraStorageApiV1 provider{};
  provider.struct_size = sizeof(provider);
  provider.abi_version = 1;
  assert(storage.GetApi(provider) == 0);
  PlayServiceGate gate;
  PlayApiProxy proxy(gate, provider);
  NX_PROXY_BIND(proxy, NexoraStorageApiV1, save, true, true);
  NX_PROXY_BIND(proxy, NexoraStorageApiV1, remove, true, true);
  NX_PROXY_BIND(proxy, NexoraStorageApiV1, load, false, true);
  const auto cached = proxy.Get();
  const std::array<std::uint8_t, 4> bytes{0, 17, 255, 42};
  assert(cached.save(cached.context, "run", 3, 7, bytes.data(), 4) == 0);
  const auto files = [&] {
    std::map<std::filesystem::path, std::string> result;
    for (const auto &entry : std::filesystem::directory_iterator(root)) {
      std::ifstream stream(entry.path(), std::ios::binary);
      result.emplace(entry.path().filename(),
                     std::string(std::istreambuf_iterator<char>(stream), {}));
    }
    return result;
  };
  const auto baseline = files();
  for (const auto phase : {Phase::Capture, Phase::Candidate, Phase::Retired}) {
    assert(gate.SetPhase(phase));
    assert(cached.save(cached.context, "run", 3, 9, bytes.data(), 1) == NEXORA_STORAGE_IO_ERROR &&
           cached.remove(cached.context, "run", 3) == NEXORA_STORAGE_IO_ERROR && !gate.Valid());
    std::array<std::uint8_t, 4> output{};
    NexoraStorageInfoV1 info{sizeof(info), 1, 99, 99};
    assert(cached.load(cached.context, "run", 3, output.data(), 4, &info) == 0 && output == bytes &&
           info.schema_version == 7 && files() == baseline);
  }
  assert(gate.SetPhase(Phase::Live));
  std::thread wrong([&] {
    NexoraStorageInfoV1 info{sizeof(info), 1, 99, 99};
    assert(cached.load(cached.context, "run", 3, nullptr, 0, &info) ==
               NEXORA_STORAGE_WRONG_THREAD &&
           info.schema_version == 99 && info.payload_size == 99);
  });
  wrong.join();
  assert(!gate.Valid() && files() == baseline);
  assert(gate.SetPhase(Phase::Live) && cached.remove(cached.context, "run", 3) == 0);
}

void AllocationSignatures() {
  struct Memory {
    std::array<std::byte, 64> bytes;
    unsigned allocations{}, frees{};
  } memory{};
  NexoraGameplayHostV3 host{};
  host.struct_size = sizeof(host);
  host.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  host.capabilities = NEXORA_GAMEPLAY_CAPABILITY_HOST_ALLOCATOR;
  host.context = &memory;
  host.allocate = [](void *context, std::uint64_t, std::uint64_t, std::uint64_t) -> void * {
    auto &value = *static_cast<Memory *>(context);
    ++value.allocations;
    return value.bytes.data();
  };
  host.deallocate = [](void *context, std::uint64_t, void *, std::uint64_t, std::uint64_t) {
    ++static_cast<Memory *>(context)->frees;
  };
  PlayServiceGate gate;
  PlayApiProxy proxy(gate, host);
  NX_PROXY_BIND(proxy, NexoraGameplayHostV3, allocate, true, false);
  NX_PROXY_BIND(proxy, NexoraGameplayHostV3, deallocate, true, false);
  const auto cached = proxy.Get();
  assert(cached.capabilities == host.capabilities);
  assert(cached.allocate(cached.context, 1, 64, 8) == memory.bytes.data());
  assert(gate.SetPhase(Phase::Capture));
  assert(!cached.allocate(cached.context, 1, 64, 8));
  cached.deallocate(cached.context, 1, memory.bytes.data(), 64, 8);
  assert(!gate.Valid() && memory.allocations == 1 && memory.frees == 0);
  assert(gate.SetPhase(Phase::Live));
  cached.deallocate(cached.context, 1, memory.bytes.data(), 64, 8);
  assert(memory.frees == 1);
}

void CachedAudioRouting() {
  runtime::GameplayAudio live(48000, 4);
  unsigned submissions{};
  live.SetOutput(
      [](void *context, const float *, std::uint32_t, std::uint32_t) {
        ++*static_cast<unsigned *>(context);
        return true;
      },
      &submissions);
  const std::array<float, 2> pcm{.125F, .25F};
  assert(live.RegisterPcm(1, 48000, 1, pcm));
  PlayServiceGate old_gate;
  PlayApiProxy old_proxy(old_gate, live.Api());
  NX_PROXY_BIND(old_proxy, NexoraGameplayAudioApiV1, play, true, false);
  NX_PROXY_BIND(old_proxy, NexoraGameplayAudioApiV1, stop_all, true, false);
  const auto cached_old = old_proxy.Get();
  const NexoraAudioPlayDesc desc{1, NEXORA_AUDIO_BUS_MUSIC, 1, 1};
  std::uint64_t first{}, added{};
  assert(cached_old.play(cached_old.context, &desc, &first) == 0);
  auto branch = live.ForkForReload();
  PlayServiceGate next_gate(Phase::Candidate);
  PlayApiProxy next_proxy(next_gate, branch->Api());
  NX_PROXY_BIND(next_proxy, NexoraGameplayAudioApiV1, play, true, false);
  NX_PROXY_BIND(next_proxy, NexoraGameplayAudioApiV1, is_playing, false, false);
  const auto cached_next = next_proxy.Get();
  assert(cached_next.play(cached_next.context, &desc, &added) == 0 && submissions == 0);
  assert(live.SwapReloadMixer(*branch) && old_proxy.Redirect(branch->Api()) &&
         next_proxy.Redirect(live.Api()) && old_gate.SetPhase(Phase::Retired) &&
         next_gate.SetPhase(Phase::Live));
  assert(cached_old.stop_all(cached_old.context) == 0);
  std::uint32_t playing{};
  assert(cached_next.is_playing(cached_next.context, first, &playing) == 0 && playing &&
         cached_next.is_playing(cached_next.context, added, &playing) == 0 && playing &&
         old_proxy.Get().context == cached_old.context &&
         next_proxy.Get().context == cached_next.context && live.Pump(1) && submissions == 1);
}
#undef NX_PROXY_BIND
} // namespace
int main() {
  Input();
  Storage();
  AllocationSignatures();
  CachedAudioRouting();
}
