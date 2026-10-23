# SP-05 bounded offline voice — cloud evidence

Base: `d68ade0240c865a6288b2e66332e4c05d708e7da`, latest remote main at task start.
Branch: `feat/voice-residency-sp05`. Beads: `nexora-6d6`; final corpus/physical gate: `nexora-6d6.1`.

## Delivered

Bounded decoded PCM admission before allocation; exact active/retired generation accounting;
optional AudioState service with independent Voice bus and unchanged Audio V1; all historical
services prefixes; strict exported `audio_capacity`; caller-owned Zig per-line dialogue lifecycle;
metadata inventory of actual local WAV recordings. No new dependencies or download service.
See [the complete contract](../../../Engine/Runtime/GameplayVoice.md).

## Executed in Linux cloud

- Full Development configure/build/CTest: **285 registered, 269 passed, 16 existing native
  prerequisite skips, zero failures**, 171 seconds. New residency, inventory, actual Zig PCM
  consumer and independent Zig/Player package cases ran, rather than skipping.
- Full Monolithic Shipping build succeeded. Separate baseline-CPU ReleaseSafe Zig shared module
  exported and ran with Shipping Player; inventory verifies no Development test copy.
- C/Zig all-field layout, 56/64/72/80-byte old services prefixes and new 88-byte table, old-host
  rejection, canaries and wrong-thread rejection passed. Original Audio V1 still rejects bus 3;
  dedicated Voice operations are negotiated through the new optional table.
- Captured PCM verifies independent Music/SFX/Voice levels, mute, paused cursor, skip/replay,
  missing/corrupt IO, failed sink/reopen and cleanup without stopping music.
- Twelve procedural mono 48 kHz PCM16 lines: **100663824 encoded bytes**, **201326592 decoded
  float bytes**, **1048.576 total seconds**. A one-line resident set is exactly **16777216 PCM
  bytes**. Installed package exceeds 96 MiB; all WAVs are deferred blobs. Final PCM and blob
  staging counters are zero. Invalid capacity settings reject before package publication.
- Real ALSA null output is required on Linux and submitted frame counts are checked. Audio-off
  fallback also runs. Development observed process peaks: 40808448 and 41193472 bytes; Shipping:
  38031360 and 38481920 bytes. Parent sampling can miss short peaks; these observations are not
  production budgets. Runtime stress repeats 16 MiB PCM 48 times: warm/tail RSS 37158912/37158912 bytes.
- API-M6: ABI 3, 114 documented exports passed. Touched C++/Zig formatting, Python compilation
  and diff checks pass. Root README unchanged; no M6/M7 or physical acceptance gate is closed.

[Commands](SP05-OfflineVoice-Cloud-2026-10-23/commands.txt),
[JUnit](SP05-OfflineVoice-Cloud-2026-10-23/development-junit.xml),
[CTest output](SP05-OfflineVoice-Cloud-2026-10-23/development-tests.log),
[Shipping consumer](SP05-OfflineVoice-Cloud-2026-10-23/shipping-consumer.log).
Log trailing whitespace is normalized; JUnit is unchanged. Hosted CI results belong in the PR
and game integration document after those jobs complete; this report records local execution.

## Sanitizer RSS regression

The initial PR run [build-contract](https://github.com/jimlee1972/Nexora/actions/runs/39976291790/job/120491927043)
failed the new RSS growth assertion: its first twelve allocations did not fill ASan's default
256 MiB quarantine. This was reproduced locally (warm 304209920, tail 361021440 bytes). Every
round's live PCM counters were still zero; no sanitizer memory violation was reported.

The test now executes 48 rounds, comparing equal 24-round warm/tail windows. This fills the default
quarantine before comparing plateaus; the existing **32 MiB growth limit is unchanged**, and all
48 rounds still require zero active/retired PCM. Diagnostics print before the assertion.
Targeted ASan/UBSan/LeakSanitizer passes with defaults: warm 361000960, tail 361246720 bytes.
The accepted command ran outside the ptrace sandbox (which prevents LeakSanitizer thread scanning);
no sanitizer/quarantine option was disabled. This is a targeted native-graphics-off Runtime test,
not a claim that the entire local sanitizer preset ran. Hosted full gates must pass at final head.

[Original RSS reproduction](SP05-OfflineVoice-Cloud-2026-10-23/asan-before-default.log),
[fixed sanitizer output](SP05-OfflineVoice-Cloud-2026-10-23/asan-fixed.log).

## Boundaries and open acceptance

No formal Japanese recordings exist in the game repository. The inventory and PCM figures above
measure procedural fixtures, not a final corpus. Inventory validates WAV metadata, not float
payload validity, final subtitles or unlisted content. Missing declared package files still fail
strict startup validation; unmapped lines and runtime read failures can retain subtitles.

Owner-thread decode/publication and platform output may stall. This is per-line/chapter local
PCM, not compressed decoding, disk streaming or realtime mixing. Defaults now bound decoded PCM
at 64 MiB active/128 MiB live; legacy larger consumers must configure measured larger budgets.
Queued native blocks can remain audible after stop/mute until consumed. Host output recovery
and explicit game replay are separate. No null/captured sink proves audible speaker behavior.

Final recordings/subtitle coverage, concurrent BGM/SFX/voice residency, complete offline install,
Windows audible playback/device loss/reopen, queue tails, target RAM/frame time and long soak
remain open in `nexora-6d6.1`. Other six SP assessments retain their existing gates.
