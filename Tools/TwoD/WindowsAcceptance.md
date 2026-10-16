# Windows／MSVC 實機驗收

目標是實際交付的 x64 Monolithic Shipping 遊戲封裝。Linux 軟體 Vulkan、ALSA null 與 hosted
Windows CI 只能提供自動化證據；下列項目必須在 Windows 目標機驗證，不能由提交成功推定通過。
遊戲規則、正式美術、WAV 與有授權的中文字型由遊戲專案提供。

先依 [2D Player 指南](../../Apps/TwoDPlayer/README.md) 建立專案描述檔、烘焙字型與封裝。
在 MSVC developer shell 建置對應 Shipping preset 的 `NexoraTwoDPlayer`、
`NexoraTwoDAssetCompiler`、`NexoraTwoDAudioProbe`；編譯 Zig DLL 時使用相同 x64 目標。
匯出時提供匹配的 MSVC CRT 目錄。把整個輸出資料夾複製到目標機，雙擊 `Run.cmd`。
保留 `inventory.json`、Windows/CPU/GPU/驅動資訊、實際操作記錄與必要截圖；錄到畫面不代表聽到聲音。

| 實機項目 | 操作與通過條件 | 要保留的證據 |
| --- | --- | --- |
| 鍵鼠／重綁 | 測試所有遊戲動作、滑鼠按鍵與滾輪；重綁後舊鍵不再觸發，新鍵與 UI 操作一致。若遊戲提供設定保存，重啟後確認保留。 | 按鍵／動作表、重綁前後結果 |
| XInput 手把 | 測試實際手把的按鈕、搖桿死區、方向鍵、確認／取消；遊戲中插拔與重新連接；鍵鼠／手把切換後裝置提示正確，沒有卡住的按鍵。 | 手把型號、連線方式與插拔結果；非 XInput 裝置不在此後端範圍 |
| 焦點與視窗 | 持續移動／按住按鍵時 Alt-Tab、點到其他程式、最小化、恢復；失焦後不繼續操作或殘留滑鼠狀態；Editor 中擷取 Game View 的第一次點擊不觸發遊戲。 | 失焦／恢復與擷取結果 |
| 音效／背景音樂 | 先執行 `NexoraTwoDAudioProbe --frames 48000` 並由人確認真的出聲；再在遊戲中分別調整 Master／Music／SFX，測試播放、暫停、停止與循環。測試預設裝置切換、耳機／USB 裝置拔除與恢復，記錄後端實際結果。 | probe JSON、輸出裝置、人工聽音結果、恢復結果；提交音訊資料本身不算通過 |
| 中文 UI／DPI | 使用正式語料與指定字型，確認所有中文字、標點、數字與換行；選卡、HUD、按鈕焦點／取消、滑鼠命中區正確。測試 100%／150%／200% DPI、縮放視窗與跨螢幕移動。缺字必須報錯，不能把方框當成完成。 | 字型／語料版本、各 DPI 截圖與操作結果 |
| 永久存檔／續局 | 在遊戲設計的存檔點保存，完整離開再啟動；確認狀態還原。確認 `%LOCALAPPDATA%/Nexora/Games/game-<id>/saves` 與不同專案隔離；測試中文使用者路徑、寫入失敗和損壞資料；失敗不能靜默覆蓋原檔。 | 前後狀態、slot/schema、錯誤顯示與重啟結果；正式 schema／遷移由遊戲實作 |
| 乾淨電腦啟動 | 在未安裝 Editor、Zig、Python、MSVC developer tools 的 Windows 電腦雙擊 `Run.cmd`；封裝內 CRT、DLL、素材齊全，以硬體 DX12 呈現。確認不依賴開發機 PATH、原始專案或網路。 | 實際目標機配置、封裝 hash、啟動／退出結果 |
| 關閉與長時間運行 | 重複啟動／退出、Editor Play／Stop、音訊開關與裝置恢復；正常關閉後沒有殘留程序、聲音或檔案鎖。以目標最低規格執行真正整合 UI／特效負載，量測持續幀時間與記憶體。 | 次數、持續時間、程序／資源與效能觀測；VM 軟體光柵化不算最低硬體驗收 |

這份清單不宣稱已完成上述實機驗收。主 Editor 的 2D Play 綁定仍限於所選的一個場景與凍結的
Atlas／Clip；沒有自動保存 authored Sprite、Tilemap 專案綁定或無縫熱更新。獨立 Player 可用
`tile_atlas` 配置 Tilemap。正式遊戲畫面、玩法數值、選卡內容與存檔 schema 不由引擎測試素材代替。
M6／M7 的完整實機驗收狀態以 [2D roadmap](../../Roadmap/zh-TW/2D_Roadmap.md) 為準。
