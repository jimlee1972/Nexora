# Selected-path graphical Git output — Linux, 2026-10-21

Beads `nexora-gu7.22.4`; depends on the bounded diff backend (`nexora-gu7.22.3`, PR #679).
Backend PR #679 is accepted as `95d0b3e5` after all 38 applicable exact-head checks succeeded.
Its verified main tree is `20752b7e40e83b70fd952c30dfb73eae71dca6b3`, identical to the exact SDK
head privately integrated below. This is inspection, not semantic merge or complete ED-M4.

## Behavior and ownership

The status-only public UI entry remains. The new overload borrows both providers for one serialized
frame; persistent state owns its exact selected status snapshot, last-good raw result and display
storage. Native Editor creates both providers with the existing JobSystem, shares fresh project
scope/root handling, drains hidden work and shuts both down before job teardown. Replacing either
provider requires draining it and supplying a fresh scope. SDK objects/captures must be released
before library unload.

Only explicit working-tree/staged buttons execute a query for a selected tracked UTF-8 file or
rename pair. Tool input, window opening and row selection do not launch Git. Paths containing UI
format/ID characters retain their literal identity. Untracked and non-UTF8 paths remain observable
in status but do not cross the diff argv boundary. Read-only projects can inspect without source
or index writes; repository configuration/other helpers remain explicitly trusted by the owner.

Row identity includes the full status-operation generation. Selection/status/tool/root changes
revoke previous output; pending publication also checks exact selection snapshot, paths, scope,
root, target and diff operation. Permission/focus/Play/modal/recovery blocking cancels pending work.
Failures retain last-good only in the same context, labeled with its ORIGINAL paths and target.

Complete raw output is escaped once into staged owning text/line offsets before publication.
Controls, NUL, backslashes and invalid UTF-8 are visible escapes; newline boundaries remain lines.
Logical display storage is bounded by at most 1 MiB escaped text and 262145 u32 offsets, plus the
SDK's 256 KiB raw capture; allocator capacity/overhead is extra, and old/candidate data can coexist.
Vertical clipping/horizontal scrolling retain complete output. Allocation failure preserves the
old complete view. No raw output/tool selection is persisted or forwarded to Console/telemetry.
Git output can include warnings/binary/conflict notices; it is not a parsed patch or atomic source
snapshot. Formats, module dependencies and stable C ABI are unchanged; C++ consumers rebuild.

## Actual widget verification

Actual product pointer/keyboard/text events at 1x/2x drive temporary real Git repositories, checking
staged/working output and labels, rename pairs with `%s##`, source/index conservation, unsupported
paths, raw NUL, complete maximum output, overflow, 1000-line complete/clipped data, backend/display
allocation failure, actual native-process cancellation, focus/permission/scope changes, read-only
inspection and teardown. Original status-only controls remain covered separately.

A held row press across real asynchronous status replacement fails against the actual pre-fix
GUI library with `Held row release retargeted a replacement status generation`. Generation-qualified
IDs fix it; corrected status+diff 1x/2x tests pass 4/4 in 2.32 s. An initial isolated-link loader
failure (exit127) is retained/excluded; correcting library lookup reproduces the actual regression.
No correctness expectation or existing deadline is weakened.

## Corrected complete Linux validation

The branch starts at actual main and normally integrates accepted main `5f926931`; its upstream
tree is exactly `9a24dc26465cb2849f3c1b6fa95af032ca2b0095`. Backend source includes the separate
exported constant declarations that correct PR #679's MSVC C2487. No UI behavior/budget/warning
policy changes accompany that declaration correction.

```sh
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
```

Graphical shell and ProjectPlayer are ON, local Slang/Zig OFF, GCC 14.2/CMake 4.4.4. Corrected build
passes 233 integration steps over the initial 460-step full product build. Full CTest passes
**306 registered, 294 passed, 12 environment skips, zero failures, 647.75 seconds**. All 38 frozen
source/test/CMake/helper hashes remain unchanged before/after. Focused original status/new diff
controls at 1x/2x pass 4/4 in 2.49 s. Original Deep/100k pass 57.34/227.42 s at scale1 and unchanged
180/300-second limits. No production-isolation/percentile or physical-device claim is inferred.

Minimal Shipping config/build passes nine integration steps over the existing complete 102-step
build; it strips Editor and registers no tests. Earlier pre-declaration-correction UI306/615.48s
and focus4/2.23s remain historical, not substitutes for this corrected full-source proof.

Private integration normally merges the exact backend head `34b162f1`; its sole contract-document
conflict uses that backend's final SDK README. All 38 tested implementation/test/CMake/helper hashes
remain identical before/after. Final bilingual entries cover selected-path inspection only and keep
merge/nested discovery/ED-M4 acceptance open. Final documentation tests pass 16/16 in 3.630 s;
changed Markdown/bilingual links, source hashes, touched formatting and unchanged root README pass.
The accepted backend tree adds no source difference to this full UI proof. Publication starts from
that freshly verified main with the identical feature tree and no private integration history. Exact-head
hosted CI must pass before squash. These are headless actual widgets/native Git processes and
native-owner compilation/lifetime wiring; OS-window diff-button interaction and physical
DPI/IME/display/complete ED-M4 are not claimed.
