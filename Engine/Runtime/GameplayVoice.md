# Bounded offline voice playback

The optional `NexoraGameplayAudioStateApiV1` adds decoded PCM usage and a dedicated Voice bus.
It is negotiated through the appended `get_audio_state_api` services suffix; original/SP-01/
SP-02/SP-03 services prefixes remain intact. The original Audio V1 table and its accepted bus
values (master 0, music 1, SFX 2) are unchanged. New modules check returned services size before
reading the suffix: `nx.getAudioState(services)` returns null on old/absent hosts.

## PCM residency

`GameplayAudio::Limits` defaults to 256 registered assets, 64 MiB active float PCM and 128 MiB
live PCM including removed/replaced samples still held by voices. Configurable bounds are
1..4096 assets, 1 byte..1 GiB active and active..2 GiB live. Invalid C++ limits throw before use.
The original two-argument constructor is retained and applies these bounded defaults.

Registration validates shape/rate, predicts decoded float bytes and checks count/active/live
budgets **before allocating decoded storage**. Replacement reserves the entire new generation
in addition to the old live data, even when that old generation can expire immediately after
publication. Failed admission preserves the previous registration/voices. WAV decode now moves
its sample vector into the published generation instead of copying it a second time. Malformed
WAV returns Audio V1 `-1`; budget/allocation failure returns `-3`. Encoded WAV stays at most 64 MiB.
Direct PCM registration uses the same decoded budgets. Nonfinite/out-of-range float samples reject.

Remove/replacement detaches the registry but does not stop voices. Old playing/paused voices
retain their exact samples and contribute to retired bytes/generations until completion/stop.
Shared residency counters retire that generation once, not once per voice. Usage reports current
registered assets, active voices (including paused), active/retired bytes, retired generations,
peak live decoded bytes and configured limits. It is not total RSS, raw blob staging, allocator
capacity or native queued-output memory. Audio V1 ownership/serialized execution rules still apply.

## Independent Voice bus

`play_voice(asset, gain, &voice)` starts a one-shot Voice-bus sound and returns a normal opaque
Audio V1 voice ID. Existing stop/pause/per-voice gain/is-playing calls work with it. Voice bus gain
is set separately by `set_voice_bus_gain`; it is unaffected by Music/SFX bus sliders. Master gain
scales all three buses. Gains remain finite 0..16; play failures preserve the output ID. New
callbacks are owner-thread only; foreign-thread calls return `-3`. Tables borrow `GameplayAudio`
until destruction, including module stop/destroy. Queries copy observations and return no PCM pointers.

Existing Audio V1 play/set-bus still reject bus 3, so an old host never pretends to support Voice.
New service calls require the independent table. No changes were made to platform output queues:
stop/mute only affect future mixed blocks; already submitted waveOut copies can remain audible
until consumed. Close/reset drops pending device buffers. See [audio/device contracts](GameplayAudio.md).

## Local IO and Zig policy

The Player accepts optional `audio_capacity` in `game.json`:

```json
"audio_capacity": {"assets": 64, "active_bytes": 67108864, "live_bytes": 134217728}
```

Exactly those three integer fields are required when present; unknown/duplicate keys, booleans,
nulls, invalid ranges and live below active reject. Export preserves `audio_capacity` in the
text project descriptor. No setting means the defaults above. Existing asset `capacity` remains
separate and unchanged. Legacy projects exceeding the new decoded defaults must choose a measured
larger audio budget; encoded size is not decoded size (PCM16 grows 2x when decoded to float).

Use existing deferred `blob` catalog entries for local WAVs. They bypass the old aggregate 64 MiB
startup preload set, while each file stays bounded. `nx.dialogue.Line` borrows the Audio V1,
AudioState and asset services plus a caller-owned buffer and exclusive nonzero audio asset ID.
It owns at most one request/registered line/voice; `start(index)` cancels only those resources,
then asynchronously reads/copies/registers/plays the new line. Explicit replay starts at sample
zero; pause retains the mixer cursor. Per-line gain/mute and global Voice-bus gain are independent
from music/SFX settings. `cancel` is required before freeing the caller's buffer or host services.
No gameplay callback or file read runs in the mixer/device thread.

Missing catalog mappings (`start(null)`), read failure after startup, corrupt/unsupported WAVs,
insufficient caller buffer/PCM capacity and unavailable/failed output enter `fallback`, allowing
the game to keep subtitles. A naturally finished voice releases its registration and becomes
`complete`. Playback recovery is explicit: the host reopens output, then the game decides whether
to replay the line. The generic Player retries optional output at one-second intervals. It does
not automatically repeat dialogue or advance a subtitle transaction. All declared package files
still pass strict startup validation: a missing manifest file is a damaged install, not silently
accepted as a complete voice corpus. Unrecorded lines should have no catalog mapping.

Decode/publication still occurs synchronously on the owner thread, and the platform output can
block. This is bounded per-line/chapter PCM, **not compressed decoding, disk audio streaming or
realtime mixing**. Profile final recordings and frame hitches before deciding whether those
extensions are needed. No new dependencies or network/download service were introduced.

## Recording inventory

`Tools/TwoD/InspectVoices.py manifest.json --output report.json` inventories PCM16/float32 WAV
metadata with no external package. The schema-1 manifest has `clips` (`id`, relative `path`,
optional `kind: voice|music|sfx`, `subtitle_id`) and optional `resident_sets` (`name`, `ids`). It
reports encoded/decoded bytes, duration, missing recordings/subtitle IDs, and the largest declared
concurrent PCM set. Missing coverage is explicit; a missing file contributes no measured bytes.
The tool validates container metadata, not float sample validity, translation or final coverage.
It does not guess concurrency: declare BGM, voice prefetch/cache and SFX owners together.
Budget replacement old+new and raw IO/buffer/device costs in addition to each set.

The procedural acceptance inventory has 100663824 encoded bytes, 201326592 decoded float bytes,
1048.576 seconds in total, but a one-line resident set of 16777216 PCM bytes. These are fixture
measurements, not estimates of the final Japanese voice corpus. Actual recordings are not yet
available in the game repository, so no final corpus size or optimal compression ratio is claimed.

## Verification and open gates

`runtime.gameplay_audio_residency` checks budgets before publication, retained generations,
independent Music/SFX/Voice samples, unavailable output/reconfiguration, legacy service canaries
and 24 rounds of 16 MiB PCM with observed process RSS. `gameplay.zig_voice_dialogue` drives the
actual Zig controller through captured PCM, pause position, mute, skip/replay, missing/corrupt
reads, sink failure/reopen, cancellation, old-host rejection and cleanup. C/Zig all-field layouts
are checked with the services/asset wire suite.

`gameplay.zig_voice_player` exports an independent Zig module with twelve 87-second procedural
WAVs (>96 MiB installed), plays/skips/replays with 16 MiB peak PCM and zero final residency, and
checks audio-capacity rejection. Linux additionally opens an actual ALSA null sink and submits
blocks; `off` mode verifies subtitle-only unavailable-output cleanup. Process RSS observations
are reported separately from PCM counters. A null/captured sink is never speaker acceptance.

Beads `nexora-6d6.1` keeps final recordings/subtitle coverage, worst-case BGM/SFX/VO concurrency,
complete offline installation, target Windows audible playback/unplug/reopen, queued-tail policy,
RAM/frame-time budgets and long soak open. No procedural fixture substitutes for those gates.
