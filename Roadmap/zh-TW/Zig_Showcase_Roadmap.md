# Nexora Zig Showcase 與 Engine-owned Entry Point Roadmap

> 版本：v1.0｜狀態：規劃基線｜更新：2026-10-01

> **進度：100%**（截至 2026-10-01；依第 4 節 6 個 milestone 的加權驗收清單計算，
> 已完成項目以 ✅ 標示，結果向下取整至 10%。）

**施工狀態（2026-10-01）：** ZS-M0 至 ZS-M5 已完成。ABI V3 現在定義明確的 result 與 capability 值、
分離 create/start/stop/destroy 階段、可回報失敗的 variable/fixed update，以及 C++ Host 的
transactional state migration；repository 內的 C++ fake module 會驗證此 contract。Zig module
現在會透過成對的 V3 host allocator callback 取得與釋放獨立 state，reload candidate
也不再共用 global state。Fake 與 Zig module 現在共用一份 vector set；allocation tag、negative
descriptor/callback coverage 與 Linux sanitizer preset 已完成 ZS-M0。Runtime dynamic discovery
與 generation lifecycle 已完成；Showcase 會建置並選取 Zig Development shared library，Shipping
則保留相同 ABI 的 static link。

**本機施工狀態（2026-09-23）：** 已提供 deterministic headless/static 與 Development-dynamic
的 ZS-M1 驗證 slice，
輸出 `NexoraShowcase.exe`。C++ 擁有 `main`、Engine lifecycle、小型 `GameWorld`、
fixed/update scheduling、offscreen scene rendering、reload 與 shutdown；Zig consumer 透過
公開 ABI 修改 primary entity 的 Transform，並由 executable 輸出 JSON evidence report。Dynamic
module discovery 已納入 CTest。Native Win32/DX12 presentation 已透過 Window 與 Presentation
contract 實作，但仍須在 Windows target host 驗收。

## 1. 核心決策

Showcase 的 `main`、平台視窗、Engine lifecycle、render loop 與 shutdown 必須由 C++ Host/Engine 啟動；Zig 是被載入的 gameplay module，不是 process owner。Zig 透過穩定 C ABI 呼叫 Engine API，建立內容、處理 tick/input、操作 entity/component 並更新展示狀態。這取代「Zig 只做 ABI smoke」作為對外示範，但保留 smoke test。

```text
OS -> C++ NexoraShowcase main
   -> Engine bootstrap/window/RHI/runtime
   -> load Zig gameplay module
   -> nx_game_query_api / nx_game_create
   -> on_start -> fixed_update/update -> on_stop
   -> Engine render/present and final shutdown
```

## 2. Module contract

Zig dynamic/static module只輸出固定的 C symbols；entry descriptor 帶 ABI version、struct size、capability bits 與 callbacks。Engine 提供 versioned `NxEngineApi` function table 和 opaque context。Engine 擁有 world、assets、window、GPU、thread pool 與 callback invocation；Zig 擁有自己的 gameplay state，並在 `destroy` 前釋放。

規則：

- callback 不拋 exception 穿越 ABI；以 `NxResult` 回報。
- Engine API pointer 僅在 module lifetime 有效，不得 cache borrowed frame data。
- hot reload 須先 quiesce jobs，Zig 用 versioned blob `serialize_state`/`restore_state`；失敗回滾舊 generation。
- render callback 只能提交高階 debug draw/render requests，不取得 native device pointer。
- Shipping 可 static link，但使用相同 ABI contract；Development 優先 dynamic reload。

## 3. Showcase 內容

第一個可玩 slice 是小型 3D gallery：Zig spawn camera、旋轉 cubes、燈光與材質；以 input snapshot 控制 camera；用 raycast 選取物件；顯示 Transform、entity、asset 與 frame diagnostics；切換一個物理/角色區與 large-world streaming 區。所有畫面由 Engine renderer 呈現，Zig 不建立 swapchain。

每個房間同時展示 API：

