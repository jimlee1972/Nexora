# SP-01 display implementation evidence — 2026-10-23

**Portable implementation and Linux Development/Shipping verification PASS. Windows native
acceptance remains NOT RUN / NOT ACCEPTED.** The implementation is the commit containing this
report, based on Nexora main `7087e832`. No existing M6/M7 milestone count is advanced.

## Results

| Check | Result and boundary |
| --- | --- |
| Full linux-development configure/build/CTest | 268 registered; **252 passed, 16 skipped, 0 failed** |
| New display cases | C++ snapshot/service-prefix/geometry/UI hit tests, all-field C/Zig layout, Zig coordinate wrapper and actual independent Zig Player consumer executed and passed |
| Existing service ABI consumer | New Zig consumer retains historical service hosts; absent display suffix returns unsupported without losing older services |
| linux-shipping | Full Monolithic build passed with 2D and Zig enabled |
| Shipping independent Zig consumer | ReleaseSafe Zig library exported with the Monolithic Player; three headless dimensions, safe-frame observations, UI generation and malformed descriptor admission passed; inventory is not marked as a development copy |
| Windows-target compilation | Win32Window.cpp compiled to a Windows x64 GNU COFF object using Zig/Clang; no MSVC or Windows native execution claim |
| Formatting and patch hygiene | Touched C++ files match clang-format 19; git diff --check passed |

The 16 CTest skips are the native Vulkan/showcase/window/recording and X11 held-key gates whose
runtime prerequisites are unavailable. Native backends/Vulkan are enabled in this configuration;
Slang and the graphical Editor shell retain their disabled defaults, and Editor core is enabled.
No new display case is skipped. See [raw CTest output](SP01-GameplayDisplay-Linux-2026-10-23/development-tests.log),
[JUnit](SP01-GameplayDisplay-Linux-2026-10-23/development-junit.xml),
[Shipping consumer output](SP01-GameplayDisplay-Linux-2026-10-23/shipping-display-tests.log),
[commands/toolchain](SP01-GameplayDisplay-Linux-2026-10-23/commands.txt) and
[Windows compile input hashes](SP01-GameplayDisplay-Linux-2026-10-23/windows-cross-inputs.sha256).

Early validation attempts exposed the missing host clang++ prerequisite, a missing Python lookup
in the new CMake test registration, and shared-library relinking overlapping an initial test run.
The lookup was fixed and task-local Clang was provisioned. The recorded final full gate ran after
its build completed, with no overlapping rebuild. Its evidence supersedes those setup attempts.
Hosted compilation additionally identified macOS unused parameters and an MSVC test-local shadow; both were corrected. The final runtime review made work-area fitting opt-in for existing hosts, with the Player enabling it. The full gate was rerun after that runtime adjustment; the later MSVC test-local rename passed [its isolated rerun](SP01-GameplayDisplay-Linux-2026-10-23/display-post-review-tests.log) and additional GCC shadow diagnostics.
A final review also made recoverable resize-time presentation failures retry on the next frame
without falsely incrementing delivered-frame counters; native acceptance of that path stays open.

## Implemented behavior

The Player accepts optional startup display settings without changing legacy descriptor defaults.
It publishes actual framebuffer/logical extents, DPI, observed mode and centered safe viewport
through an append-only optional C/Zig service. Snapshot state is stable during module callbacks;
window requests report native admission and later snapshots show actual dimensions. UI input and
geometry use one DPI conversion. Camera projection, Tilemap visibility, Sprite offset/scissors
and Zig picking share the safe frame, with black bars and no world picking outside it.

Portable tests cover 1080p, 21:9, 4:3, odd ultrawide/narrow dimensions, DPI 100/125/150/175%,
UI 100/150%, 24/28-unit projection, rotated coordinate round trips, empty extents, original ABI
prefix/canary protection, absent services, invalid descriptors and wrong-thread calls. The game
selects its camera, aspect bounds and UI layout in Zig; the host has no game progression rules.

## Remaining acceptance

Run the [Windows display procedure](../../TwoD/WindowsDisplayAcceptance.md) on the exact head.
Retain initial high-DPI startup, decorated-window fitting on a smaller work area, true native
windowed/borderless restoration, UI clicks, manual resize and minimize/restore evidence. Beads
`nexora-4i1.1` tracks this gate; parent `nexora-4i1` remains open for full acceptance. Editor Play
integration and Cocoa borderless support are not delivered by this change. This does not complete
full-game art/UI, loading, color, audio, lifecycle/save or performance acceptance.

## X11 close-race repair

Hosted CI on f83021c3 exposed two Editor close-path regressions: `editor.linux_display_acceptance`
and `editor.linux_native_scene_preview` queried native attributes after external destruction.
RenderSurface now stops before metrics on close, treats a missing native handle as a lost window,
and rejects window requests after closing. Native X11 metrics scopes only expected BadWindow
errors for its own window, preserves failed outputs and restores the previous error handler;
unrelated protocol errors retain their handler or fatal behavior.

The new `window.x11_display_metrics_native` runs an actual isolated Xvfb server without a GPU.
Eight external-destruction cycles exercise query-before-pump and cleanup-before/after-pump;
live metrics, unchanged failed output and handler restoration pass. The same probe against the
preserved f83021c3 Window DSO fails with the same opcode-3 BadWindow as hosted CI. See the
[negative control](SP01-GameplayDisplay-Linux-2026-10-23/x11-before.log) and
[repaired probe](SP01-GameplayDisplay-Linux-2026-10-23/x11-after.log).

The recorded full gate is now **252 passed, 16 unavailable native GPU/input prerequisites skipped,
zero failures, 268 registered**; the new native Window case executed. Xvfb is supplied only to this
CPU-only test through NEXORA_XVFB_EXECUTABLE, without changing unrelated GPU gates' prerequisites.
Shipping consumer/export verification passed again. Remote graphics CI must pass before merging.
