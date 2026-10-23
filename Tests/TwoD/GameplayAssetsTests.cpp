#include "Nexora/Core/ProcessMemory.h"
#include "Nexora/Runtime/GameplayServices.h"
#include "Nexora/TwoD/GameplayAssets.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>
using namespace nexora;
namespace {
twod::Bytes Cook(twod::AssetId id, bool large = false) {
  twod::Atlas a{id, large ? 2048U : 8U, large ? 2048U : 8U, 1, true, {{1, 1, 1, 2, 2}}, {}};
  a.rgba.resize(std::size_t(a.width) * a.height * 4, std::byte{255});
  runtime::DerivedDataCache cache;
  const auto cooked = runtime::AssetCooker{}.Cook({id, "sprite_atlas", {}, twod::EncodeAtlas(a)},
                                                  "test", "", cache);
  assert(cooked);
  return runtime::AssetCooker::Serialize(*cooked);
}
NexoraGameplayAssetsApiV1 Api(twod::GameplayAssets &assets) {
  NexoraGameplayAssetsApiV1 api{};
  api.struct_size = sizeof(api);
  api.abi_version = 1;
  assert(assets.GetApi(&api) == 0);
  return api;
}
NexoraAssetLoadStateV1 Wait(twod::GameplayAssets &assets, NexoraGameplayAssetsApiV1 api,
                            std::uint64_t ticket) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  NexoraAssetLoadStateV1 state{};
  state.struct_size = sizeof(state);
  do {
    assets.Tick();
    assert(api.poll(api.context, ticket, &state) == 0);
    if (state.phase >= NEXORA_ASSET_READY)
      return state;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < deadline);
  assert(false && "bounded asset request deadline");
  return state;
}
NexoraAssetUsageV1 Usage(NexoraGameplayAssetsApiV1 api) {
  NexoraAssetUsageV1 u{};
  u.struct_size = sizeof(u);
  assert(api.get_usage(api.context, &u) == 0);
  return u;
}
void Prefixes(NexoraGameplayAssetsApiV1 assets) {
  runtime::GameplayServices host;
  host.assets = assets;
  constexpr auto sp01 = offsetof(NexoraGameplayServicesApiV1, get_ui_draw_api);
  constexpr auto sp02 = offsetof(NexoraGameplayServicesApiV1, get_assets_api);
  alignas(NexoraGameplayServicesApiV1)
      std::array<std::byte, sizeof(NexoraGameplayServicesApiV1) + 16>
          storage;
  for (auto prefix : {std::size_t(NEXORA_GAMEPLAY_SERVICES_V1_BASE_SIZE), sp01, sp02,
                      sizeof(NexoraGameplayServicesApiV1)}) {
    storage.fill(std::byte{0x5a});
    const std::uint32_t header[]{static_cast<std::uint32_t>(prefix), 1};
    std::memcpy(storage.data(), header, sizeof(header));
    auto *api = reinterpret_cast<NexoraGameplayServicesApiV1 *>(storage.data());
    assert(host.GetApi(api) == 0 && api->struct_size == prefix);
    for (auto i = prefix; i < storage.size(); ++i)
      assert(storage[i] == std::byte{0x5a});
  }
  NexoraGameplayServicesApiV1 services{};
  services.struct_size = sizeof(services);
  services.abi_version = 1;
  assert(host.GetApi(&services) == 0);
  auto out = assets;
  assert(services.get_assets_api(services.context, &out) == 0 && out.context == assets.context);
}
void Portable() {
  const auto cooked = Cook({11, 22});
  auto corrupted = cooked;
  corrupted.back() ^= std::byte{1};
  std::array<twod::Bytes, 3> data{cooked, corrupted, twod::Bytes(13, std::byte{77})};
  std::vector<twod::GameplayAssetFile> files{
      {1, data[0].size(), "0"}, {1, data[1].size(), "1"}, {3, data[2].size(), "2"}};
  twod::GameplayContent content;
  const auto thread = std::this_thread::get_id();
  twod::GameplayAssets assets(content, files, [&](const auto &f) {
    assert(std::this_thread::get_id() != thread);
    return data[static_cast<unsigned>(f.path[0] - '0')];
  });
  auto api = Api(assets);
  Prefixes(api);
  std::uint64_t ticket = 999;
  assert(api.request(api.context, 99, &ticket) == -2 && ticket == 999);
  assert(api.request(api.context, 0, &ticket) == 0);
  auto state = Wait(assets, api, ticket);
  assert(state.phase == NEXORA_ASSET_READY && state.high == 11 && state.low == 22);
  auto retained = content.ResolveAtlas({11, 22});
  assert(retained && retained->atlas->rgba[0] == std::byte{255});
  assert(api.release_request(api.context, ticket) == 0);
  assert(api.poll(api.context, ticket, &state) == -2);
  assert(api.remove_asset(api.context, 11, 22) == 0);
  assert(!content.ResolveAtlas({11, 22}) && content.CollectRetiredTextures().empty());
  assert(Usage(api).retired_bytes > 0);
  retained = {};
  assert(content.CollectRetiredTextures().size() == 1);
  assert(Usage(api).active_bytes == 0 && Usage(api).retired_bytes == 0);
  assert(api.request(api.context, 1, &ticket) == 0);
  state = Wait(assets, api, ticket);
  assert(state.phase == NEXORA_ASSET_FAILED && state.result == -1);
  assert(api.release_request(api.context, ticket) == 0 && Usage(api).staging_bytes == 0);
  assert(api.request(api.context, 2, &ticket) == 0);
  assert(Wait(assets, api, ticket).phase == NEXORA_ASSET_READY);
  std::uint64_t length = 99;
  std::array<std::byte, 13> buffer{};
  assert(api.copy_bytes(api.context, ticket, nullptr, 0, &length) == 0 && length == 13);
  assert(api.copy_bytes(api.context, ticket, buffer.data(), 12, &length) == -2 &&
         buffer[0] == std::byte{0});
  assert(api.copy_bytes(api.context, ticket, buffer.data(), 13, &length) == 0 &&
         buffer[12] == std::byte{77});
  int wrong{};
  std::thread worker([&] { wrong = api.release_request(api.context, ticket); });
  worker.join();
  assert(wrong == -3 && api.release_request(api.context, ticket) == 0);
  assert(Usage(api).staging_bytes == 0 && Usage(api).requests == 0);
  // An abandoned in-flight read must drain before its reservation can be reused.
  std::promise<void> release;
  const auto ready = release.get_future().share();
  twod::GameplayAssets blocked(
      content, {{3, 13, "raw"}},
      [ready](const auto &) {
        ready.wait();
        return twod::Bytes(13, std::byte{1});
      },
      13);
  auto blockedApi = Api(blocked);
  assert(blockedApi.request(blockedApi.context, 0, &ticket) == 0);
  blocked.Tick();
  assert(blockedApi.release_request(blockedApi.context, ticket) == 0);
  assert(Usage(blockedApi).staging_bytes == 13);
  assert(blockedApi.request(blockedApi.context, 0, &ticket) == -2);
  release.set_value();
  for (unsigned i = 0; i < 1000 && Usage(blockedApi).staging_bytes; ++i) {
    blocked.Tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(Usage(blockedApi).staging_bytes == 0);
}
void LiveBudget() {
  twod::GameplayContent content({2, 1000, 1000});
  NexoraGameplayContentApiV1 api{};
  api.struct_size = sizeof(api);
  assert(content.GetApi(&api) == 0);
  const auto blob = runtime::AssetCooker::Deserialize(Cook({4, 5}));
  assert(blob);
  auto atlas = twod::DecodeAtlas(blob->payload);
  assert(atlas);
  std::vector<std::shared_ptr<const twod::Atlas>> held;
  std::uint64_t high{}, low{}, last{};
  for (unsigned generation = 0; generation < 3; ++generation) {
    atlas->rgba[0] = std::byte(generation);
    const auto bytes = twod::EncodeAtlas(*atlas);
    assert(api.register_atlas(api.context, bytes.data(), bytes.size(), &high, &low) == 0);
    const auto resolved = content.ResolveAtlas({4, 5});
    assert(resolved && resolved->texture != last);
    last = resolved->texture;
    held.push_back(resolved->atlas);
  }
  atlas->rgba[0] = std::byte{99};
  const auto fourth = twod::EncodeAtlas(*atlas);
  high = 123;
  low = 456;
  assert(api.register_atlas(api.context, fourth.data(), fourth.size(), &high, &low) == -2 &&
         high == 123 && low == 456 && content.ResolveAtlas({4, 5})->texture == last);
  assert(content.Remove({4, 5}) == 0 && content.CollectRetiredTextures().empty());
  assert(api.register_atlas(api.context, fourth.data(), fourth.size(), &high, &low) == -2);
  held.clear();
  assert(content.CollectRetiredTextures().size() == 3);
  assert(api.register_atlas(api.context, fourth.data(), fourth.size(), &high, &low) == 0);
}

void Stress() {
  constexpr std::uint64_t MiB = 1024 * 1024;
  std::array<twod::Bytes, 5> data;
  std::vector<twod::GameplayAssetFile> files;
  for (unsigned i = 0; i < data.size(); ++i) {
    data[i] = Cook({88, i + 1}, true);
    files.push_back({1, data[i].size(), std::to_string(i)});
  }
  twod::GameplayContent content({256, 96 * MiB, 128 * MiB});
  twod::GameplayAssets assets(content, files,
                              [&](const auto &f) { return data[std::stoul(f.path)]; });
  auto api = Api(assets);
  std::array<std::uint64_t, 8> rss{};
  for (unsigned round = 0; round < 8; ++round) {
    for (unsigned i = 0; i < data.size(); ++i) {
      std::uint64_t ticket{};
      assert(api.request(api.context, i, &ticket) == 0);
      assert(Wait(assets, api, ticket).phase == NEXORA_ASSET_READY);
      assert(api.release_request(api.context, ticket) == 0);
    }
    const auto u = Usage(api);
    assert(u.active_assets == 5 && u.active_bytes > 80 * MiB && u.active_bytes < 81 * MiB);
    auto held = content.ResolveAtlas({88, 1});
    for (unsigned i = 0; i < data.size(); ++i)
      assert(api.remove_asset(api.context, 88, i + 1) == 0);
    assert(content.CollectRetiredTextures().size() == 4);
    assert(held && held->atlas->rgba.size() == 16 * MiB && Usage(api).retired_bytes > 16 * MiB);
    held = {};
    assert(content.CollectRetiredTextures().size() == 1);
    const auto empty = Usage(api);
    assert(empty.active_assets == 0 && empty.retired_generations == 0 && empty.active_bytes == 0 &&
           empty.retired_bytes == 0 && empty.staging_bytes == 0);
    assert(empty.peak_content_bytes < 81 * MiB);
    const auto resident = core::CurrentProcessResidentBytes();
    rss[round] = resident.value_or(0);
    std::cout << "SP-03 round=" << round << " active=" << empty.active_bytes
              << " retired=" << empty.retired_bytes << " staging=" << empty.staging_bytes
              << " rss_bytes=" << (resident ? std::to_string(*resident) : "unavailable") << '\n';
  }
  if (std::all_of(rss.begin(), rss.end(), [](auto value) { return value != 0; })) {
    const auto warm = *std::max_element(rss.begin(), rss.begin() + 4);
    const auto tail = *std::max_element(rss.begin() + 4, rss.end());
    assert(tail <= warm + 32 * MiB && "resident-set growth after warm-up exceeds 32 MiB");
  }
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 3 && std::string_view(argv[1]) == "--fixture") {
    const auto root = std::filesystem::path(argv[2]);
    std::filesystem::create_directories(root);
    const auto write = [&](const std::string &name, const twod::Bytes &bytes) {
      std::ofstream file(root / name, std::ios::binary);
      file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
      assert(file);
    };
    for (unsigned i = 0; i < 5; ++i) {
      write("atlas" + std::to_string(i) + ".nxcook", Cook({88, i + 1}, true));
      const twod::Clip clip{{89, i + 1}, {88, i + 1}, true, {{1, 0.1, "step"}, {1, 0.1, "step"}}};
      runtime::DerivedDataCache cache;
      const auto blob = runtime::AssetCooker{}.Cook(
          {clip.id, "sprite_clip", {clip.atlas}, twod::EncodeClip(clip)}, "test", "", cache);
      assert(blob);
      write("clip" + std::to_string(i) + ".nxcook", runtime::AssetCooker::Serialize(*blob));
    }
    write("raw.bin", {std::byte{'S'}, std::byte{'P'}, std::byte{'3'}});
  } else if (argc > 1)
    Stress();
  else {
    Portable();
    LiveBudget();
  }
}
