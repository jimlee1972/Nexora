# Hosted CI routing acceptance

Date: 2026-10-05 (Asia/Taipei).

## Full-build route

✅ [PR #289](https://github.com/jimlee1972/Nexora/pull/289) passed the
[hosted Build run](https://github.com/jimlee1972/Nexora/actions/runs/37217999066):
18/18 jobs succeeded, including Documentation and change detection, the existing desktop,
sanitizer, mobile and package matrix, and CI result.

Validated source: `a6b47554f242fb8ca02841fce2293b636b00e6a9`.
Merged configuration: `d231c7c54aad9c4a355bc6cb626493d074c7b1a6`.

Local validation passed 16 routing/parser/pairing tests, workflow syntax checks, Linux
Development configure/build, and CTest (62 passed, one native-display test skipped).
Hosted results are distinct from this local display limitation.

## Documentation-only exercise

This record and its repository README links are a Markdown-only follow-up on the merged
configuration. Its PR Build run is used to verify the documentation-only route.

Expected result:

- Documentation and change detection succeeds and selects `documentation only`.
- All nine expensive job groups are skipped, without compiling, testing the engine or packaging.
- CI result succeeds and reports `documentation only`.

Hosted documentation-only acceptance is pending until that PR's Build run completes.
Release and repository branch-protection settings are unchanged; tags still select full Build.

## 繁體中文

✅ PR #289 的 hosted Build 已通過 18/18 工作，包含分類／文件驗證、既有完整矩陣及
`CI result`。原始碼、合併版本與 workflow 連結如上；本機 CTest 的單一顯示測試 skip
與 hosted 驗收分別記錄。

本紀錄及 README 連結是合併後的純 Markdown 變更，用來實際驗證文件分流。預期只執行
分類／文件檢查與 `CI result` 兩個輕量工作，其餘九個昂貴工作群組全部跳過。
文件路線的 hosted 驗收待本 PR 的 Build 完成後記錄；Release、branch protection 不變，
tag 仍跑完整 Build。
