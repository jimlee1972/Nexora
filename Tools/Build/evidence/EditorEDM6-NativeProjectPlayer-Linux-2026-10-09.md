# ED-M6 native StaticView ProjectPlayer — Linux cloud evidence

Beads: nexora-62u.1.5. Initial reconstructed source integrates accepted main
bac7247be152c9a85e7eb757fbc0dee602417f90. Full ED-M6 remains open.

## Delivered behavior and limits

Optional NEXORA_ENABLE_PROJECT_PLAYER_NATIVE adds --run-package to the existing production
ProjectPlayer; verification-only remains the default. Use only public owning Runtime package and
Presentation APIs, with no Editor, SDK, Showcase, source-content or gameplay dependency.
Prepare an owning CPU native candidate with shared geometry/material deduplication and exact affine
row-major float conversion from the package's column-major doubles. Preserve tangents/scalar PBR,
legacy shader IDs and inactive opaque components. Native data never borrows source asset storage.

Admission rejects the complete scene instead of silently omitting instances: 1..4096 instances,
65,535 combined vertices, 1,048,576 combined triangle indices, 63 scalar materials plus neutral
slot zero, finite invertible native transforms, PBR tangent orthogonality and public material/batch
validation. A conservative 8 MiB upload estimate does not promise native allocation/alignment success.
The lowest-ID authored camera must provide a valid view; resize recomputes projection. The lowest-ID
light supplies neutral-white intensity/orientation, with explicit Presentation defaults if absent.
There is no invented fallback camera, authored light color or arbitrary gameplay execution.

RenderSurface owns native acquisition/draw/presentation/resize/drain; device loss and unsupported
backend/admission are visible failures. Successful RENDERED_STATIC_VIEW JSON requires actual drawn
presentation and successful drain; it reports actual draw/present/resize and software-rasterizer
observations. The requested backend is labelled as requested, not fabricated observed identity.
Frames are bounded to 1,000,000 successful presents when explicitly supplied, with 30-second stall
failure. Without a frame limit, a real native close request ends the session. Invalid options exit 2;
native/data failures exit 1 and print no success report. No package/source file is modified.

## Real acceptance

Actual AssetCooker/codec/package fixtures test exact 4096/4097 instance, 63/64 scalar palette,
65,535-vertex and 1,048,575-triangle-index boundaries and aggregate two-mesh overflow, native
mirror/shear matrices, copied geometry/material ownership after package destruction, resized
authored camera projection, missing camera, excessive PBR light intensity and inactive payloads.

The Linux test creates a real cooked package at a Unicode/spaced path, starts the standalone player
with Vulkan under Xvfb and reads actual window pixels. Both red and green scalar materials appear
before and after 960x640 -> 800x600 resize. WM_DELETE_WINDOW produces a successful drain report with
positive native draw/present counts and resize generations. A separate run presents exactly four
frames. Invalid/duplicate options and corrupt packages fail, and original package bytes stay equal.
Only generic Xvfb/WM_DELETE/XImage test helpers are reused; no Editor process/project/SDK is involved.

## Linux gates

GCC 14.2, CMake 3.31.6, Ninja, Slang 2026.18, Zig, Xvfb and Mesa Vulkan software rendering.
No physical GPU/display or other-platform native pixel acceptance is claimed.

~~~sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON \
  -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
cmake --preset linux-shipping -B build/linux-native-player-shipping \
  -DNEXORA_SHIPPING_PROFILE=Full -DBUILD_TESTING=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON \
  -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON -DNEXORA_ENABLE_EDITOR_SDK=OFF \
  -DNEXORA_FEATURE_EXAMPLE_PLUGIN=OFF
cmake --build build/linux-native-player-shipping --target \
  NexoraProjectPlayer NexoraNativeProjectPlayerTests NexoraProjectPackageTests -j4
ctest --test-dir build/linux-native-player-shipping \
  -R '^runtime.(native_static_view|linux_native_static_player|static_project_player_cli)$' \
  --output-on-failure
cmake --preset linux-development -B build/linux-verifier-only \
  -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=OFF \
  -DNEXORA_ENABLE_WINDOW_PRESENTATION=OFF -DNEXORA_ENABLE_SCENE_RENDERING=OFF \
  -DNEXORA_ENABLE_EDITOR=OFF -DNEXORA_ENABLE_EDITOR_SDK=OFF -DNEXORA_FEATURE_EXAMPLE_PLUGIN=OFF
cmake --build build/linux-verifier-only --target NexoraProjectPlayer NexoraProjectPackageTests -j4
ctest --test-dir build/linux-verifier-only -R '^runtime.static_project_player_cli$' --output-on-failure
~~~

Focused real producer/CLI/native cases: 3/3, 4.51s, after 63 build steps.
Full graphical Development: 205/205, zero skips, 413.70s, after full 352-step build.
Minimal Shipping: 74 steps. Full Monolithic native Shipping with Editor/SDK/Example OFF:
62 steps; actual focused tests 3/3 in 4.24s, including native pixels/resize/close/drain.
Verifier-only/Window-OFF/scene-OFF/SDK-OFF: 53 steps and real CLI test 1/1 in 0.13s;
--run-package returns unavailable/exit 2 and no success output. ldd confirms no Window/Presentation/
Editor libraries in the verifier and only system X11/Vulkan/runtime libraries in Monolithic native.
Touched C++ clang-format, Python syntax and git diff checks pass.

Existing Windows/macOS/Linux desktop CI enables this path and CPU tests; Linux display CI requires
both native tests to be registered and actually runs/retains their logs. Full milestone count/root
README stay unchanged; compilation, gameplay, graphical Build/deploy and physical acceptance stay open.

Enabling the producer/Player CLI integration on Windows exposed a previously narrow-argv fixture:
editor.static_project_export_cli and its optimized variant could not write their Chinese filename.
The existing producer fixture now uses a shared runner with Windows wmain native-wide paths and
MinGW -municode; Unicode fixture paths and all exact-boundary producer assertions remain enabled.
The repaired source passes full graphical Development again: **205/205**, zero skips, **402.62s**.
Minimal Shipping rebuild passes; Full Monolithic native Shipping rebuild passes and its actual
CLI/native admission/pixels/resize/close/drain tests pass **3/3 in 4.47s**. Fresh hosted Windows
acceptance remains required before merging the repaired head.
