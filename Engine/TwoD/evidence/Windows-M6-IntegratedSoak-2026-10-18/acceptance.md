# Physical Windows integrated 10k and 30-minute soak acceptance

**Verdict: NOT ACCEPTED.** All ten native processes complete correctly, but the fixed
performance gate fails. Keep every failed run; do not merge this work while this gate fails.

## Fixed host, source and workload

AMD FX-6100, 16 GiB RAM, NVIDIA GTX 960 2 GiB, Windows 10 Home x64 build 19045,
NVIDIA 32.0.15.8180; physical developer PC, not a clean host. Desktop 1920x1080/60 Hz,
native client 640x480, VSync, two frame slots, Monolithic Shipping. Balanced power plan
and ordinary desktop applications remain in place. No local builds/other native fixtures
run during the formal measurement; no affinity, timer, power or security policy changes.

- Remote main base: `3ddace3b63c4386a15e30013894c81f8f1662e25`; compiled working base: `04979eb128d980f4ac2749062a28c10730e31761`.
- Measurement uses that base plus [the exact working patch](implementation.patch) and
  byte-preserved source snapshots under `source/`. It was not compiled from the later delivery commit.
- Frozen EXE SHA-256: `621ccc04c7352044f2daacfffa731f05b5718c95477faf47cf38bfcc48eac44c` (same before/after).
- Original `Measure` body is unchanged: normalized SHA-256 `965110db9fdf90967b1ac558e466efba93385fc5fe70628f5ccbda9a835ac6fc`.
- [Plan](plan.json) was written before all measurements; [source manifest](source-manifest.json)
  and [identity](identity.json) retain exact source/executable/raw artifact hashes.

The original six cases remain 100/1,000/5,000/10,000 mixed, 1,000 overlap and 10,000 sparse:
600 measured + four warm-up frames, twice per backend. Animation, movement, full spatial
rebuild/query/sweep, extraction/culling/sorting, native instancing, midpoint full-pool recycle
and immutable atlas retirement/reload are preserved. Every original non-timing output
(counts, query totals, coverage, batches, GPU sample counts and native identity) exactly
matches the preceding [24/24 screen](../Windows-M6-DX12Pacing-2026-10-18/acceptance.md);
see [load equivalence](diagnostics.json). The new failed screen does not erase that older run.

Integrated mode always updates/query-builds all 10,000 objects and additionally animates
256 translucent 0.6-world-unit VFX (alpha 0.125), uses two Sprite atlases/batches, and builds
16 panels, eight buttons and eight ten-glyph labels through the actual GameplayUi service
(120 UI quads/commands). Total visible counts including VFX are 9,956 mixed / 451 sparse;
native draw count is 123 per frame including background. Default two-region animation
advances every 0.1 simulation seconds; all fixed updates are 1/60 s per rendered frame.
UI navigation/activation is deterministic synthetic input, not physical-device acceptance.

At each reload the full pool recycles with stale-handle rejection; an explicit 8 MiB allocation
is touched at 4 KiB intervals and its last byte through volatile stores, then freed. The real
Atlas codec decodes again; both previous GPU atlas IDs retire before fresh immutable IDs upload.
This is in-memory loading churn, not asynchronous disk IO. The original six-case workload is
separate and unchanged. No gameplay, production art, CPU update skipping or GPU simulation.

Integrated short screens run 600+4 frames twice/backend, reload every 300. Soaks run at least
1,800 measured seconds after 1,200 warm-up frames, reload/layout alternation every 600 frames.
All full/cycle and mixed/sparse phase distributions must satisfy p95/p99 <=33.333333333333336 ms
and max <=66.66666666666667 ms. No repeated-until-green runs or discarded measured spikes.

## Results

