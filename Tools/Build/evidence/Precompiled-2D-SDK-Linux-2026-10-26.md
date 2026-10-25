# Precompiled Windows 2D SDK: Linux supporting validation

This record covers portable tools and the full Linux regression gate. Windows producer/consumer
and public Release evidence are retained in the SDK workflow artifacts and PR discussion.
No Windows execution is inferred from these Linux commands.

## Results

- `python -m unittest discover -s Tools/SDK/tests -v`: 3/3 passed, including bounded PE normal
  and delay imports, Unicode paths, truncated headers, invalid RVAs/names/terminators and
  Shipping rejection of Editor/engine/unknown/missing CRT imports.
- `cmake --preset linux-development`, `cmake --build --preset linux-development`,
  `ctest --preset linux-development`: 301 registered, 284 passed, 17 skipped, 0 failed;
  CTest completed in 428.46 seconds. The SDK portable PE test and existing exporter/script-project
  regressions actually ran. Skips require unavailable native display/GPU prerequisites.
- `cmake --preset linux-shipping`, `cmake --build --preset linux-shipping`: passed.
  `ctest --preset linux-shipping`: no tests registered, as expected with Shipping `BUILD_TESTING=OFF`.
- A real Windows candidate ZIP was independently checked on Linux against the game resolver:
  archive digest, manifest identity, all 32 payload hashes, download receipt and offline reuse passed.
  This validates archive consumption only, not execution of its Windows binaries.

Local configure used existing external Vulkan headers/library and X11 headers/library; graphical
Editor and Slang were disabled, Zig 0.14.0 enabled. The first configure lacked X11 header discovery
and failed; explicit local SDK paths corrected discovery. No assertion or native acceptance was
weakened. The Windows producer initially failed because PowerShell split the unquoted descriptor
argument; quoting repaired that CI command. Original failed workflow runs remain available.

## Acceptance boundary

The fixed SDK requires fresh isolated Windows consumer checks before publication, including an
asset-bearing Atlas/Clip fixture and game integration. Native DX12/zero-argument GUI/manual close,
physical DPI/input/audio, clean retail Windows and complete game/target-machine acceptance remain
separate NOT_RUN gates. M6/M7 are unchanged.
