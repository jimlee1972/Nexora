#include "Nexora/Editor/MeshThumbnail.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace nexora::editor {
namespace {
struct Point {
  double x{}, y{}, z{};
};
double Edge(const Point &a, const Point &b, double x, double y) {
  return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}
bool Bounded(const MeshGeometry &mesh) {
  return !mesh.vertices.empty() && mesh.vertices.size() <= kThumbnailMaximumVertices &&
         mesh.indices.size() >= 3 && mesh.indices.size() <= kThumbnailMaximumIndices &&
         mesh.indices.size() % 3 == 0;
}
} // namespace
MeshThumbnailBuild BuildMeshThumbnail(const MeshGeometry &mesh,
                                      const std::function<bool()> &cancelled) {
  const auto stopped = [&] { return cancelled && cancelled(); };
  if (stopped())
    return {{}, {}, true};
  if (!Bounded(mesh))
    return {{}, "Thumbnail requires 1..16384 vertices and 1..8192 indexed triangles.", false};
  std::vector<Point> points;
  points.reserve(mesh.vertices.size());
  double min_x = std::numeric_limits<double>::infinity(), min_y = min_x;
  double max_x = -min_x, max_y = -min_y;
  for (const auto &vertex : mesh.vertices) {
    if (stopped())
      return {{}, {}, true};
    const auto &p = vertex.position;
    if (!std::ranges::all_of(p, [](float value) { return std::isfinite(value); }))
      return {{}, "Thumbnail positions must be finite.", false};
    // Fixed isometric camera. Double intermediates admit every finite float position safely.
    points.push_back({(double(p[0]) - p[2]) * 0.7071067811865475,
                      (double(p[0]) + p[2]) * 0.408248290463863 - p[1] * 0.816496580927726,
                      (double(p[0]) + p[1] + p[2]) * 0.5773502691896258});
  }
  for (const auto index : mesh.indices) {
    if (stopped())
      return {{}, {}, true};
    if (index >= points.size())
      return {{}, "Thumbnail index is outside the vertex payload.", false};
    min_x = std::min(min_x, points[index].x);
    max_x = std::max(max_x, points[index].x);
    min_y = std::min(min_y, points[index].y);
    max_y = std::max(max_y, points[index].y);
  }
  const double extent = std::max(max_x - min_x, max_y - min_y);
  if (!(extent > 0))
    return {{}, "Thumbnail geometry has no projected extent.", false};
  constexpr int side = int(MeshThumbnailPixels::kSide);
  const double scale = (side - 4.) / extent;
  for (auto &p : points) {
    p.x = 2 + (extent - (max_x - min_x)) * scale * .5 + (p.x - min_x) * scale;
    p.y = 2 + (extent - (max_y - min_y)) * scale * .5 + (p.y - min_y) * scale;
  }
  auto image = std::make_unique<MeshThumbnailPixels>();
  for (std::size_t i = 0; i < image->rgba.size(); i += 4) {
    image->rgba[i] = 24;
    image->rgba[i + 1] = 27;
    image->rgba[i + 2] = 32;
    image->rgba[i + 3] = 255;
  }
  std::array<double, side * side> depth;
  depth.fill(std::numeric_limits<double>::infinity());
  bool visible{};
  for (std::size_t triangle = 0; triangle < mesh.indices.size(); triangle += 3) {
    if (stopped())
      return {{}, {}, true};
    const auto ia = mesh.indices[triangle], ib = mesh.indices[triangle + 1],
               ic = mesh.indices[triangle + 2];
    const auto &a = points[ia], &b = points[ib], &c = points[ic];
    const double area = Edge(a, b, c.x, c.y);
    if (area == 0)
      continue;
    const auto &pa = mesh.vertices[ia].position, &pb = mesh.vertices[ib].position,
               &pc = mesh.vertices[ic].position;
    const Point u{double(pb[0]) - pa[0], double(pb[1]) - pa[1], double(pb[2]) - pa[2]},
        v{double(pc[0]) - pa[0], double(pc[1]) - pa[1], double(pc[2]) - pa[2]};
    const Point n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
    const double length = std::hypot(n.x, n.y, n.z);
    if (!(length > 0))
      continue;
    const double light =
        .25 + .75 * std::clamp(std::abs(.35 * n.x + .75 * n.y + .55 * n.z) / length, 0., 1.);
    const int left = std::clamp(int(std::floor(std::min({a.x, b.x, c.x}))), 0, side - 1),
              right = std::clamp(int(std::ceil(std::max({a.x, b.x, c.x}))), 0, side - 1),
              top = std::clamp(int(std::floor(std::min({a.y, b.y, c.y}))), 0, side - 1),
              bottom = std::clamp(int(std::ceil(std::max({a.y, b.y, c.y}))), 0, side - 1);
    for (int y = top; y <= bottom; ++y) {
      if (stopped())
        return {{}, {}, true};
      for (int x = left; x <= right; ++x) {
        const double wa = Edge(b, c, x + .5, y + .5) / area, wb = Edge(c, a, x + .5, y + .5) / area,
                     wc = Edge(a, b, x + .5, y + .5) / area;
        if (wa < 0 || wb < 0 || wc < 0)
          continue;
        const double z = wa * a.z + wb * b.z + wc * c.z;
        const auto offset = std::size_t(y * side + x);
        if (z >= depth[offset])
          continue;
        depth[offset] = z;
        image->rgba[offset * 4] = std::uint8_t(180 * light);
        image->rgba[offset * 4 + 1] = std::uint8_t(200 * light);
        image->rgba[offset * 4 + 2] = std::uint8_t(230 * light);
        visible = true;
      }
    }
  }
  if (stopped())
    return {{}, {}, true};
  if (!visible)
    return {{}, "Thumbnail geometry has no rasterizable projected triangles.", false};
  return {std::move(image), {}, false};
}

