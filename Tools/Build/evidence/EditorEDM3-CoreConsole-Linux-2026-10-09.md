# ED-M3 Core Console producer: Linux evidence

Date: 2026-10-09. Beads task: `nexora-rd7.3.2`. Final integration base: PR #454
merged as `f47c6f15` after 36 hosted checks, including accepted GPU capture PR #453
and native GPU timing PR #452. This is a supporting Console producer slice;
complete Runtime/build producers, native debugger attachment and ED-M3 acceptance remain open.

The graphical owner now starts a real Core asynchronous producer and polls owning incremental
snapshots before each UI frame. Editor diagnostics and actual V3 gameplay callbacks enter Core;
worker threads retain no UI/World callback or borrowed message. Shutdown unloads gameplay before
the Play World, drains Core, then forwards the final records. Native process evidence reports only
producer identity and aggregate counters, including retained gameplay records.

Core bounds both pending and retained record counts to 4096, categories to 256 bytes and messages
to 16 KiB. Valid NUL-free UTF-8 is compacted before admission. Sequence assignment follows successful
publication, uint64 exhaustion rejects further writes, and saturated rejection counts survive
restart. Stop drains accepted records; concurrent Write and Stop account every attempted write.
Flush waits for its captured accepted watermark. SnapshotSince copies already-consumed records
without waiting for later traffic; the GUI does not flush or join each frame. Lifecycle operations
remain serialized by the owner, and producers finish before destruction.

The adapter preserves source, category, producer timestamp, message and severity (Core Debug maps
to Console Trace). It accounts upstream rejection deltas and unread ring gaps once, independently
of Console rejection/eviction. Polling, Clear and presentation Pause do not replay old records.
Raw gameplay log admission rejects invalid levels before narrowing, null nonempty pointers,
oversized data before reading, NUL and malformed UTF-8; valid messages are never truncated. V3 uses
a 4-KiB budget and the existing attached V2 bridge uses the Core 16-KiB budget. Pointer ranges within
the admitted budget remain the trusted in-process ABI caller's responsibility. Stable C/Zig layouts
are unchanged; additive C++ APIs and the explicit EditorApp Core dependency require consumer rebuilds.

Host: Linux x86_64, GCC 14.2, CMake 3.31.6, Slang 2026.18, clang-format 19, Mesa lavapipe/Xvfb.
Development is Modular with graphical shell, Slang, Zig and Showcase enabled. Minimal Shipping is
Monolithic with Editor disabled and native Presentation enabled. Native gates run serially on this
four-CPU cloud host.

```bash
source /workspace/.nexora/env.sh
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
ctest --preset linux-development -R '(async_log_stream|core_console_ingress|console_display|gameplay_host_bridge|play_gameplay_module|linux_native_game)' --output-on-failure
cmake --preset linux-shipping
cmake --build --preset linux-shipping
git diff --check
```

Before final dependency integration, complete Development configure/build/test passed **188/188**,
no skips, in **348.79 seconds**, and Minimal Shipping configure/build passed. The refined focused
gate passed **9/9**, no skips, in **11.81 seconds**, including the added concurrent Write/Stop test.
After integrating accepted PRs #453/#454, the final serial Development configure/build/test gate
passed **194/194**, no skips, in **355.57 seconds**. Minimal Shipping configure/build passed.
`git diff --check` and the combined native fixture's Python syntax check passed.

Portable tests cover exact/over byte and pending limits, sequence exhaustion, saturation, disabled
ingress, compact ownership, four concurrent producers, incremental copies, repeated Start/Stop and
concurrent Write/Stop. Actual V3 static module callbacks cover all six log levels, invalid raw inputs,
exact UTF-8 budgets, Stop/Destroy delivery and Editor World isolation. 1x/2x real pointer events drive
Console source/severity filters, presentation Pause, Clear, detach/rebind and unread producer loss.
The native Linux Game View and real dynamic gameplay fixture require actual Core-to-Console shutdown
evidence, with zero reported drops and retained gameplay records when a module is loaded.

Windows/macOS are not executed locally; hosted platform checks are tracked in the PR and Beads.
