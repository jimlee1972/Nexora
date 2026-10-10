# Windows V1 Visual Demo 階段狀態（2026-10-10）

依使用者要求，本階段完成效能修正、證據與文件收尾後暫停。V1 Visual Showcase 整體仍為 **5/7**；VIS-M3 最終美術與 VIS-M6 最終交付未完成。這份狀態以已驗證的 `7c4534b8` 原生執行檔為準，不將後續規劃列為已完成。

## 已驗證成果

✅ DX12 與 Vulkan 皆在既有 fence 完成後重用相容的場景目標，保留尺寸／格式／效果角色失配時重建、原有繪製與像素門檻，以及 resize／teardown 的 drain。Vulkan 重用的同步錯誤已修復，原始失敗報告保留。

✅ Windows Development 完整 configure／build／CTest：**125/125，444.81 秒**，包含原有 DX12／Vulkan 原生 PBR 畫面驗證。

✅ 同來源 `7c4534b850c37fc0ae741ebf2198408f20613368` 的 [GitHub-hosted Build 2105](https://github.com/jimlee1972/Nexora/actions/runs/37978332845) 全部 **18 項通過**，包含 Linux Development 完整 configure／build／CTest、Shipping 與 Xvfb core／同步驗證。先前 Vulkan 修正版 `5aa60d5c` 的 [Build 2092](https://github.com/jimlee1972/Nexora/actions/runs/37974114417) 亦全部 18 項通過。Linux gate 執行於 GitHub-hosted CI；本階段未在 Codex Cloud 執行。

✅ Shipping 封裝／隔離 headless 驗證，以及 GTX 960 三種品質各三次、雙後端合計 **18 份**嚴格原生品質報告均通過。另保留四份 completed-GPU 比較、四份未暫停動畫／UI 測量、headless GPU 不可用驗證與全部較慢幀。

✅ 同一執行檔的 DX12／Vulkan 實體螢幕互動與完整導覽均通過：各 **74 張截圖、51 項 manifest 雜湊**，實際導覽時間分別 **210.011／210.012 秒**，無 issues／fallback／software renderer，最後停在完整導覽 step 6。螢幕可用性沿用操作者聲明；`clean_host_verified` 仍為 false。

## 實際流暢度

以下為原生程式測量，不是影片編碼 FPS。動畫與標準 UI 開啟，各執行 1,200 幀，暖機 60 幀後取 1,140 筆 wall 幀時間。

| Standard 動畫／UI | 上版 `5aa60d5c` FPS | 本版 `7c4534b8` FPS | 本版 p99 ms |
| --- | --- | --- | --- |
| DX12，requested vsync off | 52.00 | 133.43 | 9.35 |
| DX12，requested vsync on | 51.95 | 60.03 | 18.04 |
| Vulkan，requested vsync off | 119.55 | 126.30 | 9.60 |
| Vulkan，requested vsync on | 59.73 | 60.04 | 18.24 |

固定畫面 Standard 三次測量：DX12 **131.52–134.91 FPS**，Vulkan **122.85–125.87 FPS**。DX12 主機幀成本已大幅改善；開啟垂直同步的 p99 仍超過暫定 **16.7 ms** 預算，High Vulkan 另保留 **29.42 ms p99**，因此整體硬體預算尚未驗收。測量未與本專案編譯、CTest、Beads 同步或錄影重疊；其他桌面背景活動未受控制。

## 可執行版本與證據

- Runtime source：`7c4534b850c37fc0ae741ebf2198408f20613368`。
- Executable SHA-256：`257c23cb8051ee736d50bbdb1dcba9f4eaaea6036fecdb1446bb12c1a9b0e518`。
- 本機凍結套件：`G:/proj/Nexora/build/v1-windows-local-2026-10-10/native-demo-7c4534b8/`。
- 執行檔：`bin/NexoraShowcase.exe`。
- DX12 啟動：`G:/proj/Nexora/build/v1-windows-local-2026-10-10/run-dx12-demo-7c4534b8.cmd`。
- Vulkan 啟動：`G:/proj/Nexora/build/v1-windows-local-2026-10-10/run-vulkan-demo-7c4534b8.cmd`。
- 兩個啟動檔預設 Standard、動畫／UI 與 requested vsync on；不設幀數上限，可正常關閉視窗。
- [詳細驗收與命令](acceptance.md)、`shipping-provenance.json`、`hosted-ci.json`、雙後端品質矩陣、GPU／live 測量，以及 `committed-dx12-physical/`、`committed-vulkan-physical/` 保留原始報告與各六張選定截圖。
- [PR #449](https://github.com/jimlee1972/Nexora/pull/449) 承載目前庭院／效能修改；提交與合併狀態以 PR 即時資料為準，不將 PR 合併視為最終美術或硬體驗收。

## 暫停時未完成項目

1. **全部美術接近概念圖**：構圖、石材／青銅、晶體、植被、光影與色彩仍需完整調整。使用者未接受目前候選版作為 VIS-M3 最終美術。
2. **穩定幀時間預算**：paced p99、High 較慢幀與最後美術版本的固定路線仍需再驗證；目前不可宣稱每幀穩定 60 FPS。
3. **錄影採樣修正**：`Tools/Package/RecordVisualTourWindows.py` 仍固定 **10 FPS**；30 FPS／實際採樣頻率／補幀比例檢查只有本機未套用的準備稿，沒有提交或驗收。下一階段先完成此工具修正，再錄製與最終凍結程式一致的雙後端 100 秒影片、精華與截圖。
4. **最終 VIS-M6 同版交付**：美術、經確認的效能預算、影片與效能報告需對應同一最終版本；獨立乾淨主機驗收仍未完成。

Beads 使用既有外部資料庫與 `refs/dolt/data` 同步；本次 DX12 支援工作 `nexora-82p.16` 已依上述完整證據結案，相關 Beads 更新已推送。`nexora-82p.12` 完整美術與 `nexora-82p.4` 影片工作仍未完成。此次暫停不代表關閉 V1 整體目標。
