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

✅ [PR #290](https://github.com/jimlee1972/Nexora/pull/290) passed the
[documentation-only Build run](https://github.com/jimlee1972/Nexora/actions/runs/37218732492)
for source `3b02a86bab7e82d70691a081c71553514bb5b24f`.

Observed result:

- Documentation and change detection succeeded and selected `documentation only`.
- All nine expensive job groups were skipped, without compiling, testing the engine or packaging.
- CI result succeeded and reported `documentation only`.

Only two lightweight jobs ran; both passed. The remaining nine jobs reported `skipped`.
This report is updated after that observed run; the reporting update also contains only Markdown.
Release and repository branch-protection settings are unchanged; tags still select full Build.

## 繁體中文

✅ PR #289 的 hosted Build 已通過 18/18 工作，包含分類／文件驗證、既有完整矩陣及
`CI result`。原始碼、合併版本與 workflow 連結如上；本機 CTest 的單一顯示測試 skip
與 hosted 驗收分別記錄。

✅ 本紀錄及 README 連結的純 Markdown 變更已在 PR #290／Build 37218732492 實際通過：
只執行分類／文件檢查與 `CI result` 兩個輕量工作，兩者成功；其餘九個昂貴工作群組
全部 `skipped`。原始碼版本與日誌連結如上；本紀錄在觀察結果後更新，回報更新本身
仍只含 Markdown。Release、branch protection 不變，tag 仍跑完整 Build。
