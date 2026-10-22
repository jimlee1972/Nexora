#include "Nexora/Editor/ContentBrowser.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

int main() {
  using namespace nexora;
  using Clock = std::chrono::steady_clock;
  constexpr std::size_t count = 100000;
  std::vector<editor::ContentItem> items;
  items.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    std::ostringstream name;
    name << "Content/Asset_" << std::setw(6) << std::setfill('0') << i << "_Tag" << i % 8
         << "_authored_metadata.obj";
    editor::ContentItem item;
    item.id = {1, i + 1};
    item.path = name.str();
    item.type = i % 2 ? ".TEX" : ".MESH";
    item.artifact_hash = "metadata-only";
    items.push_back(std::move(item));
  }
  const auto milliseconds = [](auto start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
  };
  editor::ContentBrowserModel model;
  auto start = Clock::now();
  if (!model.Reset(items, 7))
    throw std::runtime_error("Benchmark Reset failed");
  std::cout << "schema,phase,items,sample,wall_ms,matches\n1,reset," << count << ",0,"
            << milliseconds(start) << "," << count << '\n';
  start = Clock::now();
  const auto all = model.VisibleCount();
  std::cout << "1,directory_and_unfiltered," << count << ",0," << milliseconds(start) << "," << all
            << '\n';
  if (all != count)
    throw std::runtime_error("Unfiltered sources lost");
  for (std::size_t sample = 0; sample < 48; ++sample) {
    const auto tag = sample % 8;
    const auto type = sample % 3 == 0 ? "" : sample % 3 == 1 ? ".mesh" : ".tex";
    const auto expected =
        type[0] == '\0' || (type == std::string_view(".mesh")) == (tag % 2 == 0) ? count / 8 : 0;
    start = Clock::now();
    model.SetFilter("TAG" + std::to_string(tag), type);
    const auto matches = model.VisibleCount();
    const auto elapsed = milliseconds(start);
    if (matches != expected)
      throw std::runtime_error("Changed filter returned incorrect complete source set");
    std::cout << "1," << (sample == 0 ? "first_filtered" : "changed_filter") << ',' << count << ','
              << sample << ',' << elapsed << ',' << matches << '\n';
  }
}
