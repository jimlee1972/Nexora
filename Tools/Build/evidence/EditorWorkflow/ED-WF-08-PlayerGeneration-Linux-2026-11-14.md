# Standalone Player generation contexts — Linux Cloud acceptance

Supporting task: `nexora-whr.8.2.2`. Local gates completed 2026-11-14T12:40:39.986816+08:00.

The common TwoD Player now owns stable contexts for active, candidate and retired generations.
Every cached World/TwoD/source/scene table stays bound to its generation. `GenerationHost::Reload`
enters read-only capture before Runtime invokes either save-state pass. Candidate/retired external
service calls reject, and active normal final Unload still cleans up services. The original
single-scene factory signature is preserved; an additive overload retains absent Tilemap behavior.
C++ consumers rebuild for the changed generation class layout; C/Zig wire layouts stay unchanged.
Dynamic initial Load and Reload use distinct host-owned temporary module images. Rejected copies
are removed after candidate finalizers; superseded/final copies are removed after library close.
Supported canonical SDK modules are self-contained; sibling native payloads are not copied.

The actual Player host factory/owner fixture passes32 canonical SDK replacements and128 rejected
prepare/restore/reloaded/schema candidates, preserving IDs, anchors, source clocks/style and pool
handles. It rejects2560 save-time World/pool/service attempts,1408 candidate service attempts and
352 retired attempts, including actual ELF library finalizers. Source reimport replaces the first
source UUID's Atlas/region/Pivot. All host state allocations balance after final Stop/Destroy.

Its Linux native companion presents192 frames. Ten-bit red/blue markers acknowledge each exact
frame before readback. Scene and pooled source Sprites remain nonblank in both image halves; all
image rows after the marker band match before and after each accepted/rejected replacement. All
192 PPM hashes and exact markers are rechecked independently in the machine receipt. These frames
use the production host/owner and RenderSurface extraction fixture, rather than32 replacements
through the complete application loop. The separate Shipping raw-sample process verifies600
native presents and its bounded whole-Player reload; the relocated source-animation Shipping
Player retains its existing strict geometry/pixel checks.

## Validation

- Full `cmake --preset linux-development`, `cmake --build --preset linux-development` and
  `ctest --preset linux-development --output-on-failure`:529/529,zero failures/skips,1830.86s.
- All14 sequential gates pass: Development build/full suite and GCC15,Shipping Full build,
  actual relocated source animation and raw-sample native reload,Monolithic build/GCC15/five
  consumers,and TwoD-OFF/testing-OFF graphical Editor/native ProjectPlayer builds.
- GCC15.2 compiles five actual translation-unit commands in each Development and Monolithic
  configuration. Warnings remain errors. Source/build/fixture11hashes stay frozen throughout.
- API145,documentation CI16,touched C++/Zig formatting,Python syntax and11Markdown files/three
  bilingual roadmap pairs pass before publication. Root README and stable C/Zig layouts are unchanged.

The [machine receipt](ED-WF-08-PlayerGeneration-Cloud.json) records gate/log/source hashes, compiler
commands, complete test inventory, native/SDK receipts and the exact sequential runner.

## Retained failures and limits

The initial target typo compiled nothing. An early native pilot had not relinked the static app
and is excluded from new acceptance. Explicit relink reproduced the sample's missing lifecycle
capability; its safe existing behavior now opts in. Initial fixture aggregate/coverage mistakes
remain in logs: a failed prepare owns partial cleanup and does not receive SDK deinit, explaining
the independently confirmed1408 candidate rejections. Complete final frozen gates supersede them.
The first complete run passed528/529 and exposed dynamic-loader handle reuse for the same source
path, independently reproduced as RestartRequired=3. Unique initial/replacement images fix it;
the fixture verifies independent paths and removal of retired/final image directories. The raw
Zig sample uses the canonical TLS-free panic entry on macOS; hosted macOS CI must verify that path.

No local Windows/macOS/mobile execution or physical display/audio/device acceptance is claimed.
Fresh exact-head hosted CI is required before delivery. Main packaged Editor/Player services,
project watcher/compiler,complete ED-WF-07/08 and parents8.2/8.3 remain open. Workflow5/9,
graphical Editor0/8 and TwoD9/11 are unchanged. Separately owned packaged Editor Play7.8 is untouched.
