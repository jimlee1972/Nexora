#include "Nexora/Editor/ContentBrowser.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Clock = std::chrono::steady_clock;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
double Milliseconds(Clock::time_point begin) {
  return std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
}
} // namespace
int main(int argc, char **argv) {
  try {
    Require(argc == 2 &&
                (std::string_view(argv[1]) == "before" || std::string_view(argv[1]) == "after"),
            "Expected before/after measurement label");
    constexpr std::size_t count = 100000, per_folder = 100, folders = count / per_folder;
    const auto fixture_begin = Clock::now();
    std::vector<editor::ContentItem> items;
    items.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      const bool mesh = i % 2 == 0;
      editor::ContentItem item;
      item.id = {1, i + 1};
      item.path = std::filesystem::path("Content") / ("Folder" + std::to_string(i / per_folder)) /
                  ("asset-" + std::to_string(i % per_folder) + (mesh ? ".obj" : ".png"));
      item.type = mesh ? "Mesh" : "Texture";
      item.artifact_hash = "metadata-only";
      items.push_back(std::move(item));
    }
    const auto fixture_ms = Milliseconds(fixture_begin);
    editor::ContentBrowserModel model;
    auto begin = Clock::now();
    Require(model.Reset(items, 17), "Metadata dataset admission failed");
    const auto reset_ms = Milliseconds(begin);
    const auto revision = model.Revision();
    begin = Clock::now();
    Require(model.ChildFolderSnapshot()->size() == folders && model.VisibleCount() == 0,
            "Initial root query lost directories or invented direct assets");
    const auto first_ms = Milliseconds(begin);
    std::uint64_t checksum{};
    begin = Clock::now();
    for (std::size_t step = 0; step < 100; ++step) {
      Require(model.SetFolder(std::filesystem::path("Content") /
                              ("Folder" + std::to_string((step * 37) % folders))),
              "Cold folder navigation failed");
      Require(model.ChildFolderSnapshot()->empty() && model.VisibleCount() == per_folder,
              "Cold folder query omitted or mixed assets");
      checksum += model.Visible(0, per_folder).size();
    }
    const auto cold_ms = Milliseconds(begin);
    begin = Clock::now();
    constexpr const char *queries[]{"asset-1", "asset-9", "not-found", ""};
    constexpr const char *types[]{"Mesh", "Texture", "", "Mesh"};
    constexpr std::size_t expected[]{5, 6, 0, 50};
    for (std::size_t step = 0; step < 200; ++step) {
      const auto query = step % 4;
      model.SetFilter(queries[query], types[query]);
      const auto matched = model.VisibleCount();
      Require(matched == expected[query], "Search returned an incomplete or wrong typed result");
      checksum += matched;
    }
    const auto search_ms = Milliseconds(begin);
    Require(checksum == 13050 && model.Items().size() == count && model.Selection().empty() &&
                model.Revision() == revision && !model.Undo(),
            "Read-only benchmark changed source, selection or authoring history");
    std::cout << std::fixed << std::setprecision(6)
              << "{\"schema\":1,\"dataset\":\"content-directory-metadata\",\"label\":\"" << argv[1]
              << "\",\"items\":" << count << ",\"folders\":" << folders
              << ",\"fixture_ms\":" << fixture_ms << ",\"reset_ms\":" << reset_ms
              << ",\"first_root_query_ms\":" << first_ms
              << ",\"cold_folder_queries\":100,\"cold_folder_ms\":" << cold_ms
              << ",\"search_queries\":200,\"search_ms\":" << search_ms
              << ",\"checksum\":" << checksum << "}\n";
    std::cout.flush();
    Require(static_cast<bool>(std::cout), "Benchmark result output failed");
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
