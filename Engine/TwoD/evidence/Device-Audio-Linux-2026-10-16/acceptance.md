# Gameplay device audio cloud slice — 2026-10-16

Baseline: `8a077f50` (`main`). Task: `nexora-j4q.7.1`.

## Delivered behavior

✅ `GameplayAudio::OpenDefaultOutput` now opens Windows default waveOut stereo PCM16 through
System32 `winmm.dll`, preserving the existing optional gameplay audio table and portable mixer.
Fixed owned PCM/header storage, synchronous event completion, a two-second completion timeout,
reset/unprepare/close and unavailable-on-error behavior are documented in
[the contract](../../../Runtime/GameplayAudio.md). It adds no import-library/module dependency.

✅ Runtime-only `NexoraTwoDAudioProbe` plays a quiet 440 Hz tone, verifies close/unavailable/reopen,
and always separates device submission from human audibility. It also supports an explicit
unavailable expectation. The tool is included in Monolithic Shipping without Editor.

## Validation

Linux GCC 14.2 / CMake 4.4.4, Development Modular and Shipping Monolithic:

```bash
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development --parallel 4
cmake --preset linux-shipping
cmake --build --preset linux-shipping --parallel 4
python3 Tests/TwoD/AudioDeviceAcceptance.py --probe /workspace/work/2d-roadmap/shipping-package/NexoraTwoDAudioProbe --evidence-dir /workspace/work/2d-roadmap/shipping-audio
ldd build/linux-shipping/Tools/TwoD/NexoraTwoDAudioProbe
```

Full Development CTest: **167 passed, zero failed, 3 skipped, 170 registered**. The three
Vulkan scene/pbr/upload-reuse tests skip without the Slang/display setup; no graphical acceptance
is claimed. `runtime.gameplay_audio` and new `twod.audio_device_lifecycle` both pass. The actual
ALSA adapter submits 8,194 frames including reopen through a null device. A missing device is
reported unavailable; an ordinary run against it fails instead of reporting PASS. The same
lifecycle passes after copying the Shipping probe into a separate directory. `ldd` lists only
the OS C/C++/math runtime and loader, with no Engine/Editor DSOs; ALSA loads dynamically.

The fresh cloud executor required task-local CMake/Ninja/Clang and extracted X11/OpenSSL SDK
packages. Initial configure supplied `CMAKE_PREFIX_PATH` to that external SDK, explicit installed
X11/Vulkan `.so` paths, and `OPENSSL_CRYPTO_LIBRARY` to the installed shared library. Initial full
CTest failed solely because `clang++` was absent; after installing task-local Clang 19, the full
gate above passed. The initial extracted static OpenSSL selection also caused unrelated zlib/zstd
link failures; selecting the installed shared OpenSSL fixed the environment without source changes.
No machine overrides or generated build output are committed.

Windows code and probe cross-compile/link with:

```bash
x86_64-w64-mingw32-g++-posix -std=c++20 -Wall -Wextra -Wpedantic -Werror -DNEXORA_RUNTIME_STATIC=1 -IEngine/Runtime/include -IEngine/API/include -c Engine/Runtime/src/GameplayAudio.cpp -o windows-audio.o
x86_64-w64-mingw32-g++-posix -std=c++20 -Wall -Wextra -Wpedantic -Werror -static -DNEXORA_RUNTIME_STATIC=1 -IEngine/Runtime/include -IEngine/API/include Tools/TwoD/AudioProbe.cpp windows-audio.o -o NexoraTwoDAudioProbe.exe
x86_64-w64-mingw32-objdump -p NexoraTwoDAudioProbe.exe
```

The resulting PE imports only `KERNEL32.dll` and `msvcrt.dll`; waveOut resolves at runtime.
This is cross-compilation, **not Windows execution or MSVC validation**.

## Remaining acceptance

Windows real-device open/write/reopen/failure recovery, human audibility and device/driver models
are **NOT RUN**. ALSA null output cannot prove speakers. The existing Zig host's `--audio` uses
this backend when a module registers/plays its own WAV. The probe is supporting device evidence,
not the integrated cooked M7 product. Physical controller, UI/font appearance, save/reopen,
DPI/display, clean Windows Runtime prerequisites and physical M6 performance remain target-host
or separate integration gates. M7 remains **PARTIAL / NOT ACCEPTED**; 9/11 (82%) is unchanged.

Machine-readable observed outcomes: [acceptance.json](acceptance.json).
