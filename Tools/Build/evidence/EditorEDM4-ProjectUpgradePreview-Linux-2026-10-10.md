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
Required Linux CI registers editor.project_upgrade_ui and editor.linux_project_upgrade. The branch
will replay only this GUI feature after the separate Core upgrade prerequisite is accepted; fresh
published-head hosted checks remain mandatory. Windows/macOS/physical workflows, scene/workspace
migration, all-file atomicity, power-loss durability and full milestones remain open (**0/8**).
