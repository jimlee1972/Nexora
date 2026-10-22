# Windows Editor local baseline and retained 100k stress failures - 2026-10-22

Status: ✅ local Windows baseline passed; full-suite / 100k stress acceptance remains blocked. Documentation-only scope change; no engine,
fixture, timeout, assertion, CTest registration or CI configuration changes.

## Accepted scope versus retained failures

The user separated the three 100,000-node CPU/ImGui fixtures from this developer PC's basic
host validation, then requested continued validation. This does not establish that hardware
insufficiency caused the timeouts, and does not accept a smaller node-count guarantee.
The full repository gate remains unfiltered. Before the scoped run completed, the user-revised
local criteria were recorded explicitly on `nexora-nqn.4.1`; it may close only after all revised
local requirements pass. The three unchanged 100k failures remain blocked as `nexora-wo1`.

The original run used source `0c24ec75330ab5c94721847abca7656c5cb5e272`, Windows 10 19045,
GTX 960 / driver 32.0.15.8180, and one 100-percent monitor. Configure/build passed; full
CTest exited 8 with 303 passed, 3 timeouts and 6 skips:

| Test | Original deadline result | Current local stress acceptance |
| --- | --- | --- |
| editor.deep_hierarchy | Timeout 180.08 s | blocked |
| editor.hierarchy_scale_lifecycle | Timeout 600.09 s | blocked |
| editor.overview_pose_snapshot | Timeout 180.09 s | blocked |

The separate native smoke exited 0 with acquired=240, presented=240, ui_draws=1915,
ui_uploads=1, ui_rejected=0 and software=0. Both required ImGui/Windows DPI-IME contracts
passed. This does not validate installed IME composition, physical DPI or keyboard recovery.
Original raw evidence is retained at `G:/proj/Nexora/build/nqn-acceptance-20261022/`;
its `artifact-manifest.json` was verified across 80 artifacts. Do not overwrite that packet.

## New local baseline execution

✅ Unchanged graphical Windows Development configure/build passed (Ninja: no work to do).
The exact local selection completed with 303 passed, 6 explicitly skipped and zero failures
(out of 309 selected; exit 0). Full registration=312, selected=309 and excluded=3
were archived and cross-checked by name; both required ImGui/Windows DPI-IME contracts passed.
The complete six native artifact/device skip names and original messages remain in raw logs
and `evidence.json`; skips are not hardware-feature passes.

The fresh 240-frame native smoke passed with exit 0 and no recognized backend validation errors:

```text
graphical evidence: acquired=240 presented=240 ui_draws=1915 ui_uploads=1 ui_rejected=0 scene_draws=0 recovery=none selector=bypassed access=read-write schema=2 upgrade=applied recents=1 scene_nodes=1 scene_selected=0 scene_documents=1 scene_references=0 pie_ticks=0 pie_steps=0
GPU timing evidence: scope=native_command_buffer_interval unit=milliseconds source=2 software=0 completed=238 retained=238 dropped=0 latest=1.1391
```

Editor SHA-256: `3309b69436408ac2a4833a1336c75579f78da51abbfb8299bac49ce8e5001d34`. All 20 frozen Editor/runtime DLL hashes
were verified; no owned residual processes remained. The CPU reports AMD FX-6100; actual
OS/GPU/driver/monitor/locale details are recorded in `host-inventory-current.json`.
These CPU/ImGui fixture results do not define a minimum hardware specification.

New packet: `G:/proj/Nexora/build/nqn-local-baseline-20261022/`.
It retains exact command argv/exits/UTC timestamps in `stages.jsonl`, full verbose raw stdout/stderr,
`test-scope-inventory.json`, source/binary hashes, CMake cache/preset, host metadata,
residual-process record, `evidence.json` and a SHA-256 `artifact-manifest.json`.
The previous full-suite packet was rehashed and remains unchanged (80 artifacts).

Only the explicitly revised local-baseline criterion of `nexora-nqn.4.1` can be completed by
this record. `nexora-wo1` and manual `.4.2`/`.4.3` gates remain blocked; no Editor milestone is accepted. Linux/macOS/mobile, clean-host,
full 100k stress and physical DPI/IME/keyboard/docking/recovery remain separate.

## Reproduction commands and documentation checks

Run in an x64 MSVC Developer shell from the isolated source root:

```powershell
cmake --preset windows-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_VULKAN_BACKEND=OFF
cmake --build --preset windows-development --parallel 4
ctest --preset windows-development -N
$stress = '^(editor\.(deep_hierarchy|hierarchy_scale_lifecycle|overview_pose_snapshot))$'
ctest --preset windows-development -N -E $stress
ctest --preset windows-development -N -R $stress
ctest --preset windows-development -V -E $stress --output-log G:/proj/Nexora/build/nqn-local-baseline-20261022/local-baseline-development-tests.log
```

Smoke used the frozen `NexoraEditor.exe --graphical --project=<fresh-disposable-project>
--recent-projects=<packet-local-store> --frames=240`; the exact argv is in `native-stages.jsonl`.

Documentation verification: `python Tools/Build/TestDocumentationCI.py` passed 16/16;
changed-document validation passed for all five Markdown files and both bilingual pairs.
`git diff --check` passed. These are documentation checks, not Linux CTest evidence.
Root README remains concise and unchanged; overall Editor acceptance remains 0/8.