| 房間 | Zig 呼叫的公開能力 | 可觀察證據 |
| --- | --- | --- |
| Math Lab | Vector/Quaternion/Transform、ray/AABB | gizmo、數值與 property test 狀態 |
| Scene Lab | entity/component、scene、asset | spawn/despawn、reload、generation |
| Gameplay Lab | input、fixed tick、physics/nav | controllable actor、query trace |
| Presentation Lab | animation/audio/VFX facade | counters、fallback/backend badge |
| Streaming Lab | cell request與 residency query | cell/HLOD overlay |

UI 必須標示 `IMPLEMENTED`、`CONTRACT ONLY`、`UNAVAILABLE`，不得以 placeholder 冒充 backend。

## 4. 階段

- **✅ ZS-M0 Contract**：確立 Host-owned lifecycle、function table、錯誤/記憶體/thread contract；C++ fake module 與 Zig smoke 共用 conformance suite。
  - ✅ ABI V3 lifecycle、result/capability、paired allocator 與 transactional state migration。
  - ✅ C++ fake module 的 lifecycle/reload contract 驗證。
  - ✅ C++ fake 與 Zig consumer 共用同一份 conformance vectors。
  - ✅ owner tags、missing symbol/version/struct-size/callback failure，以及 Linux sanitizer gates。
- **✅ ZS-M1 Bootstrap**：`NexoraShowcase` C++ target 負責 CLI、window/headless、module discovery；Zig `on_start/update/on_stop` 可執行。
  - ✅ C++-owned `main`、Engine/World lifetime、fixed/update scheduling 與 ordered shutdown。
  - ✅ deterministic headless validation backend、JSON evidence 與 static Zig object consumer。
  - ✅ Runtime dynamic library discovery、generation ownership、job quiescence 與真正的 library unload/rollback。
  - ✅ 建置 Zig Development shared-library artifact，並由 `NexoraShowcase` 選取。
  - ✅ 透過 Window 與 Presentation boundary 使用 native window/input/swapchain。
- **✅ ZS-M2 API-driven scene**：只用 API Roadmap 的 C/Zig bindings 建 scene、camera、mesh、input 與 diagnostics。
  - ✅ Zig 經 public host table 建立 scene、camera、light 與 cubes；C++ 保留 engine/world/render ownership。
  - ✅ 以 append-only 方式補齊 spawn/despawn、scene、input snapshot、opaque asset handle、raycast、高階 debug draw 與 diagnostics callbacks。
  - ✅ C header、ABI manifest/baseline、Zig binding、ownership/thread/error contract、C++ ABI gates、Zig smoke 與 deterministic headless evidence 已同步。
- **✅ ZS-M3 Feature gallery**：加入 physics、animation/audio/VFX、streaming 的 bounded showcases 與 capability fallback。
  - ✅ Math、Scene、Gameplay、Presentation 與 Streaming room 已有可 script 選擇及 deterministic
    capability／fallback 證據。
  - ✅ Native interaction 提供 WASD camera 移動，公開 Zig scene callback 則執行 selection raycast 與高階 query trace。
  - ✅ 2026-10-02 Linux/Xvfb Rendering Room keyboard gate 已驗證 600 native Vulkan GPU frames 與 `Window::Key` event 驅動的 camera 移動，並修正先前的 ASCII 比對。參見 [Phase B 證據](../../Apps/Showcase/evidence/V1-Phase-B-Linux-Vulkan-2026-10-02/acceptance.md)。
  - ✅ Physics/navigation、animation/audio/VFX 與 cell/HLOD room overlay 依編譯進 Runtime capability 顯示狀態。
  - ✅ 每個 room 皆顯示 `IMPLEMENTED` / `CONTRACT ONLY` / `UNAVAILABLE`，minimal-capability CTest 固定 fallback matrix。
- **✅ ZS-M4 Reload and failure**：transactional hot reload、state migration、錯誤 module/ABI rejection、舊版本 rollback。
  - ✅ 真正 dynamic generations 的 job drain、restore failure、old-generation rollback、shutdown-during-reload 與 repeated reload stress。
  - ✅ Dynamic library replacement 會等待檔案大小與寫入時間穩定後才載入；host 記錄帶 generation 的 update/fixed-update failure，Presentation 並將 device lost 明確分類為 device recreation，而非 surface retry。
