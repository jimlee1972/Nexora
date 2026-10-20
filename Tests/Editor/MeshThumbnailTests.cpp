#include "Nexora/Editor/MeshThumbnail.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using editor::MeshThumbnailPhase;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::shared_ptr<const editor::MeshGeometry> Mesh(bool square = false) {
  auto result =
      editor::ImportObjMesh(square ? "v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\nf 1 2 3\nf 1 3 4\n"
                                   : "v -1 -1 0\nv 1 -1 0\nv 0 1 0\nf 1 2 3\n");
  Require(result.geometry.has_value(), "OBJ fixture import");
  return std::make_shared<const editor::MeshGeometry>(std::move(*result.geometry));
}
std::array<std::uint8_t, 4> Pixel(const editor::MeshThumbnailPixels &image, int x, int y) {
  const auto offset = std::size_t(y * 64 + x) * 4;
  return {image.rgba[offset], image.rgba[offset + 1], image.rgba[offset + 2],
          image.rgba[offset + 3]};
}
void Wait(const std::function<bool()> &condition) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!condition()) {
    Require(std::chrono::steady_clock::now() < deadline, "thumbnail test did not make progress");
    std::this_thread::yield();
  }
}
void Drain(editor::MeshThumbnailCache &cache) {
  Wait([&] {
    cache.Poll();
    return cache.Pending() == 0;
  });
}
struct Gate {
  core::JobSystem &jobs;
  std::promise<void> release;
  core::JobHandle handle;
  bool opened{};
  void Open() {
    if (!opened) {
      release.set_value();
      opened = true;
    }
  }
  explicit Gate(core::JobSystem &pool) : jobs(pool) {
    auto entered = std::make_shared<std::promise<void>>();
    auto ready = entered->get_future();
    auto signal = release.get_future().share();
    handle = jobs.Submit({[entered, signal](const auto &) {
                            entered->set_value();
                            signal.wait();
                          },
                          core::JobPriority::Normal,
                          {},
                          "Thumbnail test gate"});
    Require(ready.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
            "pool gate startup");
  }
  ~Gate() {
    Open();
    jobs.Wait(handle);
  }
};
std::vector<editor::ContentItem> Items() {
  std::vector<editor::ContentItem> items;
  for (std::uint64_t i = 1; i <= 5; ++i)
    items.push_back({{1, i},
                     "Content/Mesh" + std::to_string(i) + ".obj",
                     ".obj",
                     "revision-1",
                     editor::ThumbnailState::Ready,
                     Mesh()});
  return items;
}
void Raster(const char *output) {
  auto mesh = *Mesh();
  auto first = editor::BuildMeshThumbnail(mesh);
  Require(first.pixels && first.error.empty() && !first.cancelled, "real mesh thumbnail");
  Require(Pixel(*first.pixels, 0, 0) == std::array<std::uint8_t, 4>{24, 27, 32, 255}, "background");
  const auto center = Pixel(*first.pixels, 32, 32);
  // The z-normal face has diffuse .25 + .75*.55, independent of imported normals.
  Require(center == std::array<std::uint8_t, 4>{119, 132, 152, 255}, "neutral face shading");
  std::size_t colored{};
  for (std::size_t i = 0; i < first.pixels->rgba.size(); i += 4) {
    Require(first.pixels->rgba[i + 3] == 255, "nonopaque preview pixel");
    colored += first.pixels->rgba[i] != 24;
  }
  Require(colored > 500 && colored < 1500, "rasterized triangle coverage");
  const auto retained = first.pixels->rgba;
  std::swap(mesh.indices[0], mesh.indices[2]);
  auto reversed = editor::BuildMeshThumbnail(mesh);
  Require(reversed.pixels && reversed.pixels->rgba == retained,
          "winding changed two-sided preview");
  mesh.minimum.fill(std::numeric_limits<float>::quiet_NaN());
  mesh.maximum.fill(-1000.0f);
  auto bounds = editor::BuildMeshThumbnail(mesh);
  Require(bounds.pixels && bounds.pixels->rgba == retained, "cached bounds changed actual framing");
  auto square = editor::BuildMeshThumbnail(*Mesh(true));
  Require(square.pixels && square.pixels->rgba != retained, "different geometry produced an icon");

  auto front = *Mesh();
  for (auto &v : front.vertices)
    for (auto &p : v.position)
      p -= 10;
  for (auto &p : front.vertices[1].position)
    p -= 1;
  auto nearer = editor::BuildMeshThumbnail(front);
  Require(nearer.pixels && Pixel(*nearer.pixels, 32, 32) != center, "occlusion fixture shading");
  auto layered = *Mesh();
  layered.vertices.insert(layered.vertices.end(), front.vertices.begin(), front.vertices.end());
  layered.indices.insert(layered.indices.end(), {3, 4, 5});
  auto depth = editor::BuildMeshThumbnail(layered);
  Require(depth.pixels && Pixel(*depth.pixels, 32, 32) == Pixel(*nearer.pixels, 32, 32),
          "near face depth");
  std::rotate(layered.indices.begin(), layered.indices.begin() + 3, layered.indices.end());
  depth = editor::BuildMeshThumbnail(layered);
  Require(depth.pixels && Pixel(*depth.pixels, 32, 32) == Pixel(*nearer.pixels, 32, 32),
          "submission order overrode depth");

  mesh = *Mesh();
  mesh.indices[2] = 60000;
  Require(!editor::BuildMeshThumbnail(mesh).pixels, "out-of-range index");
  mesh = *Mesh();
  mesh.vertices[0].position[0] = std::numeric_limits<float>::infinity();
  Require(!editor::BuildMeshThumbnail(mesh).pixels, "infinite position");
  mesh.vertices[0].position[0] = std::numeric_limits<float>::quiet_NaN();
  Require(!editor::BuildMeshThumbnail(mesh).pixels, "NaN position");
  mesh = *Mesh();
  mesh.indices.pop_back();
  Require(!editor::BuildMeshThumbnail(mesh).pixels, "incomplete triangle");
  mesh = *Mesh();
  for (auto &v : mesh.vertices)
    v.position = {};
  Require(!editor::BuildMeshThumbnail(mesh).pixels, "degenerate geometry");
  Require(!editor::BuildMeshThumbnail({}).pixels, "empty geometry");
  Require(editor::BuildMeshThumbnail({}, [] { return true; }).cancelled, "pre-cancelled builder");
  for (const auto scale : {1e-30f, std::numeric_limits<float>::max() / 4}) {
    mesh = *Mesh();
    for (auto &v : mesh.vertices)
      for (auto &p : v.position)
        p *= scale;
    auto extreme = editor::BuildMeshThumbnail(mesh);
    Require(extreme.pixels && Pixel(*extreme.pixels, 32, 32) == center, "finite position range");
  }
  mesh = *Mesh();
  mesh.vertices.resize(editor::kThumbnailMaximumVertices);
  mesh.indices.resize(editor::kThumbnailMaximumIndices);
  std::fill(mesh.indices.begin(), mesh.indices.end(), std::uint16_t{0});
  // Put the visible triangle last: every admitted triangle must be visited.
  mesh.indices[mesh.indices.size() - 2] = 1;
  mesh.indices.back() = 2;
  auto maximum = editor::BuildMeshThumbnail(mesh);
  Require(maximum.pixels && Pixel(*maximum.pixels, 32, 32) == center, "exact payload limits");
  std::size_t checks{};
  auto cancelled = editor::BuildMeshThumbnail(mesh, [&] {
    return ++checks > editor::kThumbnailMaximumVertices + editor::kThumbnailMaximumIndices +
                          editor::kThumbnailMaximumIndices / 3 + 5;
  });
  Require(cancelled.cancelled && !cancelled.pixels && cancelled.error.empty(),
          "scanline cancellation");
  mesh.vertices.emplace_back();
  Require(!editor::BuildMeshThumbnail(mesh).pixels, "vertex limit plus one");
  mesh.vertices.pop_back();
  mesh.indices.insert(mesh.indices.end(), {0, 1, 2});
  Require(!editor::BuildMeshThumbnail(mesh).pixels, "triangle limit plus one");
  if (output) {
    std::ofstream file(output, std::ios::binary);
    file << "P6\n64 64\n255\n";
    for (std::size_t i = 0; i < retained.size(); i += 4)
      file.write(reinterpret_cast<const char *>(retained.data() + i), 3);
    Require(bool(file), "thumbnail evidence write");
  }
}
void Jobs() {
  core::JobSystem jobs(1);
  jobs.Start();
  editor::ContentBrowserModel content;
  auto items = Items();
  Require(content.Reset(items, 7), "content setup");
  editor::MeshThumbnailCache cache(jobs, content, {2, 2});
  std::string error;
  Require(cache.Request({1, 1}, &error) && error.empty(), "thumbnail request");
  const auto operation = cache.Get({1, 1})->operation;
  Require(cache.Request({1, 1}) && cache.Pending() == 1 &&
              cache.Get({1, 1})->operation == operation,
          "duplicate worker");
  Wait([&] { return cache.Get({1, 1})->phase == MeshThumbnailPhase::AwaitingPublish; });
  Require(!cache.Get({1, 1})->pixels, "worker published without owner validation");
  Require(cache.Cancel({1, 1}) && !cache.Cancel({1, 1}), "late cancellation");
  Drain(cache);
  Require(cache.Get({1, 1})->phase == MeshThumbnailPhase::Cancelled && !cache.Get({1, 1})->pixels,
          "cancelled completion published");
  Require(cache.Request({1, 1}) && cache.Pending() == 0 &&
              cache.Get({1, 1})->operation == operation,
          "cancelled thumbnail automatically retried");
  Require(cache.Invalidate({1, 1}) && cache.Request({1, 1}), "explicit retry");
  Drain(cache);
  auto retained = cache.Get({1, 1})->pixels;
  Require(retained && cache.Get({1, 1})->phase == MeshThumbnailPhase::Ready, "ready publication");
  Require(cache.Request({1, 2}), "second cache entry");
  Drain(cache);
  Require(cache.Get({1, 1}).has_value() && cache.Request({1, 3}), "LRU touch/request");
  Drain(cache);
  Require(cache.Size() == 2 && !cache.Get({1, 2}) && cache.Get({1, 1})->pixels == retained,
          "LRU evicted a touched entry");
  Require(!cache.Request({9, 9}, &error) && !error.empty() && cache.Size() == 2,
          "missing asset evicted current data");
  const auto before_rename = cache.Get({1, 1})->operation;
  Require(content.Rename({1, 1}, "Renamed.obj") && cache.Request({1, 1}) && cache.Pending() == 0 &&
              cache.Get({1, 1})->operation == before_rename,
          "path rename invalidated stable asset identity");
  cache.Clear();
  {
    Gate gate(jobs);
    Require(cache.Request({1, 1}), "queued original revision");
    const auto old = cache.Get({1, 1})->operation;
    Require(content.PublishArtifact({1, 1}, "revision-2", editor::ThumbnailState::Ready, &error,
                                    Mesh(true)),
            "reimport");
    Require(cache.Get({1, 1})->phase == MeshThumbnailPhase::Stale && !cache.Get({1, 1})->pixels,
            "stale revision exposed");
    Require(cache.Request({1, 1}) && cache.Get({1, 1})->operation > old && cache.Pending() == 2,
            "replacement request");
    const auto replacement = cache.Get({1, 1})->operation;
    Require(!cache.Request({1, 4}, &error) && !error.empty() &&
                cache.Get({1, 1})->operation == replacement && cache.Size() == 1,
            "pending budget mutated cache");
  }
  Drain(cache);
  Require(cache.Get({1, 1})->phase == MeshThumbnailPhase::Ready &&
              cache.Get({1, 1})->pixels->rgba != retained->rgba,
          "old revision overwrote replacement");
  cache.Clear();
  {
    Gate gate(jobs);
    Require(cache.Request({1, 4}), "generation request");
    Require(content.Reset(items, 8), "project generation replacement");
    Require(cache.Get({1, 4})->phase == MeshThumbnailPhase::Stale, "stale project snapshot");
    cache.Poll();
    Require(cache.Size() == 0 && cache.Pending() == 1 && cache.Request({1, 4}),
            "generation clear lost cancellation budget");
  }
  Drain(cache);
  Require(cache.Get({1, 4})->project_generation == 8 && cache.Get({1, 4})->pixels,
          "new project completion lost");
  cache.Clear();
  {
    Gate gate(jobs);
    Require(cache.Request({1, 4}), "same-generation request");
    auto old = cache.Get({1, 4})->operation;
    cache.Clear();
    Require(content.Reset(items, 8) && cache.Request({1, 4}) && cache.Get({1, 4})->operation > old,
            "reopen operation ABA");
  }
  Drain(cache);
  Require(cache.Get({1, 4})->pixels != nullptr, "same-generation reopen failed");
  cache.Clear();
  {
    Gate gate(jobs);
    Require(cache.Request({1, 5}), "removed source request");
    const std::array removed{runtime::AssetUuid{1, 5}};
    Require(content.Delete(removed), "remove pending source");
    items.clear();
  }
  Drain(cache);
  Require(cache.Get({1, 5})->phase == MeshThumbnailPhase::Stale && !cache.Get({1, 5})->pixels,
          "removed source published");
  const auto stale = cache.Get({1, 5})->operation;
  Require(content.Undo() && cache.Request({1, 5}) && cache.Get({1, 5})->operation > stale,
          "restoring a stale source did not schedule a fresh preview");
  Drain(cache);
  Require(cache.Get({1, 5})->pixels != nullptr, "restored source remained stale");
  items = Items();
  auto broken = std::make_shared<editor::MeshGeometry>(*Mesh());
  broken->vertices[0].position[0] = std::numeric_limits<float>::quiet_NaN();
  items[0].mesh = broken;
  Require(content.Reset(items, 9) && cache.Request({1, 1}), "broken mesh request");
  Drain(cache);
  const auto failed = cache.Get({1, 1});
  Require(failed->phase == MeshThumbnailPhase::Failed && !failed->pixels &&
              !failed->message.empty(),
          "visible raster failure");
  Require(cache.Request({1, 1}) && cache.Pending() == 0 &&
              cache.Get({1, 1})->operation == failed->operation,
          "failed thumbnail automatically retried");
  Require(content.PublishArtifact({1, 1}, "revision-1", editor::ThumbnailState::Ready, nullptr,
                                  Mesh()) &&
              cache.Request({1, 1}),
          "same-hash new geometry owner");
  Drain(cache);
  Require(cache.Get({1, 1})->pixels != nullptr, "geometry owner replacement not refreshed");
  const auto before_hash = cache.Get({1, 1})->operation;
  Require(content.PublishArtifact({1, 1}, "hash-only-change", editor::ThumbnailState::Ready,
                                  nullptr, content.Find({1, 1})->mesh) &&
              cache.Get({1, 1})->phase == MeshThumbnailPhase::Stale && !cache.Get({1, 1})->pixels &&
              cache.Request({1, 1}) && cache.Get({1, 1})->operation > before_hash,
          "artifact-only revision change reused old pixels");
  Drain(cache);
  Require(cache.Get({1, 1})->pixels != nullptr, "artifact-only revision refresh");
  cache.Clear();
  {
    Gate gate(jobs);
    Require(cache.Request({1, 2}) && cache.Cancel({1, 2}) && cache.Pending() == 1,
            "queued shutdown setup");
    gate.Open();
    cache.Shutdown();
    Require(cache.Pending() == 0 && cache.Size() == 0, "shutdown did not collect owned work");
  }
  cache.Shutdown();
  Require(!cache.Request({1, 1}) && cache.Size() == 0 && cache.Pending() == 0 &&
              Pixel(*retained, 32, 32)[0] == 119,
          "shutdown or retained pixels");
  for (const auto limits : {editor::MeshThumbnailLimits{0, 1}, {1, 0}, {65, 1}, {1, 5}}) {
    bool rejected{};
    try {
      editor::MeshThumbnailCache invalid(jobs, content, limits);
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    Require(rejected, "invalid thumbnail budget");
  }
  {
    editor::ContentBrowserModel many;
    std::vector<editor::ContentItem> sources;
    const auto geometry = Mesh();
    for (std::uint64_t id = 1; id <= 65; ++id)
      sources.push_back({{2, id},
                         "Content/M" + std::to_string(id) + ".obj",
                         ".obj",
                         "v1",
                         editor::ThumbnailState::Ready,
                         geometry});
    Require(many.Reset(sources, 1), "maximum cache source setup");
    editor::MeshThumbnailCache bounded(jobs, many, {64, 4});
    {
      Gate gate(jobs);
      for (std::uint64_t id = 1; id <= 4; ++id)
        Require(bounded.Request({2, id}), "maximum pending request");
      Require(!bounded.Request({2, 5}) && bounded.Pending() == 4 && bounded.Size() == 4,
              "pending hard cap plus one");
    }
    Drain(bounded);
    for (std::uint64_t id = 5; id <= 65; ++id) {
      Require(bounded.Request({2, id}), "maximum cache fill");
      Drain(bounded);
    }
    Require(bounded.Size() == 64 && !bounded.Get({2, 1}) && bounded.Get({2, 65})->pixels,
            "entry hard cap plus one");
  }
  jobs.Stop();
  editor::MeshThumbnailCache stopped(jobs, content);
  Require(!stopped.Request({1, 1}, &error) && !error.empty() && stopped.Size() == 0 &&
              stopped.Pending() == 0,
          "failed submission published partial state");
}
} // namespace
int main(int argc, char **argv) {
  try {
    Raster(argc == 2 ? argv[1] : nullptr);
    Jobs();
    std::cout << "Mesh thumbnail pixels, ownership, cancellation, revision/project guards and "
                 "bounds passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
