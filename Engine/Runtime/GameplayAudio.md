# Gameplay audio service

✅ `nexora/gameplay_audio.h` and `Bindings/Zig/audio.zig` provide a versioned,
optional owner-thread service for music/SFX playback, looping, pause, voice gain,
master/music/SFX bus gains, playing queries and stop-all. Asset/voice IDs are opaque
64-bit values; zero is invalid. Hosts copy PCM or decode RIFF/WAVE PCM16/float32
mono/stereo through `GameplayAudio`; gameplay never retains sample pointers.
Zig may register its own `@embedFile` WAV bytes through `registerWave(asset, bytes)`;
no game-specific C++ registration is required. Registration copies decoded samples
before returning; bytes remain owned by the caller. The encoded payload limit is
64 MiB. `removeAsset` releases registration without invalidating active voices.
Replacing/removing an asset leaves already playing voices on their original data.
Voice IDs are monotonic and never reused within a host instance, including after
completion. Exhaustion fails instead of aliasing an old voice. The API table and
context are borrowed until the owning `GameplayAudio` is destroyed; gameplay must
stop using them before host destruction. Calls and callbacks are serialized by the
host on one owner thread, including hot reload and shutdown.

The fixed-capacity mixer uses nearest-neighbor sample-rate conversion, mono-to-stereo
expansion, per-voice/bus gain and output clipping. `Pump(1..4096)` allocates no mixing
buffer and invokes no gameplay callback. A nonlooping voice retires on sample
completion; pause preserves position. Hosts call `stop_all` when replacing gameplay
or starting another session. Asset registration/voice retirement can allocate/free
host storage and must not run on a realtime device callback. Device submission is
synchronous and can block; `Pump` belongs to an application-controlled owner loop,
not a realtime audio thread. This is a basic mixer, not the planned realtime DSP
or streaming audio runtime.

A host may install a synchronous stereo float sink, or call `OpenDefaultOutput`
for Linux ALSA (`libasound.so.2`, dynamically loaded, no ALSA development package
required). Only a successfully configured sink permits playback. Missing library,
missing device, rejected format and write/recovery failures report unavailable
output; a failed write disables the output until the host configures it again.
Device-write failure consumes the mixed block rather than replaying it. Other
platforms require a host sink and return an explicit unavailable backend error.
A test sink verifies mixing only; it is not evidence of audible physical output.

Return values are `0` success, `-1` invalid arguments, `-2` unavailable output,
`-3` missing assets/stale voice handles/capacity exhaustion. Queries write results
only on success. `is_playing` reports whether a voice still exists (including
paused voices); a completed or stale nonzero ID reports false. Gain range is finite
`0..16`; play bus is music or SFX; the master bus scales both. C callbacks never
return owning pointers. The API is independent of reload migration and disk saves.

Acceptance: `runtime.gameplay_audio` verifies actual PCM samples, resampling,
loop wrapping, completion, stale IDs, master/music/SFX isolation, pause retention,
asset removal residency, WAV validation, capacity limits, invalid gains, stop-all
and output-write failure. Cloud tests do not assert physical audio-device output.