- **✅ ZS-M5 Distribution**：Development dynamic 與 Shipping static/packaged profiles，產生 license/build/API manifest。
  - ✅ 可重現的 Development-dynamic 與 Shipping-static package target 會產生 license、
    build/API/content manifest、逐 artifact SHA-256 digest 與 `SHA256SUMS`。
  - ✅ 獨立配置的 Windows 10 target 已在 package root 驗證全部 16 個 `SHA256SUMS` 項目，
    執行產物記錄的 command 並取得 exit code 0 與 `launch-report.json` 的 `PASS`；
    證據保留於 [acceptance record](../../Apps/Showcase/evidence/ZS-M5-Windows-CleanMachine-2026-10-01/acceptance.md)。
  - ✅ Linux clean-package evidence 會驗證全部 checksum、建立全新的隔離副本，從該副本啟動
    relocatable dynamic package，並保留 report／exit status。其他 target host 的驗收仍待完成。
  - ✅ Windows 開發工作站上，Development-dynamic 與 Shipping-monolithic/static 套件的隔離副本
    分別通過 13 與 5 個 checksum、依記錄命令啟動並產出 `PASS` report。這是 Windows 開發工作站
    證據，與上方記錄的獨立配置乾淨機器驗收分開；CI workflow 已配置兩種 package evidence target。

## 5. 測試與驗收

- Engine 先於 Zig module 建立，且 module 永遠先於 Engine service 銷毀。
- C++ fake consumer 和 Zig consumer 跑同一份 ABI conformance vectors。
- headless scripted tour 產生 deterministic JSON；window smoke 驗證真實 present，但 screenshot 只做視覺 regression，不代替 correctness assertion。
- 測試 module 缺 symbol、版本不符、callback failure、reload 中有 job、restore failure、device lost 與 shutdown。
- Address/Undefined/Thread sanitizer 覆蓋 host；Zig 使用安全檢查 build；跨邊界 allocation 有 owner tags。
- Windows/DX12 為第一個 graphical gate；Linux headless 保持 CI gate；Vulkan/Metal 在對應 host 實測後才升級狀態。

## 6. 完成定義與非目標

完成代表 clone/build 後由 `NexoraShowcase` 啟動，明確證明 C++ 擁有 `main`，Zig gameplay 經公開 API 驅動畫面，且 headless、錯誤、reload 與 Shipping profile 有證據。第一版不允許 Zig 直接呼叫 RHI/native window，不要求 Zig 編寫 editor UI，也不把缺少平台 SDK 的功能標成完成。

本文件與既有 [`V1-Visual-Showcase-Long-Term-Plan.md`](V1-Visual-Showcase-Long-Term-Plan.md) 互補：該文件描述整體展示產品，本文件專門約束 Zig consumer 與 Engine-owned entry point。

## V1 Visual Showcase 後續整合（2026-10-03）

✅ Linux/Vulkan application 已在 Xvfb/lavapipe 呈現八個 live Runtime room、可讀原生 GPU UI、held keyboard／pointer control 與 210 秒導覽。Full package tooling 包含 content、可重現 ZIP／SHA-256 與 Linux shared-library isolated resolution。此為 V1 Visual Showcase 整合證據，不取代本 roadmap 既有平台 gate，也不代表新版 Windows clean-machine 圖形驗收。

✅ Windows DX12 本地 Development/Full 與 Shipping/Full 原生執行已記錄於
[Windows-V1-DX12-Local-2026-10-03](../../Apps/Showcase/evidence/Windows-V1-DX12-Local-2026-10-03/acceptance.md)。
GTX 960 開發主機驗收涵蓋八個房間、primitive/texture/instance rendering、
原生 offscreen graph/UI/present、輸入／編輯／reload／animation 截圖、Lab asset/plugin 拒絕
以及完整 210 秒 guided tour。明確的 Development preset 與 stock PowerShell 5 預設路徑修正
讓本地文件流程可重現。乾淨主機與實體顯示操作聲明仍待驗收；V1 最終驗收尚未完成。
✅ Windows 本地驗收腳本現要求當次匯出、已修改場景的成功 snapshot round trip、
導覽重播歸零與 M5／M6 指定拒絕案例的實際輸入／輸出。Source／package hash 採固定 LF 位元組。
基底 e4a140139189 通過 Linux Development 77/77 與 Shipping package CI；
實體顯示與乾淨主機最終 gate 仍待驗收。


