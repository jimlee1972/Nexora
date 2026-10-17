# Windows／MSVC 實機驗收

目標是實際交付的 x64 Monolithic Shipping 遊戲封裝。Linux 軟體 Vulkan、ALSA null 與 hosted
Windows CI 只能提供自動化證據；下列項目必須在 Windows 目標機驗證，不能由提交成功推定通過。
遊戲規則、正式美術、WAV 與有授權的中文字型由遊戲專案提供。

先依 [2D Player 指南](../../Apps/TwoDPlayer/README.md) 建立專案描述檔、烘焙字型與封裝。
在 MSVC developer shell 建置對應 Shipping preset 的 `NexoraTwoDPlayer`、
`NexoraTwoDAssetCompiler`、`NexoraTwoDAudioProbe`；編譯 Zig DLL 時使用相同 x64 目標。
匯出時提供匹配的 MSVC CRT 目錄。把整個輸出資料夾複製到目標機，雙擊 `Run.cmd`。
保留 `inventory.json`、Windows/CPU/GPU/驅動資訊、實際操作記錄與必要截圖；錄到畫面不代表聽到聲音。

## Visual Studio 2022 generator 的建置路徑

下列是已在本機執行的 x64 MSVC／DX12 Shipping 組合。先進入 Visual Studio x64 developer
shell，確認 `dumpbin` 在 PATH，並使用專案指定的 Zig 0.14.0。不要在同一個 build 目錄混用
Ninja 與 Visual Studio generator；既有目錄若使用其他 generator，請改用新的 `-B` 目錄。

```powershell
cmake --preset windows-showcase-shipping -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_CONFIGURATION_TYPES=Shipping -DNEXORA_BUILD_SHOWCASE=OFF `
  -DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF `
  -DNEXORA_ENABLE_VULKAN_BACKEND=OFF `
  -DNEXORA_ZIG_EXECUTABLE="$PWD/.tools/zig-win-x86_64-0.14.0/zig.exe"
cmake --build --preset windows-showcase-shipping --target NexoraTwoDPlayer `
  NexoraTwoDAssetCompiler NexoraTwoDAudioProbe NexoraZigTwoDSharedBuild --parallel 2
```

Visual Studio 是 multi-config generator，上述 EXE 位於
`build/windows-showcase-shipping/bin/Shipping/`；Ninja 範例的 `bin/` 路徑不能直接套用。
`NexoraZigTwoD.dll` 位於 `build/windows-showcase-shipping/Gameplay/Zig/`。匯出時使用實際
EXE 路徑，並從 `$env:VCToolsRedistDir/x64/` 選取匹配的 `Microsoft.VC*.CRT` 目錄。
若 configure 改用自訂 `-B <目錄>`，後續必須用 `cmake --build <目錄> --config Shipping`
及相同 targets；`--build --preset` 仍會指向 preset 原本的目錄。

## 既有 VirtualBox VM 的可重複驗收

`WindowsVmAcceptance.py` 驗證明確指定的兩個 Shipping fixture 封裝：ZIP 頂層必須包含
`UI-Shipping/`、`ZigGame-Shipping-Fixture/` 與各自的完整 hash inventory。先讓既有 VM 正常
關機，再執行下列命令；密碼檔保留在 repo 外，不讀出內容或提交。輸出目錄必須不存在。

```powershell
python Tools/TwoD/WindowsVmAcceptance.py --vm <existing-vm-name> --username <guest-user> `
  --password-file <external-password-file> --package-zip <verified-fixture-zip> `
  --vboxmanage <VBoxManage.exe> --output <fresh-evidence-directory>
```

預設執行兩次冷開機，每次跑原生 UI／Zig、跨程序與跨冷開機存檔、Unicode 路徑、損壞存檔／
Cooked 拒絕與正常視窗關閉；只有兩次都完成且觀察到 `poweroff` 才輸出總結 PASS。工具不會
捨棄 saved state、還原 snapshot、強制關機或刪除舊存檔。損壞案例只改本輪的新副本。
每個 VM UUID 以 OS lock 序列化，Guest Control stdout／stderr 以原始 bytes 分開保留，
避免主機 PowerShell 把 CLIXML progress 解成錯誤。guest 腳本明確讀取嚴格 UTF-8，遇到 NUL
直接失敗；完成解壓後 flush 檔案，再逐一驗證 SHA-256。每輪報告以 flush 後的新檔發布。
同時排空子程序兩條 pipe 並檢查真正 exit code，不能以 PASS 字串或空 ExitCode 推定成功。
原生測試等待 Guest Additions desktop level 3；level 2 只有系統服務，不表示桌面可用。
Guest Control 本身在 Session 0 執行，因此透過本輪唯一的 InteractiveToken 排程工作進入
已登入使用者的桌面，不把密碼寫入排程器。每輪驗證工作退出碼、互動 session 與完整報告，
完成後移除自己的排程工作。VM 須已有可用的登入桌面；工具不啟用自動登入或更改登入政策。
預設啟動 GUI 前端；`--frontend headless` 仍必須滿足相同原生桌面條件。
正常關閉案例先確認獨立 ProjectModule 的藍色面板實際出現在該視窗的兩個像素位置，才送出
`CloseMainWindow`，並核對退出報告有原生幀。視窗 handle 或固定等待時間不算畫面已呈現。

啟動前須有「VM 設定 RAM + 預留 1024 MiB」的可用主機 RAM。工具不會替使用者改 VM 配置
或關閉其他應用程式；資源不足時應先正常關機再調整。timeout／RPC 失敗後保留原始診斷，
不自動重試結果不明的 guest 操作；清理僅嘗試 ACPI，失敗時需人工確認狀態後再開新一輪。
這是既有 VM 的 fixture／傳輸／生命週期證據，仍需下面的實體裝置與完整產品驗收。
兩輪之間即使已回報 poweroff，前端仍可能尚未釋放 RAM；工具最多等待 60 秒實際可用量
達到相同門檻，未達時仍失敗，不會減少預留額度。
清理失敗時總結的 final state 為 unknown，另保留最後觀測狀態；不能沿用上一輪的 poweroff。

### 「歡迎」畫面停滯後的恢復驗證

先核對原 VM 的 UUID 與 snapshot 清單，保留 VBox.log、失敗報告及現有存檔的路徑／SHA-256。
`running` 或 Guest Additions level 2 都不代表已登入桌面。已由使用者重開機恢復時，記錄實際
操作與觀測結果；不要把 recovery clone 的成功，或事件記錄中的單一延遲，當成原 VM 根因已修復。
不刪除 profile／存檔、不還原 snapshot，也不由驗收工具捨棄 saved state 或強制關機。

確認原 VM 已達 desktop level 3 後，使用 `shutdown.exe /s /t 0` 正常關機，實際觀察到
`poweroff` 才開始上面的兩輪驗收。慢速 VM 可明確設定 `--boot-timeout 600`，仍須在期限內
到達桌面並完成所有案例；逾時不是通過。驗收後再比對原有存檔雜湊、snapshot UUID 與目前
snapshot，區分「本輪未再發生」與「已證明永久排除根因」。保留前次失敗證據並新增本輪紀錄。

## Windows 實體驗收清單

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
