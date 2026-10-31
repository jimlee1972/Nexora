# ED-WF-03: runtime-only game output and external publication receipts

Status: implementation in progress, **not accepted**. GitHub [#744](https://github.com/jimlee1972/Nexora/issues/744),
Epic [#741](https://github.com/jimlee1972/Nexora/issues/741), Beads `nexora-whr.3`.
Base: ED-WF-02 merge `1232574620ba784da1b3a607780986790e831b52`; reuse the existing Editor branch.
Accepted workflow progress stays **2/9**; graphical Editor milestones remain **0/8**.

## Implementation and acceptance boundary

- Runtime staging contains Game executable, cooked data/launch metadata, optional module,
  trusted runtime libraries and LICENSE. `Build.commands`, `Build.sources`, `Build.tools` and
  `Game.inventory` are staged separately and never copied into the runtime directory.
- A trusted optional host `records_parent` selects physical storage outside project/operation/game;
  otherwise engine user data is used. The CLI exposes additive JSON `receipt`/`records` locators
  and optional `--build-records=PATH`. This is not an authored setting or another graphical root.
- Complete private/runtime/record snapshots, current external records and owner/source/cancellation
  guards are checked before commit. A project/output binding detects record-key collisions.
  Foreign/changed files or aliases reject; records cannot occupy authored/runtime directories.
- Game and receipt directory replacements form a recoverable transaction. A real receipt-path
  filesystem failure after game replacement restores the different previous runtime payload;
  recovery also runs on exceptions. Failed rollback retains backups and normal errors identify
  their locations. This does not promise power-loss atomicity or hostile-writer isolation.
- Prior records remain in engine-user-data history. A complete verified legacy colocated inventory
  is required before migration; old metadata is retained as external `Legacy.*` files. Damaged
  legacy output rejects unchanged. A moved game needs no receipt to execute; replacing an existing
  clean output requires its valid records rather than claiming arbitrary files.
- SDK/compiler/CLI paths remain shared. C/Zig gameplay descriptor layouts are unchanged;
  public C++ consumers rebuild for the appended request/receipt API. Optional Asset Pipeline
  requirements and existing GUI Windows Player entry remain unchanged.
- Test-only evidence now observes the owned running process snapshot to wait for the actual
  second successful native present before closing. The original 45-second window and 360-second
  overall bounds and normal-exit/no-console/two-frame assertions remain. Completed worker snapshots
  no longer overwrite the owner's terminal Published phase with ReadyToPublish.

Contracts: [shared producer](../../../../Engine/Editor/GameBuild.md),
[Editor Core](../../../../Engine/Editor/README.md), [application](../../../../Apps/Editor/README.md).

## Native Windows validation

Existing Windows developer host, MSVC 19.44, CMake 3.31.6-msvc6, Ninja 1.12.1, Zig 0.14.0,
Development native DX12, Vulkan OFF, graphical Editor and native Player ON. Cryptography provider
is unavailable locally/fails closed; this is not a complete local Windows all-module gate or a
clean-host/device milestone. Full hosted matrices remain required.

Build executable:
`D:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`.

```powershell
cmake --build build/editor-workflow-native --config Development --target NexoraEditor NexoraGameBuildTests -j 4
ctest --test-dir build/editor-workflow-native -C Development --output-on-failure -R '^editor\.game_build$'
ctest --test-dir build/editor-workflow-native -C Development --output-on-failure -R '^editor\.windows_build_workflow$'
ctest --test-dir build/editor-workflow-native -C Development --output-on-failure -R '^editor\.(game_build|windows_package_admission|bounded_build_process|game_build_progress|game_build_sdk_bytes)$'
```

- Initial real producer gate passed 239.82 seconds; expanded unsafe-path/legacy/rollback/read-only
  producer gate passed again in 229.82 seconds, exit 0. Actual CMake/Zig compilation and consumer
  admission ran, not helper-only publication. A modified external record rejects; a timed real
  filesystem failure commits then rolls back a different Game.launch; corrupt legacy output rejects;
  valid legacy metadata migrates; read-only script builds leave the authored file set/bytes intact.
- Native GUI gate passed on **Default input desktop**, 263.45 seconds, exit 0. Saved Unicode/space
  manifest/output paths, actual controls, source-free runtime-only contents, external receipt,
  direct single responsive Game window/no console, two successful presents/normal exit, repeated
  Build & Run/Stop, compilation/stale-source/cancel protection and drained children were checked.
  The retained log also verifies Published remains terminal when native readiness is observed.
  [Sanitized source/binary/runtime hashes and receipt](ED-WF-03-Windows.json).
- Package admission passed 0.86 seconds; subsequent progress/SDK-byte/process/package checks passed
  before an additional negative-test reason assertion was corrected. The final combined core gate passed **5/5**, exit 0, 304.74 seconds, including the
  expanded actual producer in **300.83 seconds**. No complete-pass claim was inferred from a partial run.

## Retained failures and environment diagnosis

Both earlier GUI attempts ran on `CodexSandboxDesktop`, while `OpenInputDesktop` reported Default.
They created a responsive window but could not complete actual DX12 presents: first failure 90.07
seconds, then 127.78 seconds while waiting for genuine readiness. Read-only desktop queries identified
the mismatch. The approved native GUI invocation ran on the real input desktop and passed; product
rendering, pixel assertions and timeouts were not changed to manufacture a pass. The fixture now
checks this prerequisite explicitly. It operates only on its owned Editor/Game processes/windows.

The new metadata-dependency negative test initially expected the reserved-name error, but the
existing earlier library-extension guard had already correctly rejected the non-library. Its reason
assertion now checks that actual rejection, still requires failure/preserved output, and additionally
asserts the tool file is absent from output. The failed 5.90-second attempt and complete failed
invocation remain retained. The first development compile's incorrect workspace include was fixed
to the actual EditorWorkspace public header; original build log is retained.

Logs remain under ignored `build/editor-workflow-native/`: `ed-wf03-game-build.log`,
`ed-wf03-native-acceptance.log`, `ed-wf03-gui-ready.log`, `ed-wf03-interactive-gui.log`,
`ed-wf03-final-core.log` and retry logs, plus MSBuild logs and archived native attempts under
`Tests/Editor/ed-wf03-attempt-*`. No private desktop image or temporary-user path is published.

## Remaining gates

Native core/GUI gates above passed. Source/diff/format/document review, complete exact-head
Codex Cloud Development configure/build/CTest and Shipping configure/build, hosted platform CI,
and this batch's PR/merge are pending. No Windows/macOS run is inferred from Linux. ED-WF-04–09,
the new final Editor ZIP and clean-host/physical acceptance remain separate and open.
