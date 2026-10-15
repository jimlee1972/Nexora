# Isolated 2D Editor Play ownership — Linux, 2026-10-15

This is a prerequisite for `nexora-zih`, tracked independently as `nexora-zih.1`.
It does not attach 2D to the main project Editor, admit project/cooked asset generations,
add integrated Game UI/preview, implement hot reload, or accept physical Windows/Linux hosts.
Editor milestones remain 0/8 and the 2D roadmap remains 9/11.

## Change and ownership

`TwoDPlayContext` owns a distinct activated Play World and copied Sprite/collider metadata,
immutable Atlas/Clip owners, Tilemap/chunk state, fresh 1..4096-slot generational pool and
canonical 2D GameplayBridge. Creation admits at most 64 MiB of canonical document snapshot.
Read-only source inspection works; no source pointer, IO, project permission or renderer is held.

The serialized owner unloads all gameplay modules before context destruction. Stop/Destroy
still see the services; afterward the context releases the bridge before its borrowed owners
and World. Copied C tables are invalid after owner destruction. This is not automatic module
management or permission to call a retired context. Fresh sessions reject previous pool handles.
World deletion requires `ForgetEntity` for each destroyed descendant.

Scene/pool/loaded-visible-tile inputs share one globally sorted extraction and a 100,000-input
cap. Exact-boundary extraction succeeds; one additional tile rejects the frame. Extracting does
not advance animation. Frames retain immutable Atlas pixels after context/source destruction.
The presentation owner assigns and retires distinct native texture IDs for distinct frozen
generations, including changed pixels under the same Atlas UUID. Stable C/Zig wires, source
formats, module graph and the existing TwoD feature boundary remain unchanged.

## Focused acceptance

Preflight `ctest --preset linux-development -R '^twod.play_context' -V` passed **2/2**,
portable **1.27 s**, native **1.74 s**, total **3.02 s**, with zero skipped tests and the
Khronos validation layer requested. That preflight precedes the final explicit activation
assertion and structured native RGB report; final unchanged-source gates are recorded below.

- 32 actual static V3 module Create/Start/Update/Stop/Destroy lifetimes use the canonical API.
- Full-width Atlas/Clip UUIDs, copied collider queries and unloaded negative Tilemap chunks survive.
- Authoring replacement with the same Atlas UUID and changed Clip events leaves Play frozen.
- Empty documents, short API tables, bad texture/extent, wrong-thread calls, pool quota and
  previous-session handles have explicit rejection/conservation checks.
- Failed Create and failed Start leave source bytes/history intact; descendant metadata is retired.
- Source Undo/Redo remains usable, read-only mode is preserved by capture, and source destruction
  does not invalidate context assets. Owning frames survive context destruction.
- Native X11/Vulkan pixels distinguish frozen red before/after source replacement, animated
  green, retained green after context destruction and new-session blue under the same UUID.
  Exactly three native UI texture uploads comprise two Atlas generations plus the existing
  immutable white background; both Atlas generations retire explicitly.
- A separately loaded valid scene stores IDs 20,10, while the existing SpriteScene renderer
  resolves equal-depth translucent ties as 10,20. Comparing the complete vertex stream caught
  a new-context ordering defect (0.01 s failure before correction). Sorting the owning pose
  snapshot by entity ID before assigning draw-only namespace IDs preserves that contract.

Initial test corrections are retained: the first test used the wrong wire event-length field
and assumed vertex equality support; subsequent UV comparison incorrectly selected the first
globally sorted vertex (a Tile). The complete UV comparison corrected that oracle. A real empty
document extraction gap was fixed by treating an empty Atlas-less Tilemap as zero inputs.
The first native run passed every pixel phase but counted only the two Atlas uploads; the
existing background upload makes the exact expected total three. Assertions were corrected
to the actual existing contract; pixel, quota and source-conservation checks were retained.
The initial full gate was interrupted during hierarchy scale testing to add and fix the
non-ID-storage transparency regression; it is not counted as a completed full run.

Initial source `eb766960` passed Linux310/310 but hosted Windows MSVC14.51 /W4 /WX rejected
three implicit size_t-to-uint32 test capacities (events2, hits4, cells99999), C4267. Explicit
casts now state those bounded widths; no warning policy, production code or assertion changed.
Clang `-Werror=shorten-64-to-32` syntax validation and a complete restarted Linux gate pass.
Companion JSON preserves the earlier gate and exact failure run/job separately.

## Final gates

- `cmake --preset linux-development`; `cmake --build --preset linux-development`: passed,
  **570 build actions**, graphical shell/real Zig/Slang enabled.
- `ctest --preset linux-development`: final restarted run **310/310, zero skipped, 1056.48 s**.
  New portable/native tests **1.40/1.75 s**, including the independent storage-order regression.
  Native RGB samples are exactly red/red/green/green/blue, three verified texture uploads,
  unchanged source bytes, no stderr, Khronos validation requested. Companion JSON retains the
  structured report, capture hash and all five unchanged final source/test hashes.
- Existing 100k hierarchy lifecycle **165.97 s** and native Play soak **82.00 s** pass.
  The latter retains 32 cycles/128 records, source pixels/bytes and zero validation errors;
  warmed RSS peak growth **5025792 bytes** is within its unchanged 33554432-byte guard.
  This observation is not a cause/fix or proof of all-memory leak freedom.
- `cmake --preset linux-shipping`; `cmake --build --preset linux-shipping`: passed;
  prescribed Minimal Monolithic, Editor/testing excluded. Initial incremental build had seven
  actions, final rerun had no work. It does not compile/test this Editor-only class.
- Fresh Development configure with `NEXORA_ENABLE_2D=OFF` and graphical shell ON: passed,
  optional implementation/test targets absent. Initial fresh-cache attempts needed the same
  explicit cloud X11/Vulkan/Slang/Zig paths as the working cache; no source workaround or
  platform policy change. This is a configure check, not an additional feature-off build.
- Documentation routing **16/16**, seven Markdown files and three touched C++ format checks
  pass. Both Editor and 2D bilingual roadmaps are synchronized; root README is unchanged.

The [companion JSON](EditorEDM3-TwoDPlayContext-Linux-2026-10-15.json) separates preflight,
interrupted-run provenance and final results. Hosted platform checks are recorded in the PR
on its exact source head before merging.

Native virtual-display correctness does not establish physical display, GPU/CPU frame-time,
production asset budgets, full process-memory leak freedom or integrated main Editor acceptance.
