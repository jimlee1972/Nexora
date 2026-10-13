# ED-M6 retained mesh memory capture interchange — 2026-10-13

Beads: `nexora-62u.2.10`. Base: merged PR585/main169ed71f. Supporting cloud core slice;
full ED-M6 and physical milestone acceptance remain open (0/8).

## Contract

Explicit `ProjectWorkspace::ExportMeshMemoryJson` writes `.nexora/mesh-memory.json` atomically;
`ImportMeshMemoryJson` owns one bounded ordered stream, permitting read-only observers. The new
schema preserves exact decimal uint64 sequence/drop/native allocation/CPU-key bytes, finite
monotonic elapsed milliseconds, measured zero and paired unavailable null. Source/native scope/
CPU payload scope/units/time origin/project destination are validated. Process-local surface domains,
native handles, paths, device identity and absolute clocks are absent. Scope excludes textures,
frame uploads, allocator overhead, RSS and total device VRAM; history can span projects of one
surface. Existing wall/process/GPU schemas and files remain independent. Graphical controls/static
publication remain separate work; this change supplies core API only.

Shared validation rejects empty/over600 histories, partial metric pairs, zero/nonincreasing sequences,
negative/nonfinite/nonincreasing time. Nonrecursive token reader rejects unknown/duplicate/missing
fields, wrong metadata, invalid samples and trailing data, accepts reordered fields/ASCII escapes,
and reads at most192KiB. Writer lease/recovery/unsafe destination/staging/replace failure preserve
last-good data. Closed workspaces reject IO; read-only import succeeds but export rejects. IO is
externally serialized; concurrent filesystem substitution remains outside the contract. Synchronous
spans are not retained; imported captures own storage. Additive C++ consumer rebuild required;
existing class layouts, stable C/Zig/scene schemas and module dependencies unchanged.

## Validation

- Targeted native allocation/material pixel/drain round trip plus core capture and independent
  normal/optimized Python readers:4/4 passed1.25s (Vulkan0.70s).
- `cmake --preset linux-development` and `cmake --build --preset linux-development`: passed
  (349 actions after targeted preparation; native graphical Editor/SDK/Slang enabled, headless off).
- `ctest --preset linux-development --output-on-failure`:253/253 passed, no failures/skips,837.36s;
  native Vulkan allocation/pixel/drain round trip0.74s, core0.10s, normal/optimized parser0.15/0.29s.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed5 actions,
  Minimal Monolithic/native Vulkan, Editor disabled.
- Six changed C++ files: clang-format23 checks passed. DocumentationCI16 passed; changed Markdown
  relative links, synchronized EN/zh-TW, unchanged concise root README and diff checks passed.
- Final-head hosted CI is required before merge; platform results will be recorded in the PR/Beads.

Core checks include uint64 max and above2^53 precision, zero and null, finite double precision under
comma locale,600 maximum-sized samples, declared and actual limits, exact/over byte budgets,
truncation/every-byte corruption, reordered/escaped fields, retained prior capture, unrelated staging,
wrong metadata, read-only/recovery/closed workspaces, directories/symlinks and independent files.
Native Vulkan captures actual positive retained allocation/CPU-key bytes through all original five
material readback/reimport/version-rejection/Undo/reopen checks and released zero/unavailable sample;
all six observations round-trip without changing units/elapsed time/values. Platform fixture source
is Linux/X11; hosted Windows/macOS portable tests do not certify new native device-memory calibration.