## 可選 Zig 2D 引擎介面（2026-10-14）

✅ 原 V3 前綴新增可選、大小／能力／版本檢查的 2D 擴充。複製的 Sprite／Collider／
相機、動畫事件緩衝、查詢快照、Tilemap 與世代池化批次沿用既有 TwoD 服務。實際 Zig
靜態／動態／Reload 及布局契約通過；Linux 原生輸入驗收呈現 600 幀並確認 Zig 移動。
見 [Zig 使用範例](../../Gameplay/Zig/README.md)與[2D Roadmap](2D_Roadmap.md)。此引擎
控制擴充不改變本 Roadmap 原進度，也不代表實體 Windows gate 已完成。

## 2D GPU Instancing 後續交付（2026-10-15）

✅ 沿用 Zig V1 Instance 批次 API，透過 `SpriteFrame::Draw` 接入 CPU 驅動的原生 GPU
Instancing。Linux 像素驗證涵蓋單次 256 Instances 與跨 Atlas／HUD 透明排序；實際 Zig
輸入／Reload 驗證呈現 600 幀並記錄原生 Instance 計數。詳見
[2D 範圍與平台限制](2D_Roadmap.md) 及 [使用方式](../../Gameplay/Zig/README.md)。
不新增 ABI 或通用 Mesh／Material／Compute／Indirect API，也不代表 Windows 實體驗收。

## SP-01 獨立 Player 顯示整合（2026-10-23）

✅ 可攜實作與回歸檢查：252 項通過、16 項原生前提不足而跳過、0 失敗；Monolithic Shipping consumer 通過。

獨立 2D Player 已加入可選專案視窗設定、尾端擴充且獨立協商的 C／Zig 顯示 table、
每幀一致的像素／logical／DPI 快照、原生視窗模式請求、遊戲指定畫框比例、安全畫框
投影／裁切，以及正確 DPI 的 UI／輸入。舊 services consumer 與專案描述檔保留原有
前綴／預設值。世界鏡頭跨度、HUD 排版與 UI 縮放仍由遊戲決定；本次未接入 Editor Play。

參見[顯示契約](../../Engine/TwoD/GameplayDisplay.md)、
[Linux 證據](../../Tools/Build/evidence/SP01-GameplayDisplay-Linux-2026-10-23.md)與
[Windows 操作程序](../../Tools/TwoD/WindowsDisplayAcceptance.md)。
Windows 實機 DPI／點擊／模式驗收維持 NOT RUN／NOT ACCEPTED；2D M6／M7 與既有里程碑
完成數不變，可攜實作檢查不等同於通過上述驗收。

✅ X11 原生關閉回歸：Xvfb 驗證事件處理前已銷毀視窗的查詢與清理；修正前 DSO 在同一探針失敗。

## SP-02 圖像 UI 與 Zig 組合（2026-10-23）

✅ 已實作圖像／9-slice UI、獨立互動判定、字形 metrics、可回收字型 handle 及快照生命週期
貼圖回收，透過獨立協商的 C／Zig table 提供。原版與 SP-01 services 前綴維持相容。Zig
診斷畫面涵蓋 HUD 槽位、選卡、整備、換行字幕與設定，宿主不承載遊戲規則。

可攜式 NXFONT1 工具嚴格盤點輸入全文字元、來源／字型 hash 與圖集預算。企劃文件覆蓋
不等於最終台詞驗收，診斷圖示不算正式美術。見 [UI 契約](../../Engine/TwoD/GameplayUiDraw.md)
與[雲端證據](../../Tools/Build/evidence/SP02-ImageUi-Cloud-2026-10-23.md)。正式美術、Windows
字型／輸入、實體手把及既有里程碑驗收仍保持未完成。

## SP-03 延後素材載入與常駐生命週期（2026-10-23）

✅ 已實作有界目錄 IO、C／Zig 複製請求狀態、明確資產移除、依相依順序卸載及快照感知的原生貼圖回收。
原版／SP-01／SP-02 services 前綴與舊專案預載預設維持相容。容量可明確設定，不是無條件提高 64 MiB；
轉場、遊戲規則與事件政策仍由 Zig 控制。

