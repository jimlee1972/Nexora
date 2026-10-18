#include "Nexora/TwoD/Assets.h"
#include "Nexora/TwoD/Lifecycle.h"
#include "Nexora/TwoD/Queries.h"
#include "Nexora/TwoD/Scene.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <tuple>
namespace {
using namespace nexora;
using namespace twod;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Near(double a, double b, const char *message) { Require(std::abs(a - b) < 1e-5, message); }
Image Solid(std::uint32_t id, std::uint32_t w, std::uint32_t h,
            std::array<unsigned char, 4> color) {
  Image image{id, w, h, {}};
  image.rgba.resize(std::size_t(w) * h * 4);
  for (std::size_t i = 0; i < image.rgba.size(); ++i)
    image.rgba[i] = std::byte(color[i % 4]);
  return image;
}
std::shared_ptr<Atlas> TestAtlas() {
  return std::make_shared<Atlas>(*PackAtlas(
      {1, 1},
      std::vector<Image>{Solid(1, 2, 2, {255, 0, 0, 255}), Solid(2, 1, 1, {0, 255, 0, 128})}, 8,
      16));
}
void Assets() {
  std::vector<Image> images{Solid(2, 1, 1, {0, 255, 0, 128}), Solid(1, 2, 2, {255, 0, 0, 255})};
  auto a = PackAtlas({1, 1}, images, 8, 16);
  Require(a.has_value(), "atlas pack");
  std::reverse(images.begin(), images.end());
  auto b = PackAtlas({1, 1}, images, 8, 16);
  Require(b && a->regions == b->regions && a->rgba == b->rgba && EncodeAtlas(*a) == EncodeAtlas(*b),
          "noncanonical packing");
  Require(a->regions[0] == Region{1, 1, 1, 2, 2}, "packing layout");
  Require(a->rgba[0] == std::byte{255} && a->rgba[3] == std::byte{255}, "extruded corner");
  auto bytes = EncodeAtlas(*a);
  Require(DecodeAtlas(bytes).has_value(), "atlas round trip");
  for (std::size_t n = 0; n < bytes.size(); ++n)
    Require(!DecodeAtlas(std::span(bytes).first(n)), "truncated atlas");
  bytes.push_back(std::byte{0});
  Require(!DecodeAtlas(bytes), "atlas trailing bytes");
  images[1].id = images[0].id;
  Require(!PackAtlas({1, 1}, images, 8, 16), "duplicate regions");
  auto bad = *a;
  bad.regions[1].x = bad.regions[0].x;
  Require(!ValidateAtlas(bad), "overlap admitted");
  runtime::ImporterRegistry registry;
  Require(RegisterImporters(registry), "import registration");
  auto canonical = registry.Import({a->id, "sprite_atlas", "fixture.nx2atlas", EncodeAtlas(*a)});
  Require(canonical.has_value(), "atlas import");
  runtime::DerivedDataCache cache;
  runtime::AssetCooker cooker;
  auto cooked = cooker.Cook(*canonical, "linux", "2d-v1", cache);
  auto repeated = cooker.Cook(*canonical, "linux", "2d-v1", cache);
  Require(cooked && repeated &&
              runtime::AssetCooker::Serialize(*cooked) ==
                  runtime::AssetCooker::Serialize(*repeated),
          "cook reproducibility");
  auto bundle =
      runtime::BundleBuilder::Build("atlas", 1, {}, std::vector<runtime::RuntimeBlob>{*cooked});
  Require(bundle && runtime::BundleBuilder::Verify(*bundle), "bundle");
  runtime::AssetGenerationStore store;
  Require(store.Stage({*bundle}) && store.ActivateStaged(), "asset publication");
  const auto *loaded = store.Load(a->id);
  Require(loaded && DecodeAtlas(loaded->payload), "resident atlas");
  store.Release(a->id);
  auto invalid = EncodeAtlas(*a);
  invalid.pop_back();
  Require(!registry.Import({a->id, "sprite_atlas", "bad.nx2atlas", invalid}), "bad source");
  Require(store.ActiveGeneration() == 1, "bad import changed generation");
  runtime::ImporterRegistry blocked;
  Require(
      blocked.Register(".nx2atlas",
                       [](const runtime::SourceAsset &) -> std::optional<runtime::CanonicalAsset> {
                         return std::nullopt;
                       }),
      "preexisting importer");
  Require(!RegisterImporters(blocked), "duplicate importer registration");
}
void Animation() {
  Clip clip{{2, 2}, {1, 1}, true, {{1, 0.25, "first"}, {2, 0.5, "second"}}};
  const auto encoded = EncodeClip(clip);
  Require(DecodeClip(encoded).has_value(), "clip roundtrip");
  for (std::size_t n = 0; n < encoded.size(); ++n)
    Require(!DecodeClip(std::span(encoded).first(n)), "truncated clip");
  Animator animation;
  Require(animation.Play(clip), "play");
  auto u = animation.Advance(0.25);
  Require(u && u->region == 2 && u->events == std::vector<std::string>{"second"},
          "animation boundary");
  animation.Pause(true);
  Require(animation.Advance(100)->region == 2, "paused advancement");
  animation.Pause(false);
  u = animation.Advance(1.25);
  Require(u && u->region == 1 && u->events == std::vector<std::string>{"first", "second", "first"},
          "loop event trace");
  Require(!animation.Advance(1e9) && animation.RegionId() == 1U, "event budget mutation");
  auto invalid = clip;
  invalid.frames[0].seconds = 0;
  Require(!animation.Play(invalid) && animation.RegionId() == 1U, "invalid switch mutation");
  clip.loop = false;
  Require(animation.Play(clip, 2), "one shot");
  u = animation.Advance(10);
  Require(u && u->finished && u->region == 2 && u->events.size() == 1, "one shot completion");
  Require(animation.Advance(100)->events.empty(), "finished repeated events");
  Require(!animation.Advance(-1), "negative delta");
  Clip decimal{{3, 3}, {1, 1}, true, {{1, 0.1, "first"}, {2, 0.15, "second"}}};
  Animator partitioned, whole;
  Require(partitioned.Play(decimal) && whole.Play(decimal), "decimal fixture");
  std::vector<std::string> events;
  for (unsigned i = 0; i < 600; ++i) {
    auto next = partitioned.Advance(1.0 / 60);
    Require(bool(next), "partition step");
    events.insert(events.end(), next->events.begin(), next->events.end());
  }
  auto all = whole.Advance(10);
  Require(all && events == all->events && partitioned.RegionId() == whole.RegionId(),
          "decimal event partition equivalence");
  Require(partitioned.Advance(0)->events.empty(), "boundary event duplicated");
}
bool Oracle(const Shape &a, const Shape &b) {
  if (!(a.group & b.mask) || !(b.group & a.mask))
    return false;
  if (a.kind == ShapeKind::Circle && b.kind == ShapeKind::Circle) {
    const double d = std::hypot(double(a.center.x) - b.center.x, double(a.center.y) - b.center.y);
    return d <= double(a.radius) + b.radius;
  }
  if (a.kind == ShapeKind::Box && b.kind == ShapeKind::Box)
    return std::abs(double(a.center.x) - b.center.x) <= double(a.halfExtent.x) + b.halfExtent.x &&
           std::abs(double(a.center.y) - b.center.y) <= double(a.halfExtent.y) + b.halfExtent.y;
  const auto &c = a.kind == ShapeKind::Circle ? a : b;
  const auto &box = a.kind == ShapeKind::Box ? a : b;
  const double dx = std::max(0.0, std::abs(double(c.center.x) - box.center.x) - box.halfExtent.x);
  const double dy = std::max(0.0, std::abs(double(c.center.y) - box.center.y) - box.halfExtent.y);
  return std::hypot(dx, dy) <= c.radius;
}
void Queries() {
  std::mt19937 random(71);
  std::vector<Shape> shapes;
  for (std::uint64_t i = 1; i <= 500; ++i) {
    Shape s;
    s.id = i;
    s.kind = i % 2 ? ShapeKind::Circle : ShapeKind::Box;
    s.center = {float(int(random() % 400) - 200) / 10, float(int(random() % 400) - 200) / 10};
    s.radius = float(random() % 20 + 1) / 10;
    s.halfExtent = {float(random() % 20 + 1) / 10, float(random() % 20 + 1) / 10};
    s.group = 1U << unsigned(i % 3);
    s.mask = 3;
    shapes.push_back(s);
  }
  SpatialIndex index(2);
  Require(index.Build(shapes), "grid build");
  for (const auto &query : shapes) {
    std::vector<std::uint64_t> expected;
    for (const auto &other : shapes)
      if (query.id != other.id && Oracle(query, other))
        expected.push_back(other.id);
    Require(index.Query(query) == expected, "grid disagrees with independent oracle");
  }
  auto bad = shapes;
  bad[0].id = bad[1].id;
  Require(!index.Build(bad) && index.Size() == 500, "invalid rebuild mutation");
  shapes.erase(shapes.begin());
  Require(index.Build(shapes) && index.Size() == 499, "remove rebuild");
  Shape circle;
  circle.id = 1;
  circle.radius = 1;
  circle.center = {0, 0};
  Shape box;
  box.id = 2;
  box.kind = ShapeKind::Box;
  box.center = {5, 0};
  box.halfExtent = {1, 1};
  Require(index.Build(std::array{circle, box}), "sweep fixture");
  auto hits = index.Sweep({-10, 0}, {10, 0}, 0);
  Require(hits.size() == 2 && hits[0].id == 1 && hits[1].id == 2, "sweep order");
  Near(hits[0].fraction, 0.45, "swept circle");
  Near(hits[1].fraction, 0.7, "swept box");
  Require(index.Sweep({0, 0}, {0, 0}).front().fraction == 0, "initial overlap");
  // This point lies inside the expanded AABB but outside its rounded corner.
  Require(index.Sweep({3.1F, 1.9F}, {3.1F, 1.9F}, 1).empty(), "rounded corner false positive");
  Shape range;
  range.radius = 1000;
  Require(index.Query(range).size() == 2, "large-query coverage");
  Shape touch = circle;
  touch.center = {2, 0};
  Require(Overlap(circle, touch), "touching circles");
  touch.mask = 0;
  Require(!Overlap(circle, touch), "collision filter");
  box.center = {0.2F, 0};
  box.halfExtent = {0.1F, 1};
  Require(index.Build(std::span(&box, 1)), "decimal sweep fixture");
  Shape point;
  point.center = {0.3F, 0};
  point.radius = 0;
  Require(!Overlap(point, box) && index.Sweep(point.center, point.center).empty(),
          "stationary sweep disagrees with decimal overlap");
}
void RepeatedGridRebuilds() {
  SpatialIndex index(1);
  std::mt19937 random(1701);
  for (unsigned round = 0; round < 12; ++round) {
    std::vector<Shape> shapes;
    for (std::uint64_t id = 1; id <= 512; ++id) {
      if ((id + round) % 7 == 0)
        continue;
      Shape s;
      s.id = id;
      s.kind = id % 2 ? ShapeKind::Circle : ShapeKind::Box;
      s.center = {float(int(id % 32) - 16) + float(round) * 0.37F,
                  float(int(id / 32) - 8) - float(round) * 0.61F};
      s.radius = float(id % 5) * 0.25F;
      s.halfExtent = {float(id % 4) * 0.5F, float(id % 3) * 0.5F};
      s.group = 1U << unsigned(id % 3);
      s.mask = round % 2 ? 7U : 3U;
      shapes.push_back(s);
    }
    std::shuffle(shapes.begin(), shapes.end(), random);
    Require(index.Build(shapes) && index.Size() == shapes.size(), "moving shuffled grid rebuild");
    for (std::size_t n = 0; n < shapes.size(); n += 7) {
      auto query = shapes[n];
      query.radius += 0.75F;
      std::vector<std::uint64_t> expected;
      for (const auto &other : shapes)
        if (query.id != other.id && Oracle(query, other))
          expected.push_back(other.id);
      std::sort(expected.begin(), expected.end());
      Require(index.Query(query) == expected, "reused grid storage disagrees with brute force");
    }
    Shape range;
    range.radius = 1000;
    range.mask = 7;
    const auto published = index.Query(range);
    auto bad = shapes;
    bad.back().id = bad.front().id;
    Require(!index.Build(bad) && index.Query(range) == published, "duplicate rollback query");
    bad = shapes;
    bad.back().center.x = std::numeric_limits<float>::quiet_NaN();
    Require(!index.Build(bad) && index.Query(range) == published, "nonfinite rollback query");
    bad = shapes;
    bad.back().radius = 100;
    bad.back().kind = ShapeKind::Circle;
    Require(!index.Build(bad) && index.Query(range) == published, "cell coverage rollback");
  }
  Shape range;
  range.radius = 1000;
  const auto published = index.Query(range);
  std::vector<Shape> excessive(253);
  for (std::size_t i = 0; i < excessive.size(); ++i) {
    excessive[i].id = i + 1;
    excessive[i].center = {0.5F, 0.5F};
    excessive[i].kind = ShapeKind::Box;
    excessive[i].halfExtent = {31, 31};
  }
  Require(!index.Build(excessive) && index.Query(range) == published,
          "total reference budget rollback");
  Require(index.Build({}) && index.Query(range).empty() && index.Sweep({-10, 0}, {10, 0}).empty(),
          "empty rebuild retained old cells");
  std::vector<Shape> line(128);
  for (std::size_t i = 0; i < line.size(); ++i) {
    line[i].id = 1000 - i;
    line[i].center = {float(i) * 4 - 256, 0};
    line[i].radius = 0.5F;
  }
  Require(index.Build(line), "sweep across reused grid");
  const auto hits = index.Sweep({-300, 0}, {300, 0});
  Require(hits.size() == line.size(), "sweep missed cell or duplicated shape");
  for (std::size_t i = 0; i < hits.size(); ++i) {
    Require(hits[i].id == line[i].id, "sweep fraction ordering after rebuild");
    Near(hits[i].fraction, (double(line[i].center.x) - 0.5 + 300) / 600,
         "independent analytic swept-circle oracle");
  }
}
void ExtractionOrdering() {
  auto atlas = TestAtlas();
  std::vector<SpriteInstance> instances;
  for (std::uint64_t id = 1; id <= 512; ++id) {
    Sprite sprite;
    sprite.atlas = atlas->id;
    sprite.region = unsigned(id % 2) + 1;
    sprite.layer = int(id % 3);
    sprite.order = int(id % 5);
    sprite.sortY = id % 2 == 0;
    sprite.visible = id % 13 != 0;
    sprite.color[3] = id % 11 ? 1.0F : 0.0F;
    runtime::Transform t;
    t.x = double(int(id % 20) - 10) * 0.25;
    t.y = double(int(id / 20) - 10) * 0.25;
    instances.push_back({id, sprite, runtime::ToMatrix(t)});
  }
  const AtlasResolver resolver = [&](AssetId) -> std::optional<AtlasBinding> {
    return AtlasBinding{atlas, 21};
  };
  Camera camera;
  camera.width = camera.height = 100;
  camera.verticalSize = 20;
  const auto frame = ExtractSprites(instances, camera, resolver);
  Require(frame.has_value(), "large extraction fixture");
  auto expected = instances;
  std::erase_if(expected,
                [](const auto &s) { return !s.sprite.visible || s.sprite.color[3] == 0; });
  std::sort(expected.begin(), expected.end(), [](const auto &a, const auto &b) {
    const double ay = a.sprite.sortY ? -a.matrix[13] : 0, by = b.sprite.sortY ? -b.matrix[13] : 0;
    return std::tie(a.sprite.layer, a.sprite.order, ay, a.id) <
           std::tie(b.sprite.layer, b.sprite.order, by, b.id);
  });
  Require(frame->statistics.visible == expected.size(), "visible extraction count");
  for (std::size_t i = 0; i < expected.size(); ++i) {
    Near(frame->vertices[i * 4].position[0], (expected[i].matrix[12] - 0.5) * 5 + 50,
         "independent sorted sprite X");
    Near(frame->vertices[i * 4].position[1], -(expected[i].matrix[13] - 0.5) * 5 + 50,
         "independent sorted sprite Y");
  }
  std::reverse(instances.begin(), instances.end());
  const auto reversed = ExtractSprites(instances, camera, resolver);
  Require(reversed && reversed->indices == frame->indices &&
              reversed->vertices.size() == frame->vertices.size(),
          "input permutation changed extraction shape");
  for (std::size_t i = 0; i < frame->vertices.size(); ++i) {
    const auto &a = frame->vertices[i], &b = reversed->vertices[i];
    Require(a.color == b.color && a.position[0] == b.position[0] &&
                a.position[1] == b.position[1] && a.uv[0] == b.uv[0] && a.uv[1] == b.uv[1],
            "input permutation changed sorted geometry");
  }
  auto bad = instances;
  bad.back().id = bad.front().id;
  bad.back().sprite.visible = false;
  unsigned lookups = 0;
  const AtlasResolver counting = [&](AssetId) -> std::optional<AtlasBinding> {
    ++lookups;
    return AtlasBinding{atlas, 21};
  };
  Require(!ExtractSprites(bad, camera, counting) && lookups == 0,
          "invisible duplicate must reject before resolver callbacks");
  const AtlasResolver mutating = [&](AssetId) -> std::optional<AtlasBinding> {
    for (auto &s : instances)
      s.matrix[12] = 10000;
    return AtlasBinding{atlas, 21};
  };
  const auto snapshot = ExtractSprites(instances, camera, mutating);
  Require(snapshot && snapshot->statistics.visible == frame->statistics.visible,
          "resolver changed prevalidated matrix snapshot");
  for (std::size_t i = 0; i < frame->vertices.size(); ++i)
    Near(snapshot->vertices[i].position[0], frame->vertices[i].position[0],
         "resolver mutation changed snapshot geometry");
}
void Scene() {
  auto atlas = TestAtlas();
  runtime::World world;
  const auto scene = world.LoadScene("2D");
  const auto parent = world.CreateEntity(scene).id;
  const auto first = world.CreateEntity(scene).id;
  const auto second = world.CreateEntity(scene).id;
  runtime::WorldCommandBuffer commands;
  commands.SetTransform(parent, {2, 1, 0});
  commands.SetParent(first, parent, false);
  commands.SetTransform(second, {2, 0, 0});
  Require(commands.Apply(world), "world command fixture");
  SpriteScene sprites(world, scene);
  Sprite sprite;
  sprite.atlas = atlas->id;
  sprite.region = 1;
  sprite.size = {2, 2};
  sprite.sortY = true;
  Require(sprites.Set(first, sprite), "first sprite");
  sprite.region = 2;
  sprite.color[3] = 0.5F;
  Require(sprites.Set(second, sprite), "second sprite");
  const AtlasResolver resolver = [&](AssetId id) -> std::optional<AtlasBinding> {
    return id == atlas->id ? std::optional(AtlasBinding{atlas, 21}) : std::nullopt;
  };
  Camera camera;
  camera.width = 100;
  camera.height = 100;
  camera.verticalSize = 10;
  auto frame = sprites.Extract(camera, resolver);
  Require(frame && frame->statistics.visible == 2 && frame->batches.size() == 1,
          "batch extraction");
  Near(frame->vertices[0].position[0], 60, "parent transform extraction");
  Near(frame->vertices[0].position[1], 50, "orthographic transform");
  Require(frame->vertices[4].color == 0x80FFFFFF, "alpha tint");
  sprite.flipX = true;
  sprite.flipY = true;
  sprite.pivot = {0, 0};
  Require(sprites.Set(second, sprite), "flip edit");
  frame = sprites.Extract(camera, resolver);
  Require(frame && frame->vertices[4].uv[0] > frame->vertices[5].uv[0] &&
              frame->vertices[4].uv[1] < frame->vertices[6].uv[1],
          "flip UVs");
  auto snapshot = sprites.Snapshot();
  Require(snapshot.has_value(), "scene snapshot");
  runtime::World reopened;
  auto restored = SpriteScene::Load(reopened, *snapshot);
  Require(restored && restored->Find(second)->flipX, "scene reopen");
  auto previewWorld = world.CloneForPlay();
  auto preview = SpriteScene::Load(previewWorld, *snapshot);
  Require(!preview, "collision with cloned scene IDs");
  runtime::World isolated(runtime::WorldKind::Play);
  auto play = SpriteScene::Load(isolated, *snapshot);
  Require(play.has_value(), "isolated Play load");
  sprite.order = 999;
  Require(play->Set(second, sprite) && sprites.Find(second)->order == 0, "Play isolation");
  auto corrupt = *snapshot;
  corrupt.pop_back();
  Require(!sprites.Restore(corrupt) && sprites.Snapshot() == snapshot, "corrupt scene rollback");
  Camera far = camera;
  far.center = {1000, 1000};
  Require(sprites.Extract(far, resolver)->statistics.culled == 2, "camera culling");
  unsigned missingLookups = 0;
  const AtlasResolver unavailable = [&](AssetId) -> std::optional<AtlasBinding> {
    ++missingLookups;
    return std::nullopt;
  };
  Require(sprites.Extract(camera, unavailable)->statistics.unavailable == 2,
          "missing residency diagnostics");
  Require(missingLookups == 1, "duplicate missing-atlas load requests within a frame");
  frame = sprites.Extract(camera, resolver);
  auto uploads = frame->Uploads();
  auto merged = Nexora::Presentation::PrepareSpriteDraws(
      {frame->vertices, std::as_bytes(std::span(frame->indices)), frame->batches, uploads, true},
      {}, 100, 100);
  Require(merged && merged->commands.size() == 2 && merged->indices.size() == 18,
          "native scene composition");
  auto compact = Nexora::Presentation::PrepareSpriteDraws(
      {frame->vertices, std::as_bytes(std::span(frame->indices)), frame->batches, uploads, true},
      {}, 100, 100, true);
  Require(compact && compact->spriteInstances.size() == 2 && compact->vertices.size() == 4 &&
              compact->indices.size() == 6 && compact->commands[1].instanceCount == 2,
          "compact GPU instance packet");
  const auto &instance = compact->spriteInstances[0];
  Near(instance.origin[0], frame->vertices[0].position[0], "instance origin");
  Near(instance.origin[0] + instance.axisX[0] + instance.axisY[0], frame->vertices[2].position[0],
       "instance affine corner");
  Require(Nexora::Presentation::ValidateSpriteInstances(compact->View()), "valid instance range");
  auto invalid = *compact;
  invalid.commands[1].firstInstance = UINT32_MAX;
  Require(!Nexora::Presentation::ValidateSpriteInstances(invalid.View()),
          "instance range overflow");
  invalid = *compact;
  invalid.commands[1].instanceCount = UINT32_MAX;
  Require(!Nexora::Presentation::ValidateSpriteInstances(invalid.View()),
          "instance count overflow");
  invalid = *compact;
  invalid.spriteInstances[0].axisX[0] = std::numeric_limits<float>::infinity();
  Require(!Nexora::Presentation::ValidateSpriteInstances(invalid.View()), "nonfinite instance");
  auto nonquad = frame->vertices;
  nonquad[2].position[0] += 10;
  Require(!Nexora::Presentation::PrepareSpriteDraws(
              {nonquad, std::as_bytes(std::span(frame->indices)), frame->batches, uploads, true},
              {}, 100, 100, true),
          "non-affine quad must reject rather than change geometry");

  auto boundary = *compact;
  boundary.spriteInstances.resize(100000);
  boundary.commands[1].firstInstance = 99999;
  boundary.commands[1].instanceCount = 1;
  Require(Nexora::Presentation::ValidateSpriteInstances(boundary.View()), "last bounded instance");
  boundary.commands[1].instanceCount = 2;
  Require(!Nexora::Presentation::ValidateSpriteInstances(boundary.View()), "end range overflow");
  boundary.spriteInstances.resize(100001);
  boundary.commands[1].instanceCount = 1;
  Require(!Nexora::Presentation::ValidateSpriteInstances(boundary.View()),
          "instance payload budget");
  auto invalidCommand = frame->batches;
  invalidCommand[0].elementCount = 999;
  Require(!Nexora::Presentation::PrepareSpriteDraws({frame->vertices,
                                                     std::as_bytes(std::span(frame->indices)),
                                                     invalidCommand, uploads, true},
                                                    {}, 100, 100),
          "invalid GPU range admitted");
}
void Lifecycle() {
  InstancePool a(10), b(10);
  Instance instance;
  instance.sprite.atlas = {1, 1};
  instance.sprite.region = 1;
  auto handles = a.Create(std::vector<Instance>(10, instance));
  Require(handles && a.Size() == 10 && !a.Create(std::array{instance}), "pool capacity");
  Require(!b.Find(handles->front()), "foreign handle");
  auto old = handles->front();
  Require(!a.Destroy(std::array{old, old}) && a.Size() == 10, "duplicate destroy mutation");
  Require(a.Destroy(*handles) && a.Size() == 0 && !a.Find(old), "stale handle");
  for (unsigned i = 0; i < 100; ++i) {
    handles = a.Create(std::vector<Instance>(10, instance));
    Require(handles && a.Size() == 10, "pool recycling");
    a.ForEach([&](Instance &v) {
      v.transform.x += 1;
      Require(!a.Destroy(std::span(*handles).first(1)), "structural reentry");
    });
    Require(a.Destroy(*handles), "recycle destroy");
  }
  FixedStepper one(0.1), many(0.1);
  auto total = one.Advance(1);
  Require(total && total->steps == 10, "fixed total");
  std::uint64_t steps = 0;
  for (double delta : {0.07, 0.13, 0.3, 0.1, 0.4}) {
    auto r = many.Advance(delta);
    Require(r && r->first == steps, "step ordering");
    steps += r->steps;
  }
  Require(steps == 10, "frame partition changed update count");
  Near(many.Advance(0)->remainder, 0, "remainder");
  Require(!many.Advance(1000) && many.Advance(0)->first == 10, "backlog drops simulation time");
}
} // namespace
int main() {
  try {
    Assets();
    Animation();
    Queries();
    RepeatedGridRebuilds();
    ExtractionOrdering();
    Scene();
    Lifecycle();
    std::cout << "2D runtime contracts passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
