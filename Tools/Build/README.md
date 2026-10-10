# Build CI routing

The `Build` workflow always starts for pushes and pull requests. Its `Documentation and change
detection` job uses the complete Git diff, validates changed Markdown, and selects either a
documentation-only run or all existing build, test, and package jobs. `CI result` always reports a
final result and fails if change detection or any selected job fails or is cancelled.

## Documentation-only scope

- All changed paths must end in `.md` (case insensitive).
- `.github/`, `Tests/`, `Content/`, `AGENTS.md`, and `CLAUDE.md` require full CI even when Markdown.
- Any other file type, a mixed change, an empty/unknown comparison, or a version tag requires full CI.
- PRs compare the merge base with their head; ordinary pushes compare before/after commits. A new
  topic branch compares against the default branch. A new default branch uses full CI.
- Git rename detection is disabled, so renaming code to Markdown cannot hide the old code path.
- The tag-triggered Release workflow remains unchanged; tag pushes still run the full Build gate.

The documentation validator checks UTF-8, nonempty content, final newlines, closed fenced blocks,
and the existence of local Markdown links/images in changed files, using a pinned CommonMark parser
for inline/reference links, balanced parentheses and list containers. It skips code examples,
remote URLs, and heading-anchor validation. Roadmaps in `Roadmap/en/` and `Roadmap/zh-TW/` must
exist, change, or be deleted as bilingual pairs. A maintained mapping covers differently named
legacy pairs; new same-name pairs are required automatically. This checks paired updates, not
translation equivalence. When the comparison is unavailable,
changed-document validation is unavailable and full CI is required.

Existing build check names remain available as skipped checks on documentation-only changes.
`CI result` is the stable aggregate check to use for branch protection; this change does not edit
repository protection settings. The whole workflow is not filtered with `paths-ignore`, avoiding
required workflow checks left pending by an omitted run.

## Local validation

For the exact cloud Editor branches listed in `editor-ci-cleanup.cjs`, change detection cancels
only queued/running `build.yml` runs from superseded heads in this repository. It rereads the
branch before each cancellation and stops when the head changes. Current-head push and PR runs,
main, foreign repositories and unlisted branches remain eligible for their complete CI gates.
Cleanup errors are warnings; they never override build acceptance. The existing actions-write
permission and legacy ED-M0 allowlist remain unchanged. Run the eight guard cases locally with
`node Tools/Build/tests/editor-ci-cleanup.test.cjs`.

```bash
python3 -m pip install --requirement Tools/Build/documentation-requirements.txt
python3 Tools/Build/TestDocumentationCI.py
python3 Tools/Build/DocumentationCI.py --event /path/to/event.json --event-name pull_request
```

The scripts use Git and pinned `markdown-it-py`/`mdurl` packages. Workflow changes themselves require full CI.

Routing-test repositories disable automatic Git maintenance and GC in their own local config
before the first commit. Detached housekeeping must not outlive these temporary fixtures or race
their strict cleanup. Production repositories, user/global Git settings, routing decisions and
cleanup error handling are unchanged; cleanup failures still fail the tests.

## 繁體中文

`editor-ci-cleanup.cjs` 只對明列的本次雲端 Editor branch 清理本 repo 舊 head 的 queued／running
`build.yml` run。每次取消前重新讀取 branch；head 推進即停止。當前 head 的 push／PR run、main、
其他 repo 與未列 branch 均保留完整 CI。清理失敗僅回報 warning，不取代驗收；既有 actions-write
permission 與 ED-M0 清單維持原樣。可執行 `node Tools/Build/tests/editor-ci-cleanup.test.cjs`
驗證八個 guard case。

