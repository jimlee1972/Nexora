# ED-M1 Content asset visibility projection — Linux, 2026-10-20

Beads: `nexora-gu7.2.1`, under M1-02 `nexora-gu7.2` and execution epic `nexora-gu7`.
Original base: `b8c31d72437a77cc190ad49eb70cf97b6c23fac6` (PR #662).

## Contract and scope

The previous Count and every clipped asset-range query rescanned all Content items, rebuilding
parent paths and case-folded filenames/types even when the view had not changed. They now share
one lazy immutable owning vector of matching item indices. SelectVisible uses that projection too.
Ordering, exact current-folder membership and normalized query/type semantics remain unchanged.
Warm Count is O(1); a warm Visible range visits only its clamped requested entries. Initial query
work still scans all items, and retained index storage is O(matches), not an overall memory budget.

Successful Reset/Discover/path Commit/Undo and actual folder/filter changes reset the projection.
Reveal also resets it when clearing filters in the same folder. Failed admission, equal normalized
filters, same-folder navigation, selection and artifact-only publication retain it. Queries resolve
current item storage every time, so artifact data remains current. Copies may share immutable
indices but never item pointers; replacing one model does not change another. Item pointers returned
by Visible retain their existing mutation-bounded borrow contract. Queries are serialized on the
authoring thread, like ChildFolderSnapshot; const does not imply concurrent access.

The exported C++ model has an additional private shared owner and requires consumer rebuilds.
Stable C/Gameplay ABI, persisted files, module dependencies, writer/Undo rules and backend behavior
are unchanged. Cold folders, activation/other lookups, indexing, whole frames, process memory,
GPU and representative production/physical budgets remain open. No ED milestone is closed.

## Scoped preliminary query measurements

These are standalone GCC14.2 C++20 -O2 measurements of Count plus the last24 matching rows,
not ImGui/full-frame timings. Both binaries directly compile their own ContentBrowser.cpp and
header layout and link the same unchanged base Foundation/Runtime libraries, never EditorCore.
Optional 2D payload validation is not compiled into this standalone harness; all fixture payloads
are empty and no query branch depends on that feature. The full product gate separately rebuilds
all affected modules with their actual feature flags.

Each binary resets100,000 synthetic assets (half directly in Content, half in a nested folder),
filters MATCH/.TXT to50,000 rows and verifies exact row IDs. For each of31 samples, a fresh model
copy outside the measured interval provides one cold Count/range query, followed by four warm
queries. Both checksums are38,742,560. No CPU pinning or calibrated physical baseline is implied;
sequential A/B order and shared-executor load can affect the retained raw timings.

| Count plus last24 query | Before p50/p95 (microseconds) | After p50/p95 (microseconds) |
| --- | ---: | ---: |
| Cold | 54880.651000 / 210101.196000 | 34609.917000 / 87276.867000 |
| Warm | 56504.008500 / 151268.896500 | 0.086500 / 0.111000 |

No whole-frame speedup or total allocator/RSS/GPU reduction is inferred. Raw samples, including
variance, are retained below. Source-only code in the probe uses no IO or native renderer.

### Reproducible standalone probe

Save this source as `content-visible-probe.cpp`. For each source tree, compile its model source
against its own Editor header. In the recorded run, `CONTENT_PROBE_COMMON` was the base Development
build shared by both variants because Foundation/Runtime were unchanged:

```bash
c++ -std=c++20 -O2 \
  -I"$CONTENT_PROBE_TREE/Engine/Editor/include" \
  -I"$CONTENT_PROBE_COMMON/Engine/Foundation/include" \
  -I"$CONTENT_PROBE_COMMON/Engine/Runtime/include" \
  -I"$CONTENT_PROBE_COMMON/Engine/Renderer/include" \
  content-visible-probe.cpp "$CONTENT_PROBE_TREE/Engine/Editor/src/ContentBrowser.cpp" \
  -L"$CONTENT_PROBE_COMMON/build/linux-development/Engine/Foundation" \
  -L"$CONTENT_PROBE_COMMON/build/linux-development/Engine/Runtime" \
  -Wl,-rpath,"$CONTENT_PROBE_COMMON/build/linux-development/Engine/Foundation" \
  -Wl,-rpath,"$CONTENT_PROBE_COMMON/build/linux-development/Engine/Runtime" \
  -lNexoraRuntime -lNexoraFoundation -o content-visible-probe
./content-visible-probe
```

```cpp
#include "Nexora/Editor/ContentBrowser.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
int main() {
  using namespace nexora;
  std::vector<editor::ContentItem> items;
  for (unsigned i = 0; i < 100000; ++i) {
    auto digits = std::to_string(i);
    digits.insert(0, 8 - digits.size(), '0');
    items.push_back({{802, i + 1}, (i % 2 ? "Content/Nested/" : "Content/") +
                                      std::string("Match-") + digits + ".txt",
                     ".txt", "v1", editor::ThumbnailState::Ready});
  }
  editor::ContentBrowserModel base;
  if (!base.Reset(items, 1)) return 1;
  items.clear(); items.shrink_to_fit();
  base.SetFilter("MATCH", ".TXT");
  std::vector<double> cold, warm;
  std::uint64_t checksum = 0;
  for (int sample = 0; sample < 31; ++sample) {
    auto model = base;
    const auto query = [&] {
      const auto count = model.VisibleCount();
      const auto rows = model.Visible(count - 24, 24);
      if (count != 50000 || rows.size() != 24 || rows.front()->id.low != 99953 ||
          rows.back()->id.low != 99999) throw std::runtime_error("query mismatch");
      checksum += count + rows.front()->id.low + rows.back()->id.low;
    };
    auto start = std::chrono::steady_clock::now();
    query();
    cold.push_back(std::chrono::duration<double, std::micro>(
                       std::chrono::steady_clock::now() - start).count());
    start = std::chrono::steady_clock::now();
    for (int i = 0; i < 4; ++i) query();
    warm.push_back(std::chrono::duration<double, std::micro>(
                       std::chrono::steady_clock::now() - start).count() / 4);
  }
  std::cout << std::setprecision(10) << "{\"items\":100000,\"matches\":50000,\"slice\":24,"
            << "\"samples\":31,\"warm_calls_per_sample\":4,\"checksum\":" << checksum;
  for (auto *values : {&cold, &warm}) {
    const auto name = values == &cold ? "cold" : "warm";
    std::cout << ",\"" << name << "_samples_us\":[";
    for (std::size_t i = 0; i < values->size(); ++i)
      std::cout << (i ? "," : "") << (*values)[i];
    std::ranges::sort(*values);
    std::cout << "],\"" << name << "_p50_us\":" << (*values)[15]
              << ",\"" << name << "_p95_us\":" << (*values)[29];
  }
  std::cout << "}\n";
}
```

### Raw results

Before:

```json
{
  "items": 100000,
  "matches": 50000,
  "slice": 24,
  "samples": 31,
  "warm_calls_per_sample": 4,
  "checksum": 38742560,
  "cold_samples_us": [
    72983.226,
    54880.651,
    52129.012,
    43662.914,
    49505.712,
    55624.144,
    59056.199,
    53831.549,
    51294.249,
    54360.215,
    58523.629,
    339147.108,
    65895.488,
    84638.053,
    67964.882,
    120668.695,
    125615.089,
    55508.094,
    58360.704,
    70136.001,
    66994.82,
    54368.758,
    210101.196,
    45544.723,
    50705.048,
    47150.879,
    42722.807,
    49319.691,
    49044.335,
    49087.978,
    50569.865
  ],
  "cold_p50_us": 54880.651,
  "cold_p95_us": 210101.196,
  "warm_samples_us": [
    77634.28975,
    53710.234,
    69423.726,
    69766.5315,
    52386.111,
    55328.62575,
    50072.7365,
    56491.318,
    47936.1305,
    56504.0085,
    54466.68,
    115257.4635,
    149860.5947,
    120677.3085,
    77062.2095,
    197993.4507,
    63451.47,
    91039.256,
    111062.2355,
    151268.8965,
    55212.34625,
    69103.97675,
    56828.25575,
    48912.78025,
    52745.971,
    47429.88475,
    47208.28625,
    49710.91025,
    63153.31,
    48987.4645,
    44435.532
  ],
  "warm_p50_us": 56504.0085,
  "warm_p95_us": 151268.8965
}
```

After:

```json
{
  "items": 100000,
  "matches": 50000,
  "slice": 24,
  "samples": 31,
  "warm_calls_per_sample": 4,
  "checksum": 38742560,
  "cold_samples_us": [
    38389.639,
    38219.846,
    32336.596,
    28459.083,
    62915.611,
    27522.666,
    28334.758,
    69231.901,
    57994.376,
    34609.917,
    35053.177,
    84538.551,
    43169.761,
    27757.388,
    34500.848,
    33670.356,
    32895.942,
    26573.571,
    87276.867,
    48175.205,
    28778.328,
    58489.678,
    32428.827,
    39498.1,
    43748.091,
    26382.499,
    26120.174,
    35865.476,
    99705.016,
    27148.049,
    31409.137
  ],
  "cold_p50_us": 34609.917,
  "cold_p95_us": 87276.867,
  "warm_samples_us": [
    0.09775,
    0.083,
    0.069,
    0.091,
    0.085,
    0.0845,
    0.0875,
    0.084,
    0.09975,
    0.079,
    0.084,
    0.0825,
    0.09825,
    0.09175,
    0.08275,
    0.09275,
    0.0865,
    0.07325,
    0.08575,
    0.1015,
    0.12475,
    0.091,
    0.111,
    0.09125,
    0.088,
    0.091,
    0.078,
    0.08625,
    0.08,
    0.084,
    0.101
  ],
  "warm_p50_us": 0.0865,
  "warm_p95_us": 0.111
}
```

## Validation

The new public-behavior model regression passed when compiled directly with the candidate model.
It warms Count before checking ranges against current model-owned item addresses; covers case-folded
filters, current artifact payloads, rename/Undo, folder/reveal changes, move/delete/Undo, discovery
preserved through Undo, rejected mutations, SelectVisible/ranges, same-count/same-generation Reset,
copy/assignment/move and empty/SIZE_MAX slices. This preliminary standalone execution is not the
full product gate or graphical/native acceptance.

The final product gate used GCC14.2, Linux Development Modular, graphical Editor enabled,
Slang/Zig gameplay OFF, and four CTest workers (serial100k lifecycle remains RUN_SERIAL):

```bash
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=4 ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Development rebuilt317 steps and registered286 tests:274 actually passed,12 existing native
feature/environment skips, zero failures,342.75 s. The new model regression in
`editor.preview_contract` passed0.07 s. Existing actual1x/2x Content keyboard controls37.00 s and
dependency/folder-drop/Undo4.01 s passed. Serial100k hierarchy lifecycle218.26 s, deep hierarchy
117.85 s and the remaining full suite passed. Suite timings are not a performance comparison to
previous serial runs. The12 skipped cases are four window Vulkan scene/PBR/upload/swapchain,
seven renderer native Vulkan recording/descriptor/timing/benchmark cases, and material-scene native;
they do not count as executed native acceptance.

Shipping Monolithic configure/build passed with Editor/testing excluded. Documentation-routing
fixtures passed16/16; changed Markdown links, touched C++ formatting and diff whitespace checks
passed. The three source/test digests remained unchanged through the full gate. Windows/macOS
and physical-host validation were not run locally. Physical display and complete production
budgets remain open; ED-M0 through ED-M7 acceptance stays0/8.
