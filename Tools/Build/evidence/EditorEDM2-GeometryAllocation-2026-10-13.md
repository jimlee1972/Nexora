# ED-M2 frame geometry allocation failure conservation

The private Scene/Game preview geometry builder previously appended converted vertices before
preparing index capacity. A failed vector allocation could escape instead of returning the optional
unavailable-mesh result consumed by both callers, and could leave partially published geometry.

The builder now validates the whole input and reserves both final bounded capacities before changing
element contents. It catches `std::bad_alloc` and returns `nullopt`. Prior contents, sizes and ranges
remain valid; capacity may grow if the first reserve succeeds and the second fails. Growth remains
geometric within 65,535 vertices and 1,048,576 indices. Fitting appends do not allocate.

`editor.scene_mesh_allocation_failure` uses an isolated executable with a thread-local replacement
for ordinary allocation. Fixtures and assertions are unarmed. It faults the first reserve and then
the second reserve after the first succeeds, checks unchanged vertex attributes and index ranges,
and retries to verify one complete converted mesh with indices rebased against the original size.
It also checks that fitting and invalid appends perform no allocation. No product allocator or
public ABI changes. This test covers geometry vector growth, not every allocation in Scene/Game
frame preparation or GPU allocation failure.

The identical test compiled against the pre-change header exits 1 with
`allocation failure escaped the unavailable-mesh fallback`, establishing a regression reproducer.

Linux cloud validation:

- `cmake --preset linux-development`: passed.
- `cmake --build --preset linux-development`: passed, 435 build actions.
- `ctest --preset linux-development --output-on-failure`: 247/247 passed, no skips, 829.01 seconds.
  The new allocation fixture, existing mesh geometry/Game View tests, and actual Xvfb Scene/OBJ
  fixtures all ran successfully.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed, 7 build actions.
- Changed C++ formatting, `git diff --check`, changed Markdown/link validation and
  `python3 Tools/Build/TestDocumentationCI.py`: passed (16 documentation tests).

Hosted platform results are recorded in the associated PR and Beads task `nexora-owg.1.5`; Linux
cloud results alone do not claim Windows or macOS execution. Persistent per-asset GPU residency,
material shader workflows and full target-host graphical acceptance remain open.
