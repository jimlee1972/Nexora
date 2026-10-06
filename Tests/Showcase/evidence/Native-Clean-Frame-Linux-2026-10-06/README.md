# Native clean-frame synchronization: Linux evidence

The original PR #378 Linux Development failure accepted a stable image that still contained native navigation. Its diagnostics and selected room differed from the earlier UI reference, so byte inequality plus repeated frames falsely established a clean baseline. The following bloom-off/restored captures were identical clean images and could never restore that UI baseline. The actual CI screenshots, artifact SHA-256, run/job/head and precise failure excerpt are retained.

The capture helper now checks the opaque navigation backgrounds at their viewport-scaled positions, learning their presented color from the actual UI reference. At both F4 transitions it requires that navigation to disappear before accepting repeated frames. Ordinary camera settling still accepts its own image reference. The 15-second settling and five-second comparison deadlines, exact pixel restoration and bloom change assertions remain unchanged.

`ReplayCleanFrameRegression.py` replays the original failure, rejects stable changed UI, accepts repeated clean frames, and verifies that UI which never disappears fails at the original deadline. Full Linux configure/build and 97/97 tests pass in 118.49 seconds with Khronos core/synchronization validation. The corrected gate also passes isolated Shipping acceptance; corrected native bloom captures change on disable and restore exactly. Validation uses the reflection renderer plus separately pending ceramic inlays, frozen at `2c7d68a0c8bfe4393f8051e4c94356e34b93cd46`. This test synchronization update changes no renderer, shader, package executable or linkage. Native acceptance records its actual build identity.

Run the retained regression from the repository root:

```bash
python Tests/Showcase/evidence/Native-Clean-Frame-Linux-2026-10-06/ReplayCleanFrameRegression.py
```

Reference parity and physical-display acceptance remain open (VIS 5/7).