struct MeshThumbnailCache::Implementation final {
  struct Operation {
    MeshThumbnailSnapshot snapshot;
    std::weak_ptr<const MeshGeometry> source;
    std::shared_ptr<const MeshGeometry> input;
    core::CancellationSource cancel;
    core::JobHandle job;
    MeshThumbnailBuild built;
  };
  core::JobSystem &jobs;
  const ContentBrowserModel &content;
  MeshThumbnailLimits limits;
  std::uint64_t generation{}, next{1};
  bool stopped{};
  std::vector<std::shared_ptr<Operation>> entries, pending;
  Implementation(core::JobSystem &pool, const ContentBrowserModel &model, MeshThumbnailLimits bound)
      : jobs(pool), content(model), limits(bound), generation(model.ProjectGeneration()) {
    entries.reserve(limits.entries);
    pending.reserve(limits.pending);
  }
  bool Current(const Operation &op) const {
    const auto *item = content.Find(op.snapshot.asset);
    const auto source = op.source.lock();
    return content.ProjectGeneration() == op.snapshot.project_generation && item && source &&
           item->artifact_hash == op.snapshot.artifact_revision && item->mesh == source &&
           !item->mesh.owner_before(source) && !source.owner_before(item->mesh);
  }
  auto Find(runtime::AssetUuid asset) {
    return std::ranges::find_if(entries,
                                [&](const auto &op) { return op->snapshot.asset == asset; });
  }
  void Synchronize() {
    if (generation != content.ProjectGeneration()) {
      for (auto &op : pending)
        op->cancel.Cancel();
      entries.clear();
      generation = content.ProjectGeneration();
    }
  }
};
MeshThumbnailCache::MeshThumbnailCache(core::JobSystem &jobs, const ContentBrowserModel &content,
                                       MeshThumbnailLimits limits) {
  if (!limits.entries || limits.entries > 64 || !limits.pending || limits.pending > 4)
    throw std::invalid_argument("Thumbnail limits require 1..64 entries and 1..4 pending jobs.");
  if (jobs.IsWorkerThread())
    throw std::invalid_argument("Thumbnail owner must not run on its JobSystem workers.");
  implementation_ = std::make_unique<Implementation>(jobs, content, limits);
}
MeshThumbnailCache::~MeshThumbnailCache() { Shutdown(); }
bool MeshThumbnailCache::Request(runtime::AssetUuid asset, std::string *error) {
  auto &impl = *implementation_;
  const auto fail = [&](const char *message) {
    if (error)
      *error = message;
    return false;
  };
  impl.Synchronize();
  const auto *item = impl.content.Find(asset);
  if (impl.stopped || !impl.generation || (!asset.high && !asset.low) || !item || !item->mesh ||
      item->artifact_hash.empty() || item->artifact_hash.size() > 128)
    return fail("Thumbnail requires a current imported mesh, bounded revision and live owner.");
  auto found = impl.Find(asset);
  if (found != impl.entries.end() && impl.Current(**found) &&
      (*found)->snapshot.phase != MeshThumbnailPhase::Stale) {
    std::rotate(found, found + 1, impl.entries.end());
    if (error)
      error->clear();
    return true;
  }
  if (!Bounded(*item->mesh))
    return fail("Thumbnail mesh exceeds the vertex/index limits or has invalid counts.");
  if (impl.pending.size() >= impl.limits.pending ||
      impl.next == std::numeric_limits<std::uint64_t>::max())
    return fail("Thumbnail pending-work budget or operation identifiers exhausted.");
  auto op = std::make_shared<Implementation::Operation>();
  op->snapshot = {impl.next,
                  impl.generation,
                  asset,
                  item->artifact_hash,
                  MeshThumbnailPhase::Queued,
                  "Mesh thumbnail queued.",
                  {}};
  op->source = item->mesh;
  op->input = std::make_shared<const MeshGeometry>(*item->mesh);
  try {
    op->job = impl.jobs.Submit({[op](const core::CancellationToken &token) {
                                  try {
                                    op->built = BuildMeshThumbnail(*op->input, [&] {
                                      return token.IsCancellationRequested();
                                    });
                                  } catch (const std::exception &exception) {
                                    op->built.error = std::string(exception.what()).substr(0, 256);
                                  } catch (...) {
                                    op->built.error = "Mesh thumbnail worker failed.";
                                  }
                                },
                                core::JobPriority::Low, op->cancel.Token(), "Mesh thumbnail"});
  } catch (...) {
    return fail("Mesh thumbnail could not submit its worker.");
  }
  ++impl.next;
  if (found != impl.entries.end()) {
    (*found)->cancel.Cancel();
    impl.entries.erase(found);
  }
  if (impl.entries.size() == impl.limits.entries) {
    impl.entries.front()->cancel.Cancel();
    impl.entries.erase(impl.entries.begin());
  }
  impl.pending.push_back(op);
  impl.entries.push_back(std::move(op));
  if (error)
    error->clear();
  return true;
}
std::size_t MeshThumbnailCache::Poll() {
  auto &impl = *implementation_;
  impl.Synchronize();
  std::size_t collected{};
  for (auto it = impl.pending.begin(); it != impl.pending.end();) {
    auto &op = *it;
    const auto status = op->job.Status();
    if (status == core::JobStatus::Queued || status == core::JobStatus::Running) {
      ++it;
      continue;
    }
    try {
      impl.jobs.Wait(op->job);
    } catch (...) {
      op->built.pixels.reset();
      op->built.error = "Mesh thumbnail job failed.";
    }
    const auto entry = impl.Find(op->snapshot.asset);
    if (entry != impl.entries.end() && *entry == op) {
      auto &snapshot = op->snapshot;
      if (op->cancel.Token().IsCancellationRequested() || status == core::JobStatus::Cancelled ||
          op->built.cancelled) {
        snapshot.phase = MeshThumbnailPhase::Cancelled;
        snapshot.message = "Mesh thumbnail cancelled.";
      } else if (!impl.Current(*op)) {
        snapshot.phase = MeshThumbnailPhase::Stale;
        snapshot.message = "Mesh thumbnail source or project changed.";
      } else if (op->built.pixels) {
        snapshot.pixels = std::move(op->built.pixels);
        snapshot.phase = MeshThumbnailPhase::Ready;
        snapshot.message.clear();
      } else {
        snapshot.phase = MeshThumbnailPhase::Failed;
        snapshot.message =
            op->built.error.empty() ? "Mesh thumbnail produced no pixels." : op->built.error;
      }
    }
    op->input.reset();
    op->built = {};
    op->job = {};
    it = impl.pending.erase(it);
    ++collected;
  }
  return collected;
}
std::optional<MeshThumbnailSnapshot> MeshThumbnailCache::Get(runtime::AssetUuid asset) {
  auto &impl = *implementation_;
  const auto found = impl.Find(asset);
  if (found == impl.entries.end())
    return std::nullopt;
  const auto op = *found;
  std::rotate(found, found + 1, impl.entries.end());
  auto snapshot = op->snapshot;
  if (!impl.Current(*op)) {
    snapshot.phase = MeshThumbnailPhase::Stale;
    snapshot.pixels.reset();
    snapshot.message = "Mesh thumbnail source or project changed.";
  } else if (snapshot.phase == MeshThumbnailPhase::Queued) {
    const auto status = op->job.Status();
    if (status == core::JobStatus::Running)
      snapshot.phase = MeshThumbnailPhase::Rendering;
    else if (status != core::JobStatus::Queued)
      snapshot.phase = MeshThumbnailPhase::AwaitingPublish;
  }
  if (snapshot.phase == MeshThumbnailPhase::Cancelled && snapshot.message.empty())
    snapshot.message = "Mesh thumbnail cancelled.";
  return snapshot;
}
bool MeshThumbnailCache::Cancel(runtime::AssetUuid asset) noexcept {
  auto &impl = *implementation_;
  const auto found = impl.Find(asset);
  if (found == impl.entries.end() || !(*found)->job.IsValid() ||
      (*found)->cancel.Token().IsCancellationRequested())
    return false;
  (*found)->cancel.Cancel();
  (*found)->snapshot.phase = MeshThumbnailPhase::Cancelled;
  (*found)->snapshot.message.clear();
  return true;
}
bool MeshThumbnailCache::Invalidate(runtime::AssetUuid asset) noexcept {
  auto &impl = *implementation_;
  const auto found = impl.Find(asset);
  if (found == impl.entries.end())
    return false;
  (*found)->cancel.Cancel();
  impl.entries.erase(found);
  return true;
}
void MeshThumbnailCache::Clear() noexcept {
  auto &impl = *implementation_;
  for (auto &op : impl.pending)
    op->cancel.Cancel();
  impl.entries.clear();
  impl.generation = impl.content.ProjectGeneration();
}
void MeshThumbnailCache::Shutdown() noexcept {
  auto &impl = *implementation_;
  if (impl.stopped)
    return;
  impl.stopped = true;
  Clear();
  for (const auto &op : impl.pending) {
    try {
      impl.jobs.Wait(op->job);
    } catch (...) {
    }
  }
  impl.pending.clear();
}
std::size_t MeshThumbnailCache::Size() const noexcept { return implementation_->entries.size(); }
std::size_t MeshThumbnailCache::Pending() const noexcept { return implementation_->pending.size(); }
} // namespace nexora::editor
