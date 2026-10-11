# ED-M5 scoped material source publication — Linux, 2026-10-11

Beads: `nexora-032.1.8`, depending on the material document owner
`nexora-032.1.7` / [PR 528](https://github.com/jimlee1972/Nexora/pull/528).
Review-only foundation `c1d2f254` starts from Main
`1023b19aac8ad84ffd1b7b05a429c3e6f295cfad` plus pending native SDK,
corrected scalar tool and material document prerequisites. Never merge the foundation.
Main delivery requires prerequisite integration, a feature-only replay and fresh gates.
Full Editor milestones remain **0/8**; graphical controls and native GPU Preview are open.

## Source owner and publication contract

`MaterialToolSourceSession` privately owns the material document and borrows workspace/Content
owners that outlive it. Open binds exact project/root, Content generation and asset/path,
retaining the original bounded bytes. A distinct, nonwrapping instance identity supplies
document generation; equal project/asset/serial values cannot cross owners. Owning snapshots
remain inspectable after context loss. Construction-thread checks precede mutable owner access.

Apply/history/Save recheck live project, writer access, resolved recovery/external state,
Content generation/path, pending reimport, explicit host authoring allowance and exact raw
baseline. The host supplies its Play/modal gate. Reads reject unsupported material schemas,
growth beyond 64 KiB, target hardlinks/symlinks and parent/root aliases. No mutable document
borrow or native service pointer escapes.

Save preallocates saved strings before IO, rechecks the baseline, uses the shared atomic
helper preserving occupied staging, confirms bytes, refreshes Content and confirms again
before acknowledging. Confirmed Save keeps Undo/Redo; history itself never writes a file.
A failure after publication reports that the source was published without acknowledgement,
retaining the old baseline, serial and dirty/history state. It never reports false success
or blindly retries the obsolete baseline. Explicit inspection/discard/reopen resolves it.
Close releases stale owners without IO; dirty close/replacement requires explicit discard.

This is the serialized ordinary-file contract, not hostile filesystem race exclusion or
a power-loss recovery journal. There are no native callbacks, automatic trust/enablement,
plugin grants, graphical controls or GPU preview in this slice. Module dependencies,
persisted formats and required C/gameplay ABI stay unchanged; C++ consumers rebuild.

## Actual file and owner acceptance

The test creates a real project and persistently indexed Unicode-path `.nmaterial` source.
It checks unchanged identity sidecars and unmodified disk during in-memory edits; actual
confirmed Save refreshes Content's owning material. Twenty Undo/Save/Redo/Save cycles perform
forty writes while retaining independent history, exact baselines and other material lanes.
Close/reopen reads the actual published source rather than an in-memory substitute.

Distinct owners reject exchanged observations. Wrong-thread, disabled authoring, stale serial,
read-only observer, recovery journal, external workspace/source changes, changed asset path,
missing asset, rebound workspace and changed Content generation reject and preserve state.
Occupied `.tmp` bytes and hardlink aliases remain unchanged; Linux symlink/parent aliases
and oversized sources reject. A real Content material-capacity failure occurs after source
publication: Save reports publication without acknowledgement, retains dirty history and
rejects old-baseline retry. Explicit reopen plus restored Content capacity recovers actual
published values. No fake callback or mocked publication failure is used.

## Initial focused Linux gate

Graphical Development with OpenSSL, Slang, Zig, Showcase, native ProjectPlayer and scalar tool
ON built the import/document/source-session targets in **43 steps**. The three focused tests
passed **3/3 in 0.82 s**; source-session acceptance took **0.23 s**.

## Complete graphical Linux gate

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_CRYPTOGRAPHY=ON -DNEXORA_ENABLE_SLANG=ON \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON \
  -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON \
  -DNEXORA_FEATURE_SCALAR_MATERIAL_TOOL=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
```

Configured Linux toolchain and pinned ImGui/Vulkan dependency caches were used. Fresh full
Development built **344 steps**, **252/252 in 638.60 s**, zero failed/skipped. Minimal
Monolithic Shipping passed **5 steps**, with Editor/SDK/graphical shell/scalar tool OFF.
Actual Linux native Editor display, Scene/Game, Inspector and file-publication regressions
are included; Xvfb/software Vulkan do not claim physical-display acceptance.

## Explicit scalar-OFF portable gate

```sh
cmake --preset linux-development -B build/linux-material-source-feature-off \
  -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF -DNEXORA_ENABLE_NATIVE_BACKENDS=OFF \
  -DNEXORA_ENABLE_WINDOW_PRESENTATION=OFF -DNEXORA_ENABLE_CRYPTOGRAPHY=ON \
  -DNEXORA_CRYPTOGRAPHY_BACKEND=NONE -DNEXORA_ENABLE_SLANG=OFF \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_BUILD_SHOWCASE=OFF \
  -DNEXORA_BUILD_PROJECT_PLAYER=OFF -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=OFF \
  -DNEXORA_FEATURE_SCALAR_MATERIAL_TOOL=OFF
cmake --build build/linux-material-source-feature-off -j4 --target \
  NexoraMaterialToolSourceSessionTests NexoraMaterialToolDocumentTests NexoraMaterialImportTests
ctest --test-dir build/linux-material-source-feature-off --output-on-failure \
  -R '^editor\.(material_import|material_tool_document|material_tool_source_session)$'
```

This independent configuration built **92 steps**, **3/3 in 0.79 s**; source-session
acceptance took **0.24 s**. Current configured targets/tests exclude the native scalar tool
while retaining source/document owners. The initial post-test cache inspection used a
nonexistent `NEXORA_FEATURE_EDITOR` field and failed; corrected inspection of the actual
`NEXORA_ENABLE_EDITOR`/SDK/shell options passed without code or assertion changes to tests.
No native provider or signed callback is fabricated in this mode.

Documentation CI passed **16/16 in 0.790 s**. Changed links (including this new evidence),
touched C++ formatting and diff checks passed. No other-platform local, graphical Shipping
or native graphical material-tool/GPU Preview execution is claimed.