| Run | Frames measured | Full p95 / p99 / max (ms) | Cycle p99 / max (ms) | Performance |
|---|---:|---|---|---|
| dx12-original-1 | 6 x 600 | see per-case CSV | original boundary retained | 5/6 |
| dx12-integrated-1 | 600 | 25.2508 / 30.1195 / 44.5645 | 30.1208 / 44.5654 | PASS |
| vulkan-original-1 | 6 x 600 | see per-case CSV | original boundary retained | 6/6 |
| vulkan-integrated-1 | 600 | 26.3609 / 30.0676 / 34.8470 | 30.0684 / 34.8477 | PASS |
| dx12-original-2 | 6 x 600 | see per-case CSV | original boundary retained | 6/6 |
| dx12-integrated-2 | 600 | 25.0667 / 30.4210 / 42.4772 | 30.4219 / 42.4779 | PASS |
| vulkan-original-2 | 6 x 600 | see per-case CSV | original boundary retained | 6/6 |
| vulkan-integrated-2 | 600 | 33.5462 / 49.2553 / 63.2472 | 49.2560 / 63.2479 | FAIL |
| dx12-soak | 95,701 | 23.5659 / 28.4656 / 62.8651 | 28.4662 / 62.8658 | PASS |
| vulkan-soak | 95,772 | 23.0951 / 27.7946 / 134.9226 | 27.7955 / 134.9232 | FAIL |

Original screen: **23/24** (DX12 11/12, Vulkan 12/12). Integrated short screens: **3/4**.
Continuous soaks: **1/2**. All ten exit codes, hardware identities and correctness checks pass.
All six integrated resource screens pass their sampled budgets. Warm-up remains retained
and explicitly marked. Across the formal suite **210,785 raw frames** are retained;
the two soaks contain 191,473 measured frames plus 2,400 warm-up frames. No frame samples
or completed GPU samples are missing; exactly two final in-flight GPU submissions per case
remain outside timing collection at the final loop boundary.

The original benchmark stdout still contains its legacy “60 FPS ... NOT RUN” footer.
The fixed plan and actual gate here use the current roadmap target of 30 FPS.

### Failed rows and same-frame diagnosis

1. DX12 original repeat 1, mixed 10k: p95 25.2394 ms, p99 **39.3942 ms**, max 53.6732 ms.
   Eight measured frames exceed 33.3333 ms. Frame 231 has CPU 17.2250 ms, Draw 35.8033 ms
   and EndFrame 0.5659 ms; frame 222 has EndFrame 26.0992 ms. These are separate frames.
2. Vulkan integrated repeat 2: cycle p95/p99 **33.5469 / 49.2560 ms**, max 63.2479 ms.
   Mixed phase p95/p99 36.6556 / 51.9761 ms; sparse phase p99 37.7026 ms. Wall-time stalls
   occur in population, extraction, Draw and EndFrame. Frame 40 has CPU 55.1253 ms
   (extraction 34.4028 ms); frame 36 has CPU 19.0367 ms and EndFrame 25.3579 ms.
3. Vulkan soak frame **24,415**, 463.0519374 seconds from loop start, has cycle
   **134.9232 ms** / full 134.9226 ms. CPU 14.9771 ms, Draw 5.6583 ms, EndFrame
   **113.6250 ms**. That frame performs neither reload nor resource polling. The matching
   submission **24,416** completes with GPU interval **0.448736 ms**, observed at frame
   24,417; the 0.445376 ms GPU value printed on the spike row belongs to an earlier submission.

DX12 soak has 272/95,701 measured cycles over 33.3333 ms and zero over 66.6667 ms.
Vulkan has 211/95,772 over 33.3333 ms and **one** over 66.6667 ms. The p99 budget permits
some individual 33.3333 ms exceedances; none of the maximum-budget failures is waived.

EndFrame wraps native command finalization, queue submission/presentation and metric reads.
These wall clocks cannot distinguish API blocking from thread descheduling or prove a driver/OS
root cause. GPU command timestamps exclude host queue/presentation waits. No kernel scheduling
trace was captured; the earlier policy-rejected ETW probe is retained in the preceding evidence.
Do not change shared engine behavior or blame CPU age from this aggregate alone. Follow-up
**nexora-j4q.2.6** requires native API/thread-cycle/scheduling correlation. The historical
DX12 72.3556 ms cause under **nexora-j4q.2.4** remains independently untraced.

