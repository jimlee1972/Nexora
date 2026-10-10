# Graphical supported project upgrade preview — Linux cloud evidence

The actual project browser now provides Preview upgrade and Ctrl+Alt+M in both read-write and
read-only modes. The application calls ProjectWorkspace::PreviewUpgrade without acquiring a writer
lease, importing assets or creating project files. Widgets own only a validated summary of the
requested/canonical roots, stable project UUID, supported schema transition, descriptor bytes and
workspace document count. Captured inspection does not authorize later writes; explicit Open
reinspects under ordinary workspace access and retains the accepted exact-original backup/report.

Summary publication validates printable bounded UTF-8, identity/schema consistency, absolute
canonical roots and existing 4 KiB descriptor/4,096-document limits. Changed typed roots, new
selection and busy import clear stale observations and pending requests. Busy state rejects intake
again when the owner consumes it. Existing Create name validation and read-only Create restrictions
remain intact. Invalid summary publication preserves the previous accepted value. Public C++ callers
rebuild; stable C/Gameplay ABI and module dependencies remain unchanged.

## Actual acceptance

Real 1x/2x controls test keyboard and button preview, owning caller/returned-copy lifetimes,
read-only/busy gates, stale roots/queued requests, unsupported/contradictory schemas, malformed/control
UTF-8, labels and byte/document budgets. The empty-name Create fixture explicitly clears the existing
New Project default through the actual name input before checking its unchanged validation.
Initial widget plus existing ImGui contract tests passed **2/2 in 1.79s**.

A real native Xvfb browser verifies read-only and writable legacy previews leave exact descriptor
bytes and absence of project metadata unchanged. Explicit Open completes background activation,
upgrades to schema two and retains the exact CRLF original and plan-only report. The actual dirty
bootstrap scene is explicitly saved through the host before graceful close, preserving the existing
unsaved-close guard. Current-schema and unsupported previews preserve descriptor/backup bytes;
no Vulkan core/synchronization validation messages are accepted. Focused widget/native tests passed
**2/2 in 12.07s**. The complete gate repeated native acceptance successfully in 11.99s.

Full graphical/Slang/Zig/Showcase/native ProjectPlayer Linux Development built 380 incremental steps
and passed **221/221 in 517.84s**, zero skips. Minimal Shipping passed 74 build steps. The native driver
retains normal per-phase deadlines and waits actual activation/scene-save evidence; no assertion,
shutdown guard or Vulkan rejection was weakened.

~~~sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_BUILD_SHOWCASE=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON \
  -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
~~~

GCC 14.2, CMake 3.31.6, Ninja, Slang 2026.18, Zig, Xvfb/xdotool and software Mesa Vulkan were used.
Required Linux CI registers editor.project_upgrade_ui and editor.linux_project_upgrade. The GUI feature is replayed onto accepted Core PR 473; fresh
published-head hosted checks remain mandatory. Windows/macOS/physical workflows, scene/workspace
migration, all-file atomicity, power-loss durability and full milestones remain open (**0/8**).

## Accepted dependency integration

Only the graphical preview commit is replayed onto accepted main
374a0a3809ae53d57c01a4e6fda85f5fe30fcfce (Core PR 473 plus accepted SDK/tabs).
The owning Core plan and graphical/native upgrade fixture source remain unchanged.
Fresh full graphical/Slang/Zig/native-player Linux Development configure/build/CTest
passed **224/224 in 546.90s**, zero skips, after 298 integration build steps; the
actual native upgrade repeat passed in 12.04s. Minimal Shipping reconfiguration/build
passed, 14 steps. The integration uses independently verified clean upstream pinned
Dear ImGui v1.91.9b-docking (4806a1924ff6181180bf5e4b8b79ab4394118875) and
Vulkan-Headers v1.3.296 (29f979ee5aa58b7b005f805ea8df7a855c39ff37) through
FETCHCONTENT_SOURCE_DIR_NEXORA_IMGUI/FETCHCONTENT_SOURCE_DIR_NEXORA_VULKAN_HEADERS
local cache overrides. Dependency pins are unchanged; no generated build output is committed.
Shipping excludes Editor and proves profile/link compatibility only. Fresh hosted
published-head acceptance remains required before merge.
