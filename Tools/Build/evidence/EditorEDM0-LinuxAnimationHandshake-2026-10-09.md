# Linux cloud native animation handshake

Date: 2026-10-09. Beads: `nexora-nqn.7`. Initial PR #452 head:
`5dd967f278dc38c032c668cf4cde5735d81dc205`. This shared cloud acceptance-fixture work
does not accept physical-display/GPU or complete Editor milestones.

Two hosted Linux jobs failed the existing `showcase.linux_vulkan_interaction` at that head:

- [37929413446/job 113816350575](https://github.com/jimlee1972/Nexora/actions/runs/37929413446/job/113816350575)
  exceeded the aggregate 90-second limit (90.11 s); 183/184 tests passed.
- [37929460832/job 113816520449](https://github.com/jimlee1972/Nexora/actions/runs/37929460832/job/113816520449)
  failed after 79.71 s: `Native image did not settle within five seconds: courtyard-animated.png`;
  183/184 tests passed. Showcase leaves the new GPU profiling option disabled.

The old fixture sent Resume, slept 0.6 seconds and sent Pause before observing animation. Slow
software rendering can consume both events before an animated frame is presented. The corrected
sequence acknowledges changed presented pixels within the existing five-second comparison deadline
before Pause. A capture failure still queues Pause; success then requires repeated stable changed
frames within the existing 15-second settling bound. Exact reset/effect restoration, all-room,
native-count and resize assertions remain intact. The whole nine-room test receives a bounded
180 seconds, independent of the unchanged per-picture deadlines.

Deterministic normal/optimized Python regressions use a delayed two-second presentation clock.
The old sleep batches both toggles and leaves the original image; the corrected sequence observes
motion before Pause and proves stable paused presentation. Never-presented motion fails at five
seconds; capture errors still queue Pause. Tests use no real sleeps or X server.

```bash
source /workspace/.nexora/env.sh
python3 Tests/Showcase/AnimationHandshakeTests.py
python3 -O Tests/Showcase/AnimationHandshakeTests.py
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
ctest --preset linux-development -R '(linux_animation_handshake|linux_vulkan_interaction|gpu_timing|gpu_profile|vulkan_scene)' --output-on-failure
cmake --preset linux-shipping
cmake --build --preset linux-shipping
git diff --check
```

Both deterministic modes pass four regression cases each. The real native/GPU focused gate passed
**8/8**, no skips, in **66.41 seconds**. The required complete Development configure/build/test
gate passed **186/186**, no skips, in **354.81 seconds**. Minimal Shipping configure/build and
`git diff --check` passed. Hosted checks must pass on the final PR head before merge. Linux uses
Xvfb/lavapipe, not physical GPU timing calibration.
