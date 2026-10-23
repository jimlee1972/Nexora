//! Caller-owned offline WAV line controller. Subtitles/UI and chapter policy remain game code.
//! Stop/release only this controller's voice/asset/ticket. Never stop music or all SFX.
const std = @import("std");
const audio = @import("audio.zig");
const audio_state = @import("audio_state.zig");
const assets = @import("assets.zig");
pub const Phase = enum { idle, loading, playing, complete, fallback };
pub const Line = struct {
    sound: audio.Api,
    control: audio_state.Api,
    files: assets.Api,
    buffer: []u8,
    asset: u64,
    ticket: u64 = 0,
    voice: u64 = 0,
    registered: bool = false,
    gain: f32 = 1,
    muted: bool = false,
    paused: bool = false,
    phase: Phase = .idle,
    /// Explicit replay starts from the beginning. Null/missing/corrupt/oversized lines use subtitles.
    pub fn start(self: *Line, index: ?u32) void {
        self.cancel();
        self.phase = .fallback;
        const catalog = index orelse return;
        var entry = assets.CatalogEntry{};
        if (self.asset == 0 or !self.sound.valid() or !self.control.valid() or !self.files.valid() or
            self.files.describe_catalog.?(self.files.context, catalog, &entry) != 0 or entry.kind != 3 or
            entry.file_bytes > self.buffer.len or self.files.requestLoad(catalog, &self.ticket) != 0) return;
        self.phase = .loading;
    }
    pub fn cancel(self: *Line) void {
        if (self.voice != 0) _ = self.sound.stopVoice(self.voice);
        self.voice = 0;
        if (self.registered) _ = self.sound.removeAsset(self.asset);
        self.registered = false;
        if (self.ticket != 0) _ = self.files.releaseRequest(self.ticket);
        self.ticket = 0;
        self.phase = .idle;
    }
    pub fn volume(self: *Line, gain: f32, muted: bool) !void {
        if (!std.math.isFinite(gain) or gain < 0 or gain > 1) return error.InvalidGain;
        if (self.voice != 0 and self.sound.setVoiceGain(self.voice, if (muted) 0 else gain) != 0) return error.Audio;
        self.gain = gain;
        self.muted = muted;
    }
    pub fn pause(self: *Line, paused: bool) !void {
        if (self.voice != 0 and self.sound.pauseVoice(self.voice, paused) != 0) return error.Audio;
        self.paused = paused;
    }
    pub fn tick(self: *Line) void {
        if (self.phase == .loading) {
            var state = assets.LoadState{};
            if (self.files.readState(self.ticket, &state) != 0 or state.phase == assets.failed) {
                self.cancel();
                self.phase = .fallback;
                return;
            }
            if (state.phase != assets.ready) return;
            var length: u64 = 0;
            const copied = self.files.copyBytes(self.ticket, self.buffer, &length) == 0;
            if (copied and length <= self.buffer.len and self.sound.registerWave(self.asset, self.buffer[0..@intCast(length)]) == 0)
                self.registered = true;
            _ = self.files.releaseRequest(self.ticket);
            self.ticket = 0;
            if (!self.registered or self.control.playVoice(self.asset, if (self.muted) 0 else self.gain, &self.voice) != 0) {
                self.cancel();
                self.phase = .fallback;
                return;
            }
            if (self.paused and self.sound.pauseVoice(self.voice, true) != 0) {
                self.cancel();
                self.phase = .fallback;
                return;
            }
            self.phase = .playing;
        } else if (self.phase == .playing) {
            var available: u32 = 0;
            var playing: u32 = 0;
            if (self.sound.outputAvailable(&available) != 0 or available == 0) {
                self.cancel();
                self.phase = .fallback;
            } else if (self.sound.isPlaying(self.voice, &playing) != 0 or playing == 0) {
                self.cancel();
                self.phase = .complete;
            }
        }
    }
};