`Build` 對 push 與 PR 持續啟動。先使用完整 Git diff 判斷範圍，驗證變動的 Markdown，再選擇
純文件或完整建置／測試／封裝。純 `.md` 可走文件路線，但 `.github/`、`Tests/`、`Content/`、
`AGENTS.md`、`CLAUDE.md`、其他副檔名、混合變更、未知／空白比較與 tag 都保留完整 CI。
新 topic branch 與 default branch 比較；PR 使用 merge base。停用 rename detection，避免
程式改名成 `.md` 後漏掉完整驗證。

文件檢查涵蓋 UTF-8、非空內容、檔尾換行、code fence 與本地 link／image 路徑存在；使用
固定版本 CommonMark parser 處理 reference link、平衡括號與清單縮排，略過程式碼範例、
遠端 URL 與 heading anchor。Roadmap 中英文文件要求成對存在、更新或刪除；異名舊文件
以映射配對，新增同名文件自動要求另一語言版本；不自動驗證翻譯等價。無法比較時，文件
差異驗證不可取得，改跑完整 CI。

`CI result` 固定回報結果，分類失敗或應執行工作失敗／取消皆會失敗。既有建置工作在純文件
變更時呈現 skipped。可將 `CI result` 作為 branch protection 的彙總檢查；本變更不修改
保護設定。Release 不變，tag 仍要求完整 Build。

Routing test 的短期 Git fixture 在第一個 commit 前以自己的 local config 關閉自動
maintenance／GC，避免 detached housekeeping 超出 fixture lifetime 並與嚴格 cleanup 競爭。
正式 repo、user／global Git 設定、路由判斷與 cleanup error handling 都維持原樣；cleanup
失敗仍使測試失敗。

## Courtyard asset conversion

`PrepareCourtyardAssets.py` converts only the pinned CC0 KayKit single-node, single-primitive
untransformed GLBs and RGBA8 palette atlas in `Content/Showcase/Courtyard`. It verifies source
hashes, bounded accessors, triangle indices, finite streams, PNG CRC/filter data and bounded
decompression. Unsupported transforms/external buffers reject rather than being silently lost.
The generated private Showcase header retains position/normal/UV identity and a 64x64 area-filtered
atlas; authoring payloads still pass through Runtime import/cook/bundle activation before use.
`--check` and rejection tests run through CTest without Pillow or a runtime glTF dependency.
Original sources/license/inventory ship through the existing checksum-verifying package flow.

## Pose Search profile verification

`VerifyPoseSearchProfiles.py` configures six isolated CMake File API fixtures without SDK downloads.
It checks the actual compile-source and target graphs: Development and Monolithic Shipping Full
include PoseSearch with only a Foundation dependency; explicit OFF, Shipping Minimal/Dedicated,
and headless builds omit its implementation and target. `build.pose_search_profiles` runs this
gate through CTest. It does not replace compiling/testing the enabled library.

`VerifyAnimationProfiles.py` checks the optional Foundation-only `NexoraAnimation` target and source
graph in Development, explicit OFF, Shipping Minimal/Full/Dedicated, and headless profiles. Only
Development and Shipping Full include pose storage/retarget code; Full uses Monolithic linkage.

## Shipping optimization

Custom `Shipping` configuration names do not inherit CMake's Release compiler flags.
`NexoraBuildConfig.cmake` explicitly selects `/O2` on MSVC and `-O2` on other supported compilers,
in addition to the existing required IPO/LTO. Development and Debug retain their existing settings.
This policy retains descriptor checks, assertions and precise floating-point semantics; it does
not introduce `NDEBUG` or fast-math. It applies with both single- and multi-configuration generators.

自訂 `Shipping` 設定名稱不會繼承 CMake 的 Release 編譯旗標。建置政策在既有 IPO／LTO
之外明確指定 MSVC `/O2` 或其他支援編譯器 `-O2`；Development／Debug 維持既有設定。
保留描述資料驗證、assertion 與精確浮點語意，不加入 `NDEBUG` 或 fast-math；單一與多重
configuration generator 均適用。效能改善需由目標主機實测驗證，不能由旗標直接宣稱驗收。
