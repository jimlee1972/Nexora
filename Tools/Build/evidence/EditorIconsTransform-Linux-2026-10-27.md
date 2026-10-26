# Editor icon actions and local Transform layout — Linux evidence

Date: 2026-10-27 (Asia/Taipei). Scope: graphical Editor ImGui presentation, including optional
2D Studio. This record does not accept a complete Editor milestone or physical-host behavior.

## Implemented behavior

- Editor-owned vector artwork replaces action captions in toolbars, auxiliary panels and dialogs.
  Main/context menus, navigation content, ordinary options and field labels retain text.
- Shared icon buttons retain the original scoped action IDs, real pointer/keyboard behavior,
  disabled/selected state and last-item observations after tooltips. Common actions expose
  their existing shortcuts; disabled controls retain function tooltips.
- Local Transform uses Position / Rotation / Scale rows with red X, green Y and blue Z labels.
  Narrow Inspectors place each row name above its three inputs. Reset/Copy/Paste sit by the heading.
  Existing finite/nonzero validation, negative scales, authored Z-X-Y Euler degrees, mixed
  selection, precision, Enter/Escape and atomic Undo/Redo use the established authoring paths.

## Configuration and validation

The Linux development cache enables `NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON`; the existing
`NEXORA_ENABLE_2D=ON` default covers the optional panel. No module dependency, runtime data format,
stable ABI, icon font or external texture dependency was added. System development dependencies
were extracted to a task-local sysroot; those paths and generated build output are not committed.

Commands:

```bash
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON
cmake --build --preset linux-development -j 5
ctest --preset linux-development
```

The targeted `editor.imgui_contract`, `editor.transform_input`, `editor.icon_buttons_1x`/`_2x`
and `editor.transform_layout_1x`/`_2x` pass (6/6). Icon tests exercise actual release activation,
disabled admission, fixed hit targets, selected markers, original `###` identity, tooltip
last-item preservation and caption-independent drawing at both scales. Transform tests use
actual pointer/text/keyboard events at 520- and 220-logical-pixel Inspector widths, multi-selection,
Position-X editing, Tab from Position-Z to Rotation-X, one-step Undo and read-only observation
exclusion. The existing transform-input suite retains draft cancellation/precision/stale-value
and persistence coverage.

The full traversal registered 349 cases: 331 passed, 14 environment-dependent native cases
skipped, and four failures were resolved before delivery. The missing `clang++` was supplied
as a task-local real Clang 19 installation. The 2D and Recovery pointer fixtures were updated
from caption widths to icon hit targets. At 2x DPI in the retained 640x360 viewport, the Game
panel requires scrolling; the Play Apply fixture now sends real wheel input before clicking.
Private Play Apply observations exclude hidden/clipped button centers, including scrollbar space.

After the final configure/build, the complete affected-case set passed **29/29**, including all
four prior failures, both new 1x/2x pairs, existing Transform input, Recovery, Play Apply and
Game/Play contracts. Aggregate coverage is **335 passing cases, 14 environment skips, zero
unresolved failures**; this is not a claim that the initial full traversal was failure-free.

```bash
PATH=/tmp/nexora-editor-icons/bin:$PATH ctest --preset linux-development \
  -R '^(build\.v2_production_toolchain|twod\.graphical_controls|editor\.(recovery_scope|play_apply_changes|icon_buttons_[12]x|transform_layout_[12]x|game_.*|play_.*|imgui_contract|transform_input))$' \
  --output-on-failure
python3 Tools/Build/TestDocumentationCI.py
```

Documentation tests pass 16/16; changed Markdown link targets and bilingual roadmap pairs pass.
All 18 touched C++ sources/headers/includes pass `clang-format --dry-run --Werror`; `git diff
--check` passes. The repository-root README remains unchanged because no overall milestone moved.

## Acceptance limits

These are portable actual-widget and authoring-contract tests. Physical display/DPI/IME,
Windows/macOS target-host rendering, screen-reader semantics and complete Editor milestones
remain separate acceptance gates. Hover tooltips do not implement an accessibility bridge.

## Compiler compatibility follow-up

The initial hosted Windows desktop builds rejected three conditional integer-coordinate
expressions in the shared icon artwork with MSVC C4244 under warnings-as-errors. Their branches
now use explicit floating-point literals. The Linux preset configure/full build and the same
29 affected cases passed again after this correction (29/29). Hosted validation is tracked in
[PR #724](https://github.com/jimlee1972/Nexora/pull/724); no Windows run is claimed from Linux.
