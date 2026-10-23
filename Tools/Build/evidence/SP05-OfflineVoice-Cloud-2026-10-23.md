# SP-05 bounded offline voice — cloud evidence

Base: `d68ade0240c865a6288b2e66332e4c05d708e7da`, latest remote main at task start.
Branch: `feat/voice-residency-sp05`. Beads: `nexora-6d6`; final corpus/physical gate: `nexora-6d6.1`.

## Delivered

Bounded decoded PCM admission before allocation; exact active/retired generation accounting;
optional AudioState service with independent Voice bus and unchanged Audio V1; all historical
services prefixes; strict exported `audio_capacity`; caller-owned Zig per-line dialogue lifecycle;
metadata inventory of actual local WAV recordings. No new dependencies or download service.
See [the complete contract](../../../Engine/Runtime/GameplayVoice.md).

API-M6 now pins AudioState field order, the services getter and C/Zig mirror in an independent
baseline. All four callbacks have ownership/nullability/threading/error/determinism metadata.
Mutation tests reject field reorder, suffix removal, a missing baseline, missing callback metadata
and incomplete threading metadata. ABI 3 now documents **118 exports**.

Zig remembers pause requested before asynchronous loading completes and applies it before mixing.
Pause persists across cancel/replay until explicit resume. Captured PCM tests demand music-only
output while the newly loaded/replayed voice is paused, then its exact first sample on resume.

## Executed in Linux cloud

Full Development configure/build/CTest: **286 registered, 270 passed, 16 existing native
prerequisite skips, zero failures**, 163 seconds. The new ABI mutation guard (five internal
checks), loading-pause PCM regression, residency, inventory and independent Zig/Player cases
all ran rather than skipping. Runtime 48-round PCM RSS warm/tail: 37163008/37163008 bytes.
Development Player observed peaks: 40792064/41201664 bytes; Shipping: 38027264/38481920 bytes.

- Full Monolithic Shipping build succeeded. Separate baseline-CPU ReleaseSafe Zig shared module
  exported and ran with Shipping Player; inventory verifies no Development test copy.
- C/Zig all-field layout, 56/64/72/80-byte old services prefixes and new 88-byte table, old-host
  rejection, canaries and wrong-thread rejection passed. Original Audio V1 still rejects bus 3;
  dedicated Voice operations are negotiated through the new optional table.
- Captured PCM verifies independent Music/SFX/Voice levels, mute, loading and active pause,
  skip/replay, missing/corrupt IO, failed sink/reopen and cleanup without stopping music.
- Twelve procedural mono 48 kHz PCM16 lines: **100663824 encoded bytes**, **201326592 decoded
  float bytes**, **1048.576 total seconds**. A one-line resident set is exactly **16777216 PCM
  bytes**. Installed package exceeds 96 MiB; all WAVs are deferred blobs. Final PCM and blob
  staging counters are zero. Invalid capacity settings reject before package publication.
- Real ALSA null output is required on Linux and submitted frame counts are checked. Audio-off
  fallback also runs. Sinks capture samples or discard audio; neither proves physical audibility.
  Parent process RSS sampling can miss short peaks; these observations are not production budgets.
- Touched C++/Zig formatting, Python compilation, API-M6 and Markdown checks pass. Root README
  unchanged; no M6/M7 or physical acceptance gate is closed.

[Reproducible commands](SP05-OfflineVoice-Cloud-2026-10-23/commands.txt) are hand-maintained.
Generated JUnit, configure/build/CTest logs and other execution output stay outside source control
in the task workspace or hosted CI artifacts. Final current-head hosted links/results are recorded
in the PR and game integration document after those jobs complete; this report records local work.

## Sanitizer RSS regression

The initial PR run [build-contract](https://github.com/jimlee1972/Nexora/actions/runs/39976291790/job/120491927043)
failed the new RSS growth assertion: twelve warmup allocations did not reach the plateau of
ASan's default 256 MiB quarantine. This was reproduced locally (warm 304209920, tail 361021440
bytes). Every round's live PCM counters were still zero; no sanitizer memory violation was reported.

The test executes 48 rounds, comparing equal 24-round warm/tail windows. This fills the default
quarantine before comparing plateaus; the existing **32 MiB growth limit is unchanged**, and all
48 rounds still require zero active/retired PCM. Diagnostics print before the assertion.
Targeted ASan/UBSan/LeakSanitizer passes with defaults: warm 361000960, tail 361246720 bytes.
The accepted command ran outside the ptrace sandbox, which prevents LeakSanitizer thread scanning;
no sanitizer/quarantine option was disabled. This targeted native-graphics-off Runtime check does
not represent a full local sanitizer preset. Hosted full gates must pass at the final review head.

## Boundaries and open acceptance

No formal Japanese recordings exist in the game repository. Fixture figures do not estimate a final
corpus. Inventory validates WAV metadata, not float payload validity, final subtitles or unlisted
content. Current project catalogs contain at most 4096 total files, including art and audio;
final recording inventory must check both file count and bytes. Exceeding that bound requires a
validated packaging/catalog extension. Missing declared files still fail strict startup validation;
unmapped lines and runtime read failures can retain subtitles.

Owner-thread decode/publication and platform output may stall. This is per-line/chapter local
PCM, not compressed decoding, disk streaming or realtime mixing. Defaults bound decoded PCM
at 64 MiB active/128 MiB live; legacy larger consumers must configure measured larger budgets.
Queued native blocks can remain audible after stop/mute until consumed. Host output recovery
and explicit game replay are separate. No null/captured sink proves audible speaker behavior.

Final recordings/subtitle coverage, concurrent BGM/SFX/voice residency, complete offline install,
Windows audible playback/device loss/reopen, queue tails, target RAM/frame time and long soak
remain open in `nexora-6d6.1`. Other six SP assessments retain their existing gates.
