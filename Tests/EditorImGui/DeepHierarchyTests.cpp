#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
constexpr std::size_t kCount = 100000;
constexpr std::size_t kChain = kCount - 2;
constexpr runtime::Id kFirst = 100;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
std::string Source(bool deep) {
  std::string text = "NEXORA_EDITOR_SCENE 3\n";
  for (std::size_t i = 0; i < kCount; ++i) {
    const auto parent = deep && i > 0 && i < kChain ? kFirst + i - 1 : 0;
    text += "node " + std::to_string(kFirst + i) + " " + std::to_string(parent) + " " +
            (i == kChain - 1 ? "Deep leaf" : "Node " + std::to_string(i)) + '\n';
  }
  text += "world\nNEXORA_SCENE 3 \"Large hierarchy\" 0 " + std::to_string(kCount) + '\n';
  for (std::size_t i = 0; i < kCount; ++i) {
    const auto parent = deep && i > 0 && i < kChain ? kFirst + i - 1 : 0;
    text += std::to_string(kFirst + i) + " " + std::to_string(parent) +
            " 0 0 0 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n";
  }
  return text;
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-deep-hierarchy-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Large hierarchy");
  editor::SceneDocument scene{world, scene_id};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  std::filesystem::path path = root / "Content/Large.scene";
  std::string source;
  Fixture(bool deep, float scale) : source(Source(deep)) {
    Require(workspace.Create(root, "Large hierarchy") && world.Activate(scene_id),
            "large hierarchy workspace setup failed");
    std::ofstream output(path, std::ios::binary);
    output << source;
    output.close();
    Require(!output.fail() && scene.Reload(path) && scene.Nodes().size() == kCount &&
                !scene.Dirty(),
            "real 100k authoring scene did not load cleanly");
    ui.SetDisplay(1280 * scale, 900 * scale, scale);
    // Exercise the hierarchy in the production 3D-preview layout. This test does not measure
    // native GPU submission or the separate top-down overview marker traversal.
    ui.SetNativeScenePreview(true);
    Access::ConfigureSyntheticInput(ui, false);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
  }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, &workspace);
    static_cast<void>(ui.EndFrame());
  }
  void Settled() {
    for (int i = 0; i < 4; ++i)
      Draw();
    Access::FocusHierarchy(ui);
    Draw();
    Draw();
  }
  void Press(Nexora::Window::Key key) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(key);
    event.value1 = 1;
    ui.ProcessEvents(std::array{event});
    Draw();
    event.value1 = 0;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void Preserved() {
    std::ifstream input(path, std::ios::binary);
    const std::string bytes{std::istreambuf_iterator<char>(input),
                            std::istreambuf_iterator<char>{}};
    Require(bytes == source && !scene.Undo() && !scene.Redo() && !scene.Dirty(),
            "hierarchy observation changed source, saved baseline or authoring history");
  }
};
void Deep(float scale) {
  Fixture f(true, scale);
  const auto nodes = f.scene.Nodes();
  std::vector<editor::SceneDocument::NodeKey> expanded;
  expanded.reserve(kChain);
  for (std::size_t i = 0; i + 1 < kChain; ++i)
    expanded.push_back(nodes[i].Key());
  const auto generation = f.scene.Generation();
  Access::SetHierarchyExpanded(f.ui, expanded);
  f.Settled();
  const auto rows = Access::HierarchyRows(f.ui, f.scene);
  Require(rows.size() == kCount && Access::Inspect(f.ui).hierarchy_visible_rows == kCount &&
              Access::Inspect(f.ui).hierarchy_rendered_rows > 0 &&
              Access::Inspect(f.ui).hierarchy_rendered_rows < 128,
          "deep hierarchy did not retain all rows and clip rendered widgets");
  for (std::size_t i = 0; i < kCount; ++i)
    Require(rows[i].key == nodes[i].Key() && rows[i].depth == (i < kChain ? i : 0) &&
                rows[i].has_children == (i + 1 < kChain),
            "deep parent-first or root-sibling order, depth or generation changed");
  Access::FocusHierarchy(f.ui);
  f.Draw();
  f.Press(Nexora::Window::Key::End);
  Require(f.scene.Selection().size() == 1 && f.scene.Selection().front() == nodes.back().id &&
              Access::HierarchyRowPosition(f.ui, nodes.back().Key()),
          "deep End did not select and reveal the final root sibling");
  f.Press(Nexora::Window::Key::Home);
  Require(f.scene.Selection().size() == 1 && f.scene.Selection().front() == nodes.front().id,
          "deep Home did not restore the first root selection");
  Access::QueueHierarchyExpansion(f.ui, nodes.front().Key(), false);
  f.Draw();
  auto collapsed = Access::HierarchyRows(f.ui, f.scene);
  Require(collapsed.size() == 3 && collapsed[0].key == nodes[0].Key() &&
              collapsed[1].key == nodes[kChain].Key() &&
              collapsed[2].key == nodes[kChain + 1].Key(),
          "collapse lost stable root siblings");
  Access::QueueHierarchyExpansion(f.ui, nodes.front().Key(), true);
  f.Draw();
  Require(Access::HierarchyRows(f.ui, f.scene).size() == kCount,
          "re-expansion lost the retained deep subtree");
  Access::SetHierarchyFilter(f.ui, "Deep leaf");
  f.Draw();
  const auto filtered = Access::HierarchyRows(f.ui, f.scene);
  Require(filtered.size() == 1 && filtered.front().key == nodes[kChain - 1].Key() &&
              filtered.front().depth == 0,
          "deep filter did not retain the exact owning identity");
  Access::SetHierarchyFilter(f.ui, "");
  Require(f.scene.Reload(f.path) && f.scene.Generation() != generation,
          "generation invalidation setup failed");
  Access::SetHierarchyExpanded(f.ui, expanded);
  Access::QueueHierarchyExpansion(f.ui, nodes.front().Key(), true);
  f.Draw();
  const auto replaced = Access::HierarchyRows(f.ui, f.scene);
  Require(replaced.size() == 3 &&
              replaced.front().key.document_generation == f.scene.Generation() &&
              rows.front().key.document_generation == generation,
          "stale expansion survived document replacement or owning observations changed");
  f.Preserved();
}
void Flat() {
  Fixture f(false, 1);
  f.Settled();
  const auto rows = Access::HierarchyRows(f.ui, f.scene);
  const auto state = Access::Inspect(f.ui);
  Require(rows.size() == kCount && state.hierarchy_visible_rows == kCount &&
              state.hierarchy_rendered_rows > 0 && state.hierarchy_rendered_rows < 128,
          "100k flat hierarchy did not clip widgets");
  for (std::size_t i = 0; i < rows.size(); ++i)
    Require(rows[i].key.id == kFirst + i && rows[i].depth == 0 && !rows[i].has_children,
            "flat sibling order or leaf state changed");
  f.Preserved();
}

