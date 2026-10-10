# Session diagnostic privacy — Linux acceptance, 2026-10-10

## Scope and ownership

Source starts from accepted main 9d799cd28a6628085b04ae50077614e3da33f11c.
The native host owns a default-off TelemetryConsent; the optional GUI borrows it only
for a serialized frame call, retains copied scope/control metadata, and revokes consent
on project identity change or detachment. Closing explicitly clears the queue. No consent
or queue is serialized, and no network/storage endpoint or environment configuration is read.

Ctrl+Alt+T and Settings expose the actual checkbox, retained-count inspection and Clear.
The native producer admits only the fixed `frame.presented` operational label, once per
60 successfully presented frames while consent is enabled. It never passes paths, source,
entity names, commands, arguments or credentials to the queue. The UI displays only this
exact allowlisted label, clips visible safe records and reports excluded-record counts;
unknown generic primitive event text is never rendered. Opt-out immediately forgets all
records; Clear preserves current consent but deletes records; re-enable starts empty.

The portable generic TelemetryConsent remains a caller-controlled bounded primitive, not a
universal redactor. This slice does not deliver signed-extension verification, arbitrary
telemetry redaction, persistence or transmission. The complete milestone remains **0/8**.

## Actual acceptance

- `editor.diagnostic_privacy`: real controls at 1x and 2x DPI, keyboard shortcut,
  default-off admission, opt-in, 1,024-event saturation, unknown-text exclusion, clear,
  opt-out deletion, re-enable, project/detached scope reset and a new default-off model.
- `editor.linux_diagnostic_privacy`: actual Xvfb/Vulkan Editor keyboard opt-in, retained
  record, Clear, opt-out, re-enable, explicit existing dirty-bootstrap Discard on close,
  restart default-off, and exact whole read-only project file preservation. All captured
  Vulkan VUID/SYNC errors fail acceptance; diagnostics contain only fixed labels/counts.
- Focused pair: **2/2 passed in 4.57s**; the full native repeat passed in 4.70s.
- Full graphical/Slang/Zig/native-player Development: **223/223 passed in 541.73s**, zero skips,
  after 384 incremental build steps. Minimal Shipping build passed, 74 steps.

~~~sh
source /workspace/.nexora/env.sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_BUILD_SHOWCASE=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON \
  -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping \
  -DFETCHCONTENT_SOURCE_DIR_NEXORA_VULKAN_HEADERS=/workspace/work/nexora-editor/dependencies/nexora_vulkan_headers-src
cmake --build --preset linux-shipping -j4
~~~

Shipping's initial fresh dependency download failed GitHub authentication, before compilation.
The retry used a clean local Vulkan-Headers upstream v1.3.296 source at commit
29f979ee5aa58b7b005f805ea8df7a855c39ff37, independently checked against the peeled tag;
no repository dependency pin or network policy changed. Shipping intentionally excludes Editor;
this verifies profile/link compatibility, not execution of the Editor privacy UI in Shipping.
GCC 14.2/CMake 3.31.6/Ninja/Xvfb/xdotool/software Mesa Vulkan were used. Linux cloud tests
are not physical Windows/macOS/IME/DPI or screen-reader acceptance. Fresh published-head
hosted checks remain required before merge.
