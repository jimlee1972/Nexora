# Zig script SDK migration: Windows developer-host evidence

Scope: engine-owned build/ABI wiring with an external script-only game. This is not clean-host,
Editor UI, physical input/audio, full game content or final visual acceptance.

## Local results

- Zig 0.14.0 `zig test --dep nx -Mroot=Tests/TwoD/GameScriptAdapter.zig
  -Mnx=Bindings/Zig/nexora.zig` with explicit external cache paths: 4/4 passed.
- Engine `Tools/Zig/build.zig test` with the external OnmyojiSurvivors script: 4/4 passed
  (one entry-contract test and three authored tests); ReleaseSafe dynamic module build passed.
- `python Tests/TwoD/GameScriptProjectTests.py --cmake <cmake> --zig <zig>`: external Unicode/space
  project builds and executes entry/script tests; authored sources remain byte-identical. Parent
  traversal rejects. The strengthened test additionally requires a deliberate script-test failure
  to fail the build. macOS uses the existing host-linker workaround and needs CI confirmation.
- Consumer `cmake --preset windows` and `cmake --build --preset windows`: Shipping runtime,
  ReleaseSafe module, 4/4 Zig tests and official exporter completed with exit 0.
- Two independent packaged processes, each `--frames 600`: exit 0, 600 ticks, identical diagnostic
  hash `4164899406428918877`, balanced allocations.
- Packaged process `--frames 180 --native --backend dx12`: exit 0, 180 actual native frames,
  balanced allocations. Windowed client was 1779x1001 after Windows adjusted the requested size;
  this is presentation/startup regression, not new 1920x1080 visual acceptance.
- Zero-argument launch from a foreign working directory produced a visible window; normal window
  close exited 0. Bounded native presentation is proven by the separate 180-frame run above.
- Package SHA-256 inventory verified; no Zig sources or SDK project manifest in the exported package.
  No `build.zig`, `bootstrap.zig` or `main.zig` remains in the authored consumer Game directory.

The initial consumer package was built against PR #709 head `b7fcbb9168244d45175a2f8bfd5e8078cb978406`.
Raw consumer logs, commands, player reports, checksums and initial window-size assertion evidence
are retained under `build/sdk-migration/` in the external game checkout. The first verifier assumed
an exact windowed 1080p client and rejected 1779x1001 despite 180 native frames; the startup verifier
was corrected to test positive dimensions and retain observed size. The first report is preserved.

## Remaining gates

The Linux preset is disabled on this Windows machine. Full Linux development build/CTest,
Linux Shipping and other selected checks must pass on the exact PR head before squash merge.
Editor project-browser/build-button integration is future work. No unrelated M6/M7, SP, clean-host
or product acceptance gate is closed by this supporting change.
