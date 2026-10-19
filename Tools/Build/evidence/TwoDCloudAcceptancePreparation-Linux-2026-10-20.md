# 2D cloud acceptance preparation — Linux, 2026-10-20

Base: `69cb7b25` (`main`). Tasks: `nexora-j4q.2.8` and `nexora-j4q.7.12.1`.

## Selected integrated load

✅ Benchmark/runner accept `--objects 1000|5000|10000`, retaining the omitted-option 10,000
default. Explicit 5,000 selects the current-PC M6 load; 1,000 is functional smoke only.
Every selected object is updated/transformed and rebuilt into query shapes every frame.
The 256 VFX, two Sprite batches, 120 UI quads/commands, animation/fixed stepping, pool recycle,
Atlas codec/retirement and touched/freed 8 MiB reload allocation remain intact.

`twod.integrated_stress` runs 132 CPU frames per count, including four warm-up frames and one
reload/phase switch. Independent all-object geometry verifies query/sweep and culling. Mixed/sparse
visible counts including VFX are 1256/406, 5256/451 and 9956/451. Wrong-count summaries/traces and
invalid CLI values reject. CPU traces lack native counters/GPU samples and never accept native runs.

All six original cases, traces and individual timing verdicts remain retained. Only cases at or
below the selected count gate performance; correctness/device checks still apply to every case.
A regression retains two failing 10,000-object timing verdicts while the selected 5,000 performance
gate passes. The default historical gate still fails; corrupt extra-stress identity still rejects.
No historical evidence, 30 FPS/spike/resource budget, assertion or deadline was changed.

## Explicit Windows Vulkan loader distribution

✅ An imported `vulkan-1.dll` requires explicit `--vulkan-loader`, including development copies.
It is not an OS-DLL allowlist entry. Staging copies the specified loader, recursively checks its
imports and inventories its SHA-256 before atomic publication. Host/project/Zig acceptance helpers
forward the option; Vulkan-disabled binaries need none. Engine linkage and C/Zig ABI are unchanged.

`twod.project_export` covers missing/misnamed loaders, one loader shared by executable/module,
exact copied bytes/hash, unchanged sources, rejected transitive imports and no published partial
package. Windows imports are **mocked**, using non-executable fixture bytes. `cfgmgr32.dll` remains
an OS dependency, matching the pinned upstream [Vulkan Loader Windows linkage](https://github.com/KhronosGroup/Vulkan-Loader/blob/v1.3.296/loader/CMakeLists.txt).
The loader does not provide a Vulkan ICD/GPU driver; redistribution obligations remain with the
distributor. Existing frozen packages and recorded hashes were not edited.

## Validation

Tools: GCC 14.2, CMake 4.4.4, Ninja 1.13.2, Clang 19.1.7, clang-format 23.1.3. Tools and
X11/Vulkan development files use an external writable prefix. No generated output or machine
paths are committed. Default presets: Development/Modular and Shipping/Minimal/Monolithic;
Slang, graphical Editor and Zig features were not enabled for this local gate.

| Command/check | Result |
| --- | --- |
| `cmake --preset linux-development` | PASS |
| `cmake --build --preset linux-development -j 4` | PASS, 574 steps |
| `ctest --preset linux-development` (complete final run) | PASS: 204 registered, 193 passed, 11 native Vulkan skips, zero failures; 126.41 s |
| `cmake --preset linux-shipping` | PASS |
| `cmake --build --preset linux-shipping -j 4` | PASS, 102 steps |
| Shipping host fixture below | PASS: storage/restart/corruption/rebinding/font admission; seven-file inventory, development_test_copy=false |
| Shipping 5k benchmark below | PASS: 132 updates, one reload, 256 VFX, zero live fixture bytes and one balanced allocation/free |
| Touched C++ clang-format, Python byte-compilation, `git diff --check` | PASS |

The first full run failed only `build.v2_production_toolchain` because this new host lacked
`clang++`: 176.29 s, one failure and the same 11 skips. After installing Clang, its isolated
check passed, then the complete unchanged-source gate above passed. No test was relaxed.

Shipping validation commands (reports/package retained under the ignored artifacts directory):

```bash
python3 Tests/TwoD/HostAcceptance.py --app build/linux-shipping/Apps/TwoDPlayer/NexoraTwoDPlayer --module build/linux-development/Tests/TwoD/libNexoraTwoDHostFixture.so --shipping --output build/linux-shipping/artifacts/cloud-host
build/linux-shipping/Tools/TwoD/NexoraTwoDBenchmark --integrated --objects 5000 --frames 128 --reload-every 120 --report build/linux-shipping/artifacts/5k.csv --frame-trace build/linux-shipping/artifacts/5k-frames.csv
```

The independent Development HostFixture consumes only C ABI headers, without engine DSOs.
The exported runtime is Monolithic Shipping. Synthetic fonts and headless audio configuration
do not establish visual or audible acceptance.

## Acceptance boundary

Windows startup on a loader-free machine, physical Windows devices/graphics, the revised 5,000
native screens/30-minute soaks and independent clean-host acceptance are **NOT RUN**. Regenerate
and hash a new Windows package before those checks. No physical timing/GPU acceptance is inferred
from CPU results or mocked imports. M6/M7, physical stall investigations and parent acceptance
tasks remain open; 9/11 (82%), P0 6/8 and Editor counts are unchanged. The root GitHub README was
reviewed and needs no update.

## Local log SHA-256

Logs are retained outside the checkout; hashes distinguish the initial failure and final gate.

- `configure.log`: `5c08834021a6d8c30862bdbb8299b8038aff248413ec5db77ee75a93f8047ca3`
- `build.log`: `434edf51706141df6b166064a3e29e58989ad37dd460d18e467dd0588afd9a35`
- `ctest.log`: `f03e779d8f2cae12ede6beff6208f591d1b318a82f2ec03559a7e49c6cdb0dca`
- `ctest-final.log`: `abf466ff028b7ad491f78544bfe3d540622484cdadcc2726f76afe7a910a5a8f`
- `shipping-configure.log`: `aa598ece50c3c9ac279ff84f81b994a7ab1ecaba642ce61c2c383e629064502c`
- `shipping-build.log`: `01675069f08fd538e4d59206942f838339a7822cdd179ccc08548d6d3d212d87`
- `shipping-host.log`: `0721ad222ededc3dd506c94d6b3070ea405d2b3c0b420bf2122af5324ff0c236`
- `shipping-5k.log`: `0b048f07193707615343bc771eab92afe8588da28f8dfaac56ca7b7a8882263e`
