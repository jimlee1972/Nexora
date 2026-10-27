# Approved Nexora branding and Windows startup

Scope: approved reference `1046ab60`, maintainable path SVG sources, transparent production
wordmark/n PNGs, seven-size Windows ICO and real-stage native Windows Editor startup.
The integrated base includes PR #724 icon actions/Transform UI; those controls are retained.
Full graphical Editor milestones remain 0/8. No former developer-PC build is used.

## Contracts and review

The original raster reference is retained beside path-based SVG sources. The restored vector
silhouette follows the approved wordmark, with flat #47D9F2/#F5F5F5 colors, the shoulder gap
and cyan x segment. Independent n exports have a transparent counter and safe canvas margins.
The review sheet in [exports](../../../Assets/Brand/Export/nexora-brand-review.png) shows white,
dark and black backgrounds; it is not the production icon. The Windows native test retains an
actual rendered screenshot as `Tests/Editor/editor-startup-splash.bmp` in Actions artifacts.

`StartupSplash` owns its native UI thread; engine initialization remains on the application
owner. Synchronous stage updates precede real settings/project/index/device/Scene/workspace work.
The first admitted successful EndFrame closes and joins the splash. Failure/exception scope
exit also closes and joins it, preserving existing diagnostics. No timer or artificial delay is
used. CLI/help/build paths create no splash; the original GUI subsystem/Unicode entry stays.
The splash adopts the native host's per-monitor V2 policy before the first HWND and becomes an
owned non-activating tool window above the initializing main window. Before an owner exists it
is temporarily topmost, protecting the graphics-stage splash when the native host shows its
empty window before device creation. Owner handoff immediately clears global topmost; ownership
then preserves above-owner ordering through the first successful frame. No timer/delay is used.

`editor.brand_assets` checks actual RGBA alpha/AA/borders/colors/counter, all seven ICO payloads
and ZIP hashes with Python stdlib. `editor.windows_startup_splash` checks actual logo pixels,
DPI, ownership, idempotence, exception cleanup and both window icons. It also shows/activates a
larger white host before handoff, checks actual desktop Z-order and composed screen logo pixels,
and verifies unchanged focus plus topmost only before handoff. Separate pre-owner/handoff BMPs
are retained; failure cleanup is checked on both sides of handoff. Windows GUI entry checks
real PE brand resources, redirected Unicode CLI, no console and a real failed project-open cleanup.
After Unicode relocation with a restricted runtime PATH, the package requires exact embedded
resource bytes, lifecycle order and actual main-window ownership with topmost cleared before
scene initialization. Runtime never needs a source-relative image path.

## Validation

Final command results and Windows receipt links are recorded by the associated delivery PR.
The required Linux Development configure/build/full CTest and Shipping linkage build must pass
before publication. Hosted Windows native tests/package remain explicit gates, rather than being
inferred from Linux success. Physical multi-monitor/DPI/taskbar and independent clean-host
acceptance remain open. An older Editor ZIP is not evidence for this change.

### Cloud Linux result

✅ CMake 3.31.6 / Ninja 1.11.1 / GCC 13.3: `cmake --preset linux-development`
with graphical shell enabled, then `cmake --build --preset linux-development`, passed.
✅ The final unfiltered `ctest --preset linux-development` completed all 391 registered cases:
384 passed, seven conditional skips, zero failures, 797.80 seconds. The seven Vulkan recording
cases require optional Slang artifacts not enabled in this local configuration; hosted CI's
Slang-enabled configurations remain required. No test sizes, assertions or deadlines were relaxed.
✅ `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping` passed.
✅ Transparent brand/ICO/ZIP admission, six package-admission tests, 16 documentation-routing
tests and changed-Markdown link/structure checks passed.

Earlier environment failures are not counted as passes: incomplete generated outputs were
recreated using the stable cloud toolchain, clang++ was installed for the existing reflection
producer, and execute permissions were restored on three valid generated ELF test binaries.
The four affected cases passed their focused rerun, followed by the successful full traversal
above. The source implementation and existing UI/scale/soak assertions were retained.

Windows verification is provided by the required hosted native/PE/lifecycle/package workflows,
with their final run/receipt linked in the delivery PR. Linux success alone is not Windows evidence.

### Complete Windows delivery gate

Head `debbe9a7` passed hosted Windows branding/lifecycle/GUI-entry (3/3) and relocated
CLI/DX12 startup. Its approximately 6.4 MB Editor-only ZIP is historical startup evidence,
not the final delivery: it omitted the previously accepted private Build/Play dependencies.

The corrected delivery workflow builds native ProjectPlayer with Zig 0.14.0 and stages its generated
GameRuntime.profile, CMake 3.31.6, Ninja 1.12.1, compiler/module resources and licenses through the
existing complete-tool packager. Only NexoraEditor.exe is public at the root; Runtime and Tools
remain private dependencies, without extra 2D apps or a Nexora/game source checkout.
The actual ZIP is extracted and every inventory hash admitted, then relocated script compilation,
source-hidden native game execution (100 frames/three commits) and extracted Editor Play
(1,200 frames/three commits/native draws/source conservation) must pass. Final Windows run links,
source and ZIP hashes are recorded in PR #725 after that new-head verification succeeds.
