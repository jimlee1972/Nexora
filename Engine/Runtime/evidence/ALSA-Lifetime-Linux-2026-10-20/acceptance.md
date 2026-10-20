# ALSA output configuration lifetime — Linux, 2026-10-20

Base: `3b9fffcc` (`main`). Task: `nexora-6og`.

## Reproduction and ownership

Debian 13, GCC 14.2.0, ALSA library/data 1.2.14-1; no default sound card.
The original `GameplayAudio` dynamically loads ALSA and calls `snd_pcm_open`.
That function obtains the process-global configuration through `snd_config_update_ref`,
then drops its temporary reference with `snd_config_unref`. The global owner still retains
its tree and file-update record after failed PCM open. `snd_pcm_close` does not own that
cache. Unloading the library can discard its globals while their heap allocations survive.

✅ A direct ASan/UBSan/LSan build of the unchanged Runtime source and AudioProbe reproduces
**72,629 leaked bytes in 2,243 allocations** after one failed system-default open
(exit 1). The earlier Ubuntu hosted report had 66,666 bytes/2,049 allocations; configuration
and library versions differ. Full local trace: [default-before.txt](default-before.txt).
No leak-suppression option was used. The sandbox initially prevents LSan's thread tracing;
the recorded run used the permitted unsandboxed probe and completed leak detection.

Upstream ALSA 1.2.14 implementation references:
[snd_pcm_open / snd_pcm_open_lconf](https://github.com/alsa-project/alsa-lib/blob/v1.2.14/src/pcm/pcm.c#L2744),
[private update ownership](https://github.com/alsa-project/alsa-lib/blob/v1.2.14/src/conf.c#L4488),
[global update reference](https://github.com/alsa-project/alsa-lib/blob/v1.2.14/src/conf.c#L4697).

The repair uses `snd_config_update_r` with initially null **instance-owned** tree/update
pointers and `snd_pcm_open_lconf`. It retains that tree for the PCM generation, then closes
PCM, deletes the tree, frees the update record, and finally releases the library, including
parse/open/format failures. Reopen reloads configuration through ALSA's normal system,
user-hook and `ALSA_CONFIG_PATH` selection. No global configuration cleanup or cache
replacement is introduced into Runtime. This avoids imposing process-wide ownership on
unrelated applications/plugins using the same ALSA library. No new link or development
package dependency, ABI or platform behavior outside Linux is introduced.

## Real-adapter regression matrix

✅ `GameplayAlsaTests.cpp` and `GameplayAlsaAcceptance.py` run five configurations
(null, missing-card, malformed, absent-file, system-default) both alone and beside an
external ALSA client. Each of ten subprocesses performs 65 Nexora opens, including 32
transient owners closed/reopened and then destroyed with a live generation. A persistent
second Nexora owner submits silence after transient teardown. Missing outputs reject
playback. Close is called twice to test idempotence.

The external client owns a separate live null PCM, a retained global configuration reference
and borrowed node. Every transient iteration verifies the global root identity, borrowed
`pcm.default.type` string and successful external silent submission. The fixture alone
cleans up its own global cache after closing/unreferencing its owners; Runtime never does.
The separate no-peer subprocesses keep that fixture cleanup from masking the original leak.

✅ Compiling the same fixture against the original Runtime reproduces **4,720,885 leaked
bytes in 145,795 allocations** for 65 system-default failed opens (exit 1;
[full compressed trace](system-before.txt.gz); read with `gzip -dc`). The old missing-device peer case also fails with
`Nexora replaced the external global config` ([trace](peer-before.txt)). The old null-only
case passes; it does not reproduce the failed-default leak and is not used as proof of repair.

The system-default case observes either availability outcome without submitting a tone;
this host reports unavailable for all 65 opens. Null succeeds for all 65; the three explicit
bad configurations fail for all 65, in both peer modes. **All ten subprocesses pass under
ASan/UBSan/LSan (650 opens total)** without sanitizer options/suppression. Full stdout/stderr:
[sanitizer-after.txt](sanitizer-after.txt); [machine-readable results](sanitizer-after.json).
Expected ALSA diagnostics for bad configurations are retained rather than hidden.

Reproduce the isolated sanitizer build from the repository root:

```bash
g++ -std=c++20 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -IEngine/Runtime/include -IEngine/API/include \
  Engine/Runtime/src/GameplayAudio.cpp Tests/Runtime/GameplayAlsaTests.cpp \
  -ldl -o /tmp/nexora-alsa-lifetime
python3 Tests/Runtime/GameplayAlsaAcceptance.py \
  --test /tmp/nexora-alsa-lifetime --evidence-dir /tmp/nexora-alsa-evidence
```

The CMake-registered `runtime.gameplay_alsa_lifetime` uses the actual Runtime target in
Development and sanitizer configurations. Hosted sanitizer CI uploads all matrix logs
for success or failure, with seven-day retention. Tests do not require ALSA development headers.

## Local full gates

✅ `cmake --preset linux-development`, `cmake --build --preset linux-development`,
`ctest --preset linux-development`: **194 passed, zero failures, 11 native Vulkan skips**
(205 registered; 125.72 s). The skips require unavailable native display/GPU infrastructure;
no physical/native graphics acceptance is inferred. The new Runtime lifetime test passes
all ten subprocesses in 0.28 s against the actual modular target.

✅ `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping` pass
with the existing Editor-free Minimal/Monolithic configuration. Running
`Tests/TwoD/AudioDeviceAcceptance.py` with the Shipping `NexoraTwoDAudioProbe` passes:
8,194 submitted null frames with reopen, explicit unavailable rejection and nonzero failure
when unavailable was not requested.

✅ Touched C++ formatting, `git diff --check`, changed Markdown links and all 16
documentation-routing tests pass. Local CMake 4.4.4, Ninja 1.13.2 and Clang 19 use
external tooling/sysroot; no preset, gate or dependency policy was relaxed.

### Local log SHA-256

Full build/test logs remain outside the checkout in `/workspace/work/cloud-backlog/alsa`.
Committed sanitizer traces above are self-contained.

- `development-configure.log`: `6a1e6e559f37d41351c878eec45e01e2b3b95235ae5d91c91234b50164625c02`
- `development-build.log`: `257773c9ce0977fa3d8809fa5a3407ac497071893ddbe3a02f237a35fd3941d2`
- `development-build-final.log`: `b94d65699045a826a2d24b15d44d229dc884918bf5cfdebbc3ebe3d3edd26c1f`
- `development-ctest.log`: `75eec4d58262d53a2d5137065b2f7d59cc2aa090aea08a518ff68afec55c8a6c`
- `shipping-configure.log`: `22961df7b347b6e70e60308342e4683843ed27af432c4feaefd98ffca55007b6`
- `shipping-build.log`: `4855ac39dca439d2159faf9c0f1ba189f0cd4f9c96d0240648212e3e3fcbaa7b`
- `shipping-build-final.log`: `8c35b1682f73592d30612d7b0fabdb384fcfff35cb5a076cb48b85b1368aa794`
- `shipping-audio.log`: `0cace8c1b18025e74f52ab042865647c83fb9a5ae47f86002b5befa40e43de76`
- `documentation-tests.log`: `5895e7c7a5ed6ee7577da032833214a466485ba4590d22ac3abb4a4d75fb8220`

## Acceptance boundary

Physical speakers, audible output, hotplug/recovery and Windows device execution are not
verified by silent/null Linux tests. M6/M7, 9/11 (82%) and Editor milestone counts remain unchanged.
The root GitHub README was reviewed and needs no supporting-task entry.