// Only ImGui allocations are measured. World/document/container allocations and native GPU
// resources are outside this counter; reporting this as total process memory would be misleading.
struct AllocationStats final {
  std::size_t live_bytes = 0;
  std::size_t peak_bytes = 0;
  std::size_t live_count = 0;
};
struct alignas(std::max_align_t) AllocationHeader final {
  std::size_t bytes;
};
void *Allocate(std::size_t bytes, void *user) {
  auto *header = static_cast<AllocationHeader *>(std::malloc(sizeof(AllocationHeader) + bytes));
  if (!header)
    return nullptr;
  header->bytes = bytes;
  auto &stats = *static_cast<AllocationStats *>(user);
  stats.live_bytes += bytes;
  stats.peak_bytes = std::max(stats.peak_bytes, stats.live_bytes);
  ++stats.live_count;
  return header + 1;
}
void Free(void *memory, void *user) {
  if (!memory)
    return;
  auto *header = static_cast<AllocationHeader *>(memory) - 1;
  auto &stats = *static_cast<AllocationStats *>(user);
  stats.live_bytes -= header->bytes;
  --stats.live_count;
  std::free(header);
}
struct AllocatorScope final {
  ImGuiMemAllocFunc allocate;
  ImGuiMemFreeFunc free;
  void *user = nullptr;
  AllocationStats stats;
  AllocatorScope() {
    Require(ImGui::GetCurrentContext() == nullptr, "scale benchmark requires no current context");
    ImGui::GetAllocatorFunctions(&allocate, &free, &user);
    ImGui::SetAllocatorFunctions(Allocate, Free, &stats);
  }
  ~AllocatorScope() { ImGui::SetAllocatorFunctions(allocate, free, user); }
};
double Percentile(const std::vector<double> &sorted, double fraction) {
  return sorted[static_cast<std::size_t>(std::ceil(fraction * sorted.size())) - 1];
}
void Benchmark() {
  constexpr int kSamples = 12;
  constexpr int kCycles = 2;
  AllocatorScope allocator;
  for (int cycle = 0; cycle < kCycles; ++cycle) {
    for (const bool deep : {false, true}) {
      for (const float scale : {1.F, 2.F}) {
        allocator.stats.peak_bytes = 0;
        std::vector<double> milliseconds;
        milliseconds.reserve(kSamples);
        std::size_t maximum_rows = 0;
        std::size_t maximum_vertices = 0;
        {
          Fixture f(deep, scale);
          // SetDisplay takes logical dimensions; keep the viewport identical across DPI cases.
          f.ui.SetDisplay(1280, 900, scale);
          Require(ImGui::GetIO().DisplaySize.x == 1280 && ImGui::GetIO().DisplaySize.y == 900 &&
                      ImGui::GetIO().DisplayFramebufferScale.x == scale &&
                      ImGui::GetIO().DisplayFramebufferScale.y == scale,
                  "scale benchmark viewport or framebuffer DPI does not match metadata");
          if (deep) {
            const auto nodes = f.scene.Nodes();
            std::vector<editor::SceneDocument::NodeKey> expanded;
            expanded.reserve(kChain - 1);
            for (std::size_t i = 0; i + 1 < kChain; ++i)
              expanded.push_back(nodes[i].Key());
            Access::SetHierarchyExpanded(f.ui, expanded);
          }
          // Six production-shell draws settle docking/focus/atlas before timing starts.
          f.Settled();
          for (int sample = 0; sample < kSamples; ++sample) {
            const auto begin = std::chrono::steady_clock::now();
            f.Draw();
            const auto elapsed =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin)
                    .count();
            Require(std::isfinite(elapsed) && elapsed > 0, "invalid scale frame measurement");
            milliseconds.push_back(elapsed);
            const auto state = Access::Inspect(f.ui);
            Require(state.hierarchy_visible_rows == kCount && state.hierarchy_rendered_rows > 0 &&
                        state.hierarchy_rendered_rows < 128,
                    "scale benchmark lost real 100k rows or bounded widget clipping");
            maximum_rows =
                std::max(maximum_rows, static_cast<std::size_t>(state.hierarchy_rendered_rows));
            auto *draw = ImGui::GetDrawData();
            Require(draw && draw->Valid && draw->TotalVtxCount > 0,
                    "scale benchmark did not produce real ImGui geometry");
            maximum_vertices =
                std::max(maximum_vertices, static_cast<std::size_t>(draw->TotalVtxCount));
          }
          f.Preserved();
        }
        Require(allocator.stats.live_bytes == 0 && allocator.stats.live_count == 0 &&
                    ImGui::GetCurrentContext() == nullptr,
                "large-scene host teardown retained ImGui allocations or context");
        std::sort(milliseconds.begin(), milliseconds.end());
        std::cout
            << std::fixed << std::setprecision(3)
            << "{\"schema\":1,\"scope\":\"cpu-imgui-product-shell\",\"nodes\":" << kCount
            << ",\"dataset\":\"" << (deep ? "expanded-chain" : "flat-roots")
            << "\",\"cycle\":" << cycle << ",\"dpi\":" << scale
            << ",\"logical_width\":1280,\"logical_height\":900,\"warmup_frames\":6,\"samples\":"
            << kSamples << ",\"p50_ms\":" << Percentile(milliseconds, .5)
            << ",\"p95_ms\":" << Percentile(milliseconds, .95)
            << ",\"max_ms\":" << milliseconds.back()
            << ",\"max_hierarchy_widgets\":" << maximum_rows
            << ",\"max_vertices\":" << maximum_vertices
            << ",\"imgui_peak_bytes\":" << allocator.stats.peak_bytes
            << ",\"imgui_retained_bytes\":" << allocator.stats.live_bytes
            << ",\"sorted_frame_ms\":[";
        for (std::size_t i = 0; i < milliseconds.size(); ++i)
          std::cout << (i ? "," : "") << milliseconds[i];
        std::cout << "]}" << std::endl;
      }
    }
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string_view(argv[1]) == "--benchmark") {
      Benchmark();
      return 0;
    }
    Require(argc == 1, "expected no arguments or --benchmark");
    Deep(1);
    Deep(2);
    Flat();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
