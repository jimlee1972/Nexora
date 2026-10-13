#include "Nexora/Editor/EditorWorkspace.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Record {
  runtime::Id id{}, parent{};
  std::string name;
};
std::string Source(const std::vector<Record> &metadata, const std::vector<Record> &runtime_rows) {
  std::string text = "NEXORA_EDITOR_SCENE 3\n";
  for (const auto &r : metadata)
    text += "node " + std::to_string(r.id) + " " + std::to_string(r.parent) + " " + r.name + '\n';
  text += "world\nNEXORA_SCENE 3 \"Snapshot\" 0 " + std::to_string(runtime_rows.size()) + '\n';
  for (const auto &r : runtime_rows)
    text += std::to_string(r.id) + " " + std::to_string(r.parent) +
            " 0 0 0 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n";
  return text;
}
void Check(const editor::SceneDocument &doc, const std::vector<Record> &expected) {
  const auto rows = doc.Nodes();
  Require(rows.size() == expected.size(), "snapshot cardinality changed");
  for (std::size_t i = 0; i < rows.size(); ++i) {
    Require(rows[i].id == expected[i].id && rows[i].parent == expected[i].parent &&
                rows[i].name == expected[i].name,
            "snapshot lost Runtime order/parent or fresh metadata");
    Require(doc.Key(rows[i].id) == rows[i].Key(), "snapshot returned stale generation");
  }
}
void Correctness() {
  runtime::World world;
  const auto id = world.LoadScene("Index");
  editor::SceneDocument doc(world, id);
  const auto a = doc.Create("Alpha"), b = doc.Create("Beta"), c = doc.Create("Gamma");
  Require(a && b && c, "create failed");
  Check(doc, {{a, 0, "Alpha"}, {b, 0, "Beta"}, {c, 0, "Gamma"}});
  Check(doc, {{a, 0, "Alpha"}, {b, 0, "Beta"}, {c, 0, "Gamma"}});
  const std::string renamed = "Long metadata name that cannot depend on old string storage";
  Require(doc.Rename(*doc.Key(c), renamed), "rename failed");
  Check(doc, {{a, 0, "Alpha"}, {b, 0, "Beta"}, {c, 0, renamed}});
  Require(doc.Undo(), "rename undo failed");
  Check(doc, {{a, 0, "Alpha"}, {b, 0, "Beta"}, {c, 0, "Gamma"}});
  Require(doc.Redo(), "rename redo failed");
  Check(doc, {{a, 0, "Alpha"}, {b, 0, "Beta"}, {c, 0, renamed}});
  Require(doc.Select(std::array{b}) && doc.DeleteSelection(), "delete failed");
  const auto d = doc.Create("Delta"); // Same count as the warm index, with a removed/replaced ID.
  Require(d, "replacement create failed");
  Check(doc, {{a, 0, "Alpha"}, {c, 0, renamed}, {d, 0, "Delta"}});
  Require(doc.Undo() && doc.Undo(), "replacement/delete undo failed");
  Check(doc, {{a, 0, "Alpha"}, {b, 0, "Beta"}, {c, 0, renamed}});
  Require(doc.Redo() && doc.Redo(), "replacement/delete redo failed");
  Check(doc, {{a, 0, "Alpha"}, {c, 0, renamed}, {d, 0, "Delta"}});
  Require(doc.Move(d, 0, 0), "runtime reorder failed");
  Check(doc, {{d, 0, "Delta"}, {a, 0, "Alpha"}, {c, 0, renamed}});
  Require(doc.Reparent(c, a), "parent change failed");
  Check(doc, {{d, 0, "Delta"}, {a, 0, "Alpha"}, {c, a, renamed}});
  const auto foreign = world.CreateEntity(id).id;
  Require(foreign, "foreign Runtime entity failed");
  Check(doc, {{d, 0, "Delta"}, {a, 0, "Alpha"}, {c, a, renamed}});
  const auto before_generation = doc.Generation();
  Require(doc.NewScene() && doc.Generation() != before_generation, "new document failed");
  Check(doc, {});
  const auto fresh = doc.Create("Fresh");
  Check(doc, {{fresh, 0, "Fresh"}});
  runtime::World replacement_world;
  const auto sid = replacement_world.LoadScene("Replacement");
  editor::SceneDocument replacement(replacement_world, sid);
  const std::vector<Record> first{{100, 0, "One"}, {101, 0, "Two"}, {102, 0, "Three"}};
  Require(replacement.ReloadBytes(Source(first, {first[2], first[1], first[0]})),
          "first reload failed");
  Check(replacement, {first[2], first[1], first[0]});
  const auto old_key = *replacement.Key(100);
  const std::vector<Record> second{{102, 0, "New Three"}, {100, 0, "New One"}, {101, 0, "New Two"}};
  Require(replacement.ReloadBytes(Source(second, {second[1], second[0], second[2]})),
          "same-count reload failed");
  Check(replacement, {second[1], second[0], second[2]});
  Require(replacement.Key(100) != old_key && !replacement.Dirty() && !replacement.Undo() &&
              !replacement.Redo(),
          "snapshot changed replacement identity/baseline/history");
  Require(!replacement.ReloadBytes("corrupt"), "bad replacement accepted");
  Check(replacement, {second[1], second[0], second[2]});
}
void Benchmark() {
  constexpr std::size_t count = 100000, samples = 31;
  std::vector<Record> records;
  records.reserve(count);
  for (std::size_t i = 0; i < count; ++i)
    records.push_back({100 + i, 0, "Node " + std::to_string(i)});
  runtime::World world;
  const auto id = world.LoadScene("Snapshot benchmark");
  editor::SceneDocument doc(world, id);
  Require(doc.ReloadBytes(Source(records, records)), "100k source failed");
  for (int i = 0; i < 5; ++i)
    Require(doc.Nodes().size() == count, "warmup failed");
  std::vector<double> times;
  times.reserve(samples);
  for (std::size_t i = 0; i < samples; ++i) {
    const auto begin = std::chrono::steady_clock::now();
    const auto rows = doc.Nodes();
    const auto ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
    Require(rows.size() == count && rows.front().id == 100 && rows.back().id == 100 + count - 1 &&
                rows.front().name == "Node 0" && rows.back().name == "Node 99999" &&
                std::isfinite(ms) && ms > 0,
            "benchmark snapshot correctness/time failed");
    times.push_back(ms);
  }
  Require(!doc.Dirty() && !doc.Undo() && !doc.Redo(), "observation changed baseline/history");
  std::sort(times.begin(), times.end());
  std::cout << "{\"schema\":1,\"scope\":\"cpu-scene-document-nodes\",\"nodes\":" << count
            << ",\"samples\":" << samples << ",\"warmup\":5,\"p50_ms\":" << times[samples / 2]
            << ",\"p95_ms\":" << times[static_cast<std::size_t>(std::ceil(.95 * samples)) - 1]
            << ",\"max_ms\":" << times.back() << "}\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string_view(argv[1]) == "--benchmark")
      Benchmark();
    else
      Correctness();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
