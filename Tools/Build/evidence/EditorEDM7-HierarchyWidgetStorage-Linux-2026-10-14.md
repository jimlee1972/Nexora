# ED-M7 bounded hierarchy widget storage — 2026-10-14

Beads: `nexora-0ua.1.11`. Base: merged PR590/main `015bc87a`.
Supporting cloud slice only; full ED-M7 and physical 0/8 acceptance remain open.
[Raw samples, negative controls and candidate C++ hashes](EditorEDM7-HierarchyWidgetStorage-Linux-2026-10-14.json).

## Gap and change

A two-node parent/child scene repeatedly reloaded in one live ImGui host retained obsolete
full-generation TreeNode IDs in the Hierarchy window's default storage. At both 1x and 2x,
entries grew from 2 to 66/130/194/258 after 64/128/192/256 reloads; final capacity was 303.
Actual expansion and source/history checks still passed, so those checks alone missed the growth.

Each hierarchy draw now owns a temporary `ImGuiStorage`. Only synchronous `TreeNodeEx` calls
select it; a scoped guard captures the original window's pointer slot and restores it before
other widget/drag handling, including on unwinding. Unrelated header/window state is preserved.
The temporary table holds only submitted widgets and is destroyed before drawing returns.
Full scoped expansion keys remain authoritative view state. No persistent projection cache is added.

Private test access copies actual `TreeNodeEx` outcomes alongside row keys/positions rather than
recomputing the logical projection or retaining temporary storage. Observations own scalar data,
reset each draw and retain no document/name/World/native borrows. Their vector may retain peak
submitted-row capacity until host destruction. Matching private test consumers must rebuild;
public host layout, stable C/Gameplay ABI, modules, scene/project formats and Shipping boundaries
are unchanged.

## Regression evidence

The identical two-node workload now retains zero default-window tree entries and zero capacity
at every checkpoint through 256 reloads at both DPI settings. Actual stale-scope, same-ID reload,
keyboard and mouse-arrow checks still pass.

Four permanent navigation-test cases cover 1x/2x and Ctrl/macOS behavior settings, each with 128
reloads and 32 delete/recreate cycles. Default-window entries stay at one (an intentionally seeded
unrelated marker). Each draw restores the window storage pointer and preserves that marker.
Tests inspect actual rendered expansion/collapse, scroll replacement branches into the clipped
viewport, cover detach/reattach/read-only observation, and preserve saved World/source/history.
The macOS behavior setting here is a Linux test parameter, not execution on macOS.

The old renderer fails the new lifecycle regression with `hierarchy window storage grew from
2 to 4 obsolete generation entries`. A controlled fault bypassing only the storage guard fails
with the same error. Correct production source was restored, rebuilt and rerun successfully;
no assertions or deadlines were weakened. Existing expansion interaction cases also pass.

The unchanged 100,000-node scale diagnostic passes all eight flat/deep, 1x/2x, two-host-cycle
cases. Each submits 44 hierarchy widgets and retains zero ImGui allocator payload after teardown.
Raw CPU/allocator observations are archived without a before/after speed claim. They are scoped
shell measurements, not total STL/World/process-memory, native GPU, production-frame or physical
soak acceptance.

## Validation

- Targeted deep hierarchy, Select All and keyboard navigation: 3/3, 54.43 seconds.
- `NexoraHierarchyNavigationTests --storage`: four lifecycle cases pass after restoration.
- `NexoraHierarchyNavigationTests --expansion`: existing actual-widget cases pass.
- `NexoraDeepHierarchyTests --benchmark`: eight original scale/lifecycle cases pass.
- `cmake --preset linux-development`: pass.
- `cmake --build --preset linux-development`: pass, 294 actions.
- `ctest --preset linux-development --output-on-failure`: 255/255, 806.60 seconds, no skips.
- `python3 Tools/Build/TestDocumentationCI.py`: 16/16. Touched C++ formatting, source hashes,
  raw record counts, new evidence links and `git diff --check` pass; root README is unchanged.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: pass, five actions.
  This preset disables Editor/ImGui/SDK; it does not prove a Shipping graphical Editor.
