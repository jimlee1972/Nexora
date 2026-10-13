# ED-M6 live retained-mesh memory observations

Date: 2026-10-13. Beads: `nexora-62u.2.9`. Supporting live Profiler channel; full ED-M6/physical
0-of-8 acceptance, separate mesh-memory interchange and whole-device instrumentation remain open.

## Source, scope and compatibility

After successful Present, the application copies public Presentation immutable geometry residency
diagnostics: actual native buffer allocation bytes and owning CPU comparison-key payload bytes.
Both are available/unavailable together; zero is valid. Scope is retained mesh resources owned by
the current process-local native surface, including resources retained across project switches.
Native counts include backend allocation requirements/allocated size; CPU payloads exclude heap
allocator overhead. Neither metric is process RSS, texture/frame-upload accounting or total VRAM.
No native handle, renderer callback or OS/native read enters Editor widgets/core.

ProfileSession::ObserveMeshMemory rejects zero domains and partial metric pairs without disturbing
prior state. New valid domains reset this stream only, including while paused. Capturing samples
are throttled to 250ms and strictly monotonic steady time; missing observations create gaps and retain
observed peaks. Fixed history bounds are min(frame_capacity,600), with saturating independent drops;
capacity zero still owns latest/peak values. Clear resets measurements/origin without resuming Capture
or releasing renderer resources. The UI copies latest fields, consumes history spans immediately and
plots two byte series over elapsed time, with explicit source/scope and missing-line breaks.

The public C++ ProfileSession layout changes through fixed storage; consumers must rebuild. Stable
C/Zig/scene/module schemas and dependencies are unchanged. Existing wall/RSS/GPU capture schemas,
export/import controls and static snapshots keep their original scope; this new channel is live-only.

## Regression evidence

- Portable tests cover source/pair validation, clock/throttle/endpoints, paired gaps and zero, pause/
  Clear/domain changes, fixed600/zero capacity, evictions/peaks and unrelated CPU/GPU/wall histories.
- Actual1x/2x Profiler Capture/Clear/Resume input verifies copied observations, zero/unavailable and
  detached/new-owner states. Widgets cannot increment observations; Clear legitimately resets the
  counter. A preliminary combined assertion incorrectly treated that reset as a read; split guards
  corrected the test, and failed preliminary results are excluded.
- Existing real Vulkan material fixture now retains mesh geometry and checks copied actual positive
  native/key bytes in Profiler history while preserving all original assignment/reimport/Undo/reopen
  pixel guards. Drain validates zero released ownership and available-zero/unavailable observation.
- Final targeted core/native/widget tests3/3 passed0.88s (native0.85s); no skipped fixture is accepted.

## Validation

Final source integrates main836674d2 (accepted material-clear PR584 and standing authorization).
The existing task runtime provides GCC14.2, Slang2026.18, clang-format23, X11/Xvfb and software Vulkan;
Development enables graphical Editor/SDK/Slang with headless mode off. No machine preset/build output
is committed.

- `cmake --preset linux-development`: passed.
- `cmake --build --preset linux-development`: passed (397 actions after targeted preparation).
- `ctest --preset linux-development --output-on-failure`: **250/250**, no failures/skips, **839.99s**.
  Mesh-memory core0.00s, actual1x/2x widgets0.02s and real Vulkan allocation/pixel/drain fixture0.74s.
  Existing process-memory/GPU/CSV/JSON and all graphical regression guards remain.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed (5 actions),
  Minimal Monolithic/native Vulkan, Editor disabled. Development above exercises the new public C++
  layout/exports; this smoke does not claim a shipping Editor application.
- Final targeted core/native/widgets: 3/3,0.88s. Failed preliminary GUI Clear-counter assertion is
  excluded from acceptance.
- Changed C++ `clang-format --dry-run --Werror`, `git diff --check`, synchronized EN/zh-TW roadmap,
  changed Markdown links and `python3 Tools/Build/TestDocumentationCI.py`: passed (16 tests).
  Root README remains concise/unchanged; milestone acceptance remains0/8.

Final-head hosted results are recorded in the PR/Beads before merge; pending checks are not counted
as passed. Linux software Vulkan/Xvfb evidence does not imply physical device-memory/performance
acceptance.