可攜驗證涵蓋五張同時常駐的 2048² 圖集（超過 80 MiB）、八輪卸載／重載、舊版本預算背壓及實際程序
RSS／working-set 觀測。獨立 Zig 模組從超過 80 MiB 的封裝啟動，驗證 15 次轉場、1,800 個 pooled instances、
30 個 Animator 事件及毀損檔案拒絕。原生像素測試檢查 2048² GPU 新版本與保留畫面的回收；平台實際執行
證據須與僅編譯區分。見[契約](../../Engine/TwoD/GameplayAssets.md)及
[雲端證據](../../Tools/Build/evidence/SP03-AssetResidency-Cloud-2026-10-23.md)。

正式遊戲素材／全內容、完整拾取／戰鬥轉場及目標實機 RAM／VRAM／幀時間預算尚未驗收。
音訊串流／壓縮、色彩及既有 M6／M7 門檻保持原判定。

## SP-04 明確美術色彩與 2D fallback（2026-10-23）

✅ PNG 可明確選擇 sRGB 匯入，RGB 只解碼一次為既有 linear RGBA8 圖集；保留 straight alpha、
歷史預設、Atlas 格式及 services ABI。衝突 profile／ICC 在發布前拒絕，Zig 原始圖像／tint 工具
共用同一色彩契約。
✅ 原生 2D Player 明確要求 Vulkan／DX12／Metal 硬體 sRGB UI／Sprite 輸出；保留既有混合
3D 表面預設，原生像素測試只接受正確編碼灰階，不將 UNORM 當成通過。
✅ 獨立匯出的 Zig 診斷包含正俯視分層、Y 穿越、前景淡出、烘焙 glow／shadow 關閉及始終保留
的危險外框。這些是工程素材與一般 alpha 圖層，不是動態局部光、遮擋柔影或選擇性 bloom。

見[色彩／fallback 契約](../../Engine/TwoD/ColorAndFallback.md)及
[雲端證據](../../Tools/Build/evidence/SP04-ColorFallback-Cloud-2026-10-23.md)。
正式第一關美術、暗部漸層量化、透明過濾／白邊、可讀性、Windows 目標實機輸出與效能仍待驗收
（`nexora-ofe.1`）。原生測試須實際執行後才能宣稱像素通過，本機編譯不算。既有 M6／M7 gate
與其他 SP 缺口維持原判定。

## SP-05 有界本機語音播放（2026-10-23）

✅ 解碼 PCM 已有 active／live／數量預算，移除或替換後仍被聲音持有的樣本也正確計量。
可選 AudioState 服務提供用量快照與獨立 Voice bus；保留 Audio V1 及原版／SP-01／02／03
services 前綴。
✅ caller-owned Zig 對話控制器沿用 deferred 本機 blob，註冊 WAV 後播放／暫停／跳過／重播，
提供獨立語音音量／靜音與字幕 fallback。Player 的 audio capacity 與素材預算分開，完整語音包
不必啟動預載。
✅ 標準函式庫盤點工具可計算已宣告 WAV 的檔案量／時長、字幕 ID 缺漏與併發 PCM 集合；
metadata／工程語音報告不算正式配音全集完成。

可攜驗證涵蓋真實 PCM 樣本、故障／重新配置、取消與舊前綴 canary；獨立 Zig 模組／Player
測試超過 96 MiB 離線安裝包、十二句各 87 秒工程語音、16 MiB peak PCM 及最終常駐歸零。
捕捉／null sink 與 RSS 採樣是診斷證據，不等於喇叭可聽或正式 RAM／幀時間驗收。見
[語音契約](../../Engine/Runtime/GameplayVoice.md)及
[雲端證據](../../Tools/Build/evidence/SP05-OfflineVoice-Cloud-2026-10-23.md)。

正式配音／字幕全集、完整離線安裝、最壞併發及 Windows 實機播放／拔除／恢復／長測仍由
`nexora-6d6.1` 追蹤。按句 PCM 路徑沒有實作壓縮解碼、音訊磁碟串流或 realtime mixing。
其他 SP 評估與 M6／M7 驗收維持原狀。
