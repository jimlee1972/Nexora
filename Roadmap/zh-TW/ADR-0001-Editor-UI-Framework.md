# ADR-0001：Editor UI Framework

> 狀態：Accepted | 日期：2026-09-24 | 對應：`Editor_Roadmap.md`（ED-M0）

## 背景

`Editor_Roadmap.md` 的 ED-M0 milestone 要求 UI framework 必須「透過 ADR 跟聚焦原型選定」，Editor
shell 在這個決定被正式記錄之前不能開始 production widget 的實作。在此之前，這個選擇只以非正式的設計
筆記形式存在於 `跨平台3D_Engine_V1_完整規劃書_v1_2.md`（「Editor UI：Dear ImGui Docking /
Multi-Viewport」），從未寫成一份帶替代方案比較與後果分析的正式決策記錄。這份 ADR 補上的就是這一塊缺口，
不代表 ED-M0 其餘部分已經完成：不管這份 ADR 選定哪個 framework，圖形化 docking、theme、DPI、IME、
無障礙、crash recovery UX 都還是待完成的實作工作。

repo 裡其他地方已經確立的兩個架構限制，框住了這次的選擇範圍：

- `Engine/Editor/README.md`：`NexoraEditorCore` 是 UI-independent 的，只組合公開的 Runtime Editor
  SDK API，不能直接碰 renderer 或 platform 內部細節。
- `Editor_Roadmap.md` §3：「shell 依賴公開的 window/swapchain 路徑，不能為了顯示 UI 另外發明一條
  private presentation path」——Editor 的 Scene/Game view 是 `Nexora::Presentation` 擁有的
  `RenderSurface`（見 `Window_Presentation_Roadmap.md` WP-M3），不是 UI framework 自帶的另一套渲染
  系統。

換句話說，不管選哪個 framework，都必須透過 Nexora 自己的 RHI 跟 Window 模組渲染，不能帶一套會跟現有
架構打架的視窗/渲染系統進來，否則 Scene/Game viewport 之後要嘛被硬塞進去、要嘛整套重工。

## 決定

採用 **Dear ImGui**，使用其 docking / multi-viewport 功能集，作為 Editor 的 UI framework。

由一層專案自己維護的 backend（`engine::editor::imgui` / `engine::ui::imgui`，沿用 V1 完整規劃書已
經畫出的命名空間）負責把 ImGui 的 draw data 透過 `Nexora::RHI` 渲染出來，並把 `Nexora::Window` 的
input 事件轉譯進 `ImGuiIO`——做法上比照 ImGui 官方 Vulkan/DX12/Win32/Cocoa backend 的模式，只是改成
對接 Nexora 既有的抽象層，而不是直接呼叫原生 API。不會引入第二套視窗系統或 swapchain 路徑。
`NexoraEditorCore` 的 panel/command/selection/document 模型維持與 UI toolkit 無關（呼應 V1 完整規劃
書自己寫的「Editor Architecture 不綁死 ImGui widget tree」）：panel 透過穩定 ID 註冊，內容由
`EditorObjectAdapter`／`InspectorRegistry` 這類查找機制驅動，未來真要換 framework 也只會換掉渲染/
widget 那一層，不會動到這份 ADR 沒有碰的資料模型。

## 考慮過的替代方案

- **Qt（Widgets 或 Quick）**：docking、theme、DPI、IME、無障礙支援開箱即用都很強。放棄的原因是它會
  帶自己整套視窗/渲染系統，Scene/Game viewport 之後要嘛被硬塞進去、要嘛得繞過它，等於重做
  `Nexora::Window`/`Nexora::RHI` 已經做過的事；而且它的 GPL／商業雙授權，對一個 MIT 授權的引擎來說
  也是比較大的包袱，不如一個授權寬鬆、不綁渲染系統的函式庫。
- **wxWidgets / GTK**：跟 Qt 一樣的根本問題（自帶一套視窗/渲染系統，會跟 `Nexora::Window`/
  `Nexora::RHI` 打架），卻沒有 Qt 那麼強的 docking／無障礙支援可以抵銷這個缺點。
- **自建 retained-mode UI toolkit**：這其實已經是*runtime*、面向遊戲玩家 UI 的既定選擇（V1 完整規劃
  書：「Custom Retained Mode Runtime UI；Dear ImGui 僅 Editor / Debug」）。如果還要為 Editor 等級的
  需求（docking、property grid、支援 undo 的 widget、無障礙）再從零蓋一套，工作量只會比直接採用一個
  本來就是為這個角色設計的函式庫更大，卻沒有對應的好處。
- **Web-based shell（Chromium/CEF + JS framework）**：透過瀏覽器引擎可以拿到很強的 docking/theme/
  無障礙支援，但會拉進 Chromium 等級的依賴跟跨行程邊界的複雜度，對一個精簡、授權寬鬆的開源引擎來說
  不成比例，而且要在 web view 裡顯示 `Nexora::RHI` 的 viewport 輸出，還是得另外搭一座橋。

Dear ImGui 在同類自研 RHI／in-house 引擎裡，本來就是這個角色的事實標準，整合模式（immediate-mode UI
backend 對接自家 RHI 抽象層，而不是 ImGui 官方那些原生圖形 API backend）已經是很成熟的做法。

## 後果

- **Docking / multi-viewport**：ImGui 的 docking branch 功能集就能滿足，不需要在 ImGui 之外再多引入
  第三方依賴。
- **IME**：ImGui 提供 `io.SetPlatformImeDataFn` / `ImGuiPlatformImeData` 給平台 IME 定位用，
  `Engine/Window` 的 Win32/X11 backend 已經會送出組字完成的文字事件（`WindowEventType::Text`，
  Win32 上透過 `WM_IME_COMPOSITION` 接進來），ImGui backend 層只要把這些事件轉送進
  `ImGuiIO::AddInputCharacter` 就好。`Engine/Window` 不需要為此再新增 IME 管線。
- **無障礙（screen reader）**：這是這個決定刻意接受的唯一已知缺口。Dear ImGui 沒有內建的
  accessibility tree，換成 Qt 之類的原生 toolkit 就不會有這個問題。ED-M7 明確要求交付前要做過
  「keyboard and screen-reader audit」，所以這份 ADR 並沒有關掉那個項目——只是先定出方向（很可能是
  從 ImGui panel 本來就會註冊的那份穩定 panel/command registry，另外建一份平行的 accessibility
  tree，類似 AccessKit 這類專案讓 immediate-mode UI 搭配一座平台無障礙橋接層的做法），實際規劃仍要
  留給 ED-M7 去展開。只憑這份 ADR 就把 ED-M7 的無障礙項目標記完成，會虛報進度，明確不在這份 ADR
  範圍內。
- **授權**：Dear ImGui 是 MIT 授權，跟本 repo 自己的 MIT 授權一致，沒有雙授權或 copyleft 的摩擦。
- **每個 backend 都要寫整合層**：`Nexora::RHI` 的每個 backend（Vulkan、DX12、Metal）跟
  `Nexora::Window` 的每個平台 backend（Win32、X11、Cocoa）都要各寫一份 ImGui renderer/input
  backend——這份 ADR 授權了這件事，但沒有在這裡直接交付。

## ED-M0 現況

這份 ADR 滿足了 ED-M0 裡「UI-framework ADR」這一項。ED-M0 剩下的範圍——圖形化 docking、theme、DPI、
IME 接線、上面提到的無障礙方向展開、crash recovery UX——維持 open，照舊追蹤在 `Editor_Roadmap.md`。
