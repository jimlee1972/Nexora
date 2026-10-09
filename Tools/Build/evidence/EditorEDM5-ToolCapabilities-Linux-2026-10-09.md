# ED-M5 versioned owning capability metadata — Linux cloud evidence

The discovery registry now validates complete owning schema/interface-one metadata before
publication. Provider/capability/document/contribution IDs have bounded stable ASCII syntax;
displayed UTF-8 titles and diagnostics reject control characters. Non-implemented states require
diagnostics. Known declared permissions and positive bounded document/preview/operation budgets
are explicit. Declaration grants no permission and enforces no native allocation policy.

Admission caps discovery at 128 tools, IDs at 128 bytes, titles at 256 bytes, reasons at 1024 bytes,
each identifier list at 16 entries, and total text at 4096 bytes. Document/preview budgets are at
most 16 MiB/128 MiB and pending operations 1..64. Invalid admission preserves existing metadata.
Explicit removal releases discovery capacity; snapshots own all copied values across removal.
Borrowed lookup/span results expire on mutation. Calls are serialized on the authoring thread.

Original four-field aggregate callers retain defaults. Empty contributions represent discovery
only. No plugin callbacks/handles or source payload enter this registry; actual host authorization,
preview lifetime, production reference-plugin workflow and graphical fallback remain open.
Public C++ consumers rebuild; C/Zig gameplay ABI and full Editor milestone counts remain unchanged.

Actual contract tests cover unknown versions/states/permission bits, invalid/oversized UTF-8 and
IDs, missing diagnostics, duplicate document/contribution names, exact individual/total/resource/
registry limits, sorted lookup, removal/capacity reuse and copied fallback observations.
Both retained Xvfb helpers and their default-reset negative control are included; scene-file
acceptance explicitly restores Scene focus after Open without dropping edit/save/Undo assertions.

~~~sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON \
  -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
~~~

GCC 14.2/CMake 3.31.6/Ninja/Slang 2026.18/Zig/Xvfb/Mesa software Vulkan were used. The complete
417-step graphical build passed. Focused new metadata plus existing preview tests passed **2/2
in 0.06s**. Full graphical/native gate passed **207/207**, zero skips, **401.24s**. Minimal Shipping
passed its **74-step** build. Touched C++ formatting and diff checks pass. Linux display CI
requires the metadata test to be registered. Fresh hosted CI remains required before merge.
This does not certify physical display behavior, production specialized-tool acceptance or full ED-M5.