## Resource observations

| Soak | Sampled RSS peak MiB | Private peak / median growth MiB | Local GPU usage peak / median growth MiB | Handles first -> last median | Buffer reallocations |
|---|---:|---|---|---|---|
| dx12-soak | 74.6211 | 98.3320 / 0.3984 | 17.1289 / 0.0000 | 369 -> 365 | 2 -> 2 |
| vulkan-soak | 99.1797 | 147.4844 / -0.0078 | 20.8359 / 0.0000 | 472 -> 464 | 2 -> 2 |

Resource polls occur after frame-local destruction every 60 frames and their overhead is
included in full/cycle time. Fixed caps: sampled RSS/private/local GPU <=512 MiB each;
first/last five-minute median growth <=32/32/16 MiB; handles <=4096 with growth <=8;
native buffer reallocations <=8 with zero median growth. All pass. Both soaks perform
161 balanced scoped allocations/frees (including warm-up), 327 texture uploads and two
native buffer reallocations, with zero rejected textures.

These are post-frame sampled peaks, not OS lifetime/intraframe peaks. Tagged allocator
statistics cover only the explicit loading allocation (8 MiB peak, zero live bytes), not all
STL/engine/driver allocations. DXGI node-0 local/nonlocal usage is process WDDM accounting
on the uniquely matching PCI device, not physical VRAM residency or per-texture allocation.
A 30-minute bounded-resource result does not prove that the entire engine is leak-free.

## Validation and reproduction

- Full local MSVC Development build and Monolithic Shipping benchmark/compiler build: PASS.
- Documentation regression: **16/16**; all six changed Markdown documents/paired roadmaps validate.
  Touched C++ clang-format checks pass; root README remains concise and unchanged.
- Related CTest: **34/34**, zero failures/skips, **114.92 s**; see [raw log](ctest-final.log)
  and [JUnit](ctest-final.xml). `twod.integrated_stress` runs 132 raw frames and compares
  engine queries/culling and runner expectations with independent all-object geometry.
- Both native duration-control smoke tests pass 1,200-frame warm-up, one-second minimum,
  teardown, complete frame/GPU sample counts and resource observations. They are functional
  smoke checks, not performance gates; CPU regression work overlapped one such smoke run.
- An initial smoke validator incorrectly assumed stationary query hit counts. Movement
  changes mixed hits from 213 to 215 at frame 119. The corrected geometric oracle is checked
  independently; the [original failed validator log](duration-smoke-oracle-failure.log)
  and unchanged native raw smoke data are retained. No engine work or measured formal frame
  was removed to fix that validator.
- Linux Development configure/build/full CTest and Linux Shipping use the normal hosted PR
  workflow. This physical Windows record makes no local Linux/macOS/clean-host claim;
  final CI status is tracked on the delivery PR independently of the failed physical gate.

```powershell
python Tools/TwoD/RunIntegratedAcceptance.py --benchmark build/windows-m6-soak-20261018/candidate-benchmark.exe --output build/windows-m6-soak-20261018/formal --seconds 1800
```

Use a fresh output directory and a rebuilt/frozen Shipping executable. The actual absolute
commands for every run are in the immutable plan. Large raw traces are byte-preserved gzip
files; [archive map](archives.json) hashes both compressed and decompressed bytes. The
manifest excludes itself. Source snapshots use `.cpp.gz`/`.h.gz` to avoid source globs.
No executable or generated build tree is committed.

Full **2D-M6 remains NOT ACCEPTED**: current strict physical performance failures remain,
and product minimum specification, total allocator accounting/true VRAM residency and
production workload acceptance are separate. M7 device/audio/DPI/clean-host/Editor gates
were not run by this suite. Keep **9/11 (82%), P0 6/8** unchanged.
