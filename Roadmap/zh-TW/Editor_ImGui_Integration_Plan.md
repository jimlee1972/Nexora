# Editor ED-M0 Dear ImGui 整合計畫

> 版本：v1.2｜狀態：portable shell 修正已落地，renderer 與 target-host 驗收待完成｜更新：2026-09-24｜對應：
> `Editor_Roadmap.md`（ED-M0）、`ADR-0001-Editor-UI-Framework.md`

## 1. 目的

[ADR-0001](ADR-0001-Editor-UI-Framework.md) 選定了 Dear ImGui，把 ED-M0「UI-framework ADR」這一項
定案。這份文件規劃 ADR 明確沒有關掉的 ED-M0 剩餘範圍：圖形化 docking、theme、DPI、IME 接線、
無障礙方向、crash-recovery UX。Feature-gated portable host、RHI submission contract、docking
shell、input／DPI／IME bridge、可互動 Hierarchy、recovery UX 與 accessibility 方向現已實作。目前
RHI submission 仍是 validation scaffold，還不是完整的 textured／indexed ImGui renderer；native
visual evidence 也仍是驗收 gate，因此本次交付不會把 ED-M0 標記為已驗收。

**這份計畫的第一步會引入一個新的第三方依賴（vendor Dear ImGui）並動到 build 系統（新的 CMake
module、新的 module-graph 條目、新的 feature option）。** 按照這個 repo 一貫的規則（「遇到需要裝
新相依、改 CI、或動 build 系統的情況，先講清楚再做」），需要在動手寫程式碼之前明確確認的是*這一
步*，不是整份計畫。寫這份計畫文件本身不算踩線。

## 2. 現況基線（對照原始碼確認過）

- `Apps/Editor/NexoraEditor`（`Apps/Editor/main.cpp`）目前是一支 headless CLI：開專案、index
  content、寫出 JSON report。沒有視窗、沒有渲染，只依賴 `Nexora::Editor`（見
  `Apps/Editor/CMakeLists.txt`）。
- `NexoraEditorCore`（`Engine/Editor/`）依設計就是 UI-toolkit agnostic（`Engine/Editor/README.md`）：
  workspace、asset indexing、`SceneDocument`（create/select/reparent/undo）、specialized-tool
  capability registry、build-manifest frontend 都已存在且是 portable-tested，但 README 講得很清楚：
  「Docking、DPI/IME/accessibility、viewport rendering、gizmo、native-host 視覺驗證仍是 UI-host
  的責任」——也就是這份計畫要做的事，不是已經做完的事。
- `Nexora::Window` 跟 `Nexora::Presentation` 已經提供了平台視窗、input 轉譯、以及
  `Window_Presentation_Roadmap.md` WP-M3 點名 Editor Scene/Game view 應該重用的 RHI-backed
  `RenderSurface`（「Editor Scene/Game view 可用，不用讓 Runtime 額外依賴 Editor」）。這份計畫的
  ImGui backend 就是透過那個既有的 surface 渲染，不會另外造一個。
- `Config/Modules/modules.json` 已經有這份計畫需要的 feature-gated-module pattern：
  `Window`/`Presentation` 被 `NEXORA_ENABLE_WINDOW_PRESENTATION` 擋著，`Editor`/`EditorApp` 被
  `NEXORA_ENABLE_EDITOR` 擋著。新的 `EditorImGui` module 照同樣的形狀接進去就好。
- 這次 session 前面那輪 bug 修正剛好動到這份計畫會直接依賴的程式碼：
  `Engine/Window/src/X11Window.cpp` 的滾輪軸向修正、`Win32Window.cpp` 的 IME null/負值檢查跟
  UTF-8 標題轉碼，正好就是下面 Phase 2 要轉送進 `ImGuiIO` 的那些 input/IME 管線。

## 3. 範圍與不做的事

範圍內：vendor Dear ImGui、針對每個已支援的 RHI/Window 平台組合各寫一份 renderer/input backend、
docking、DPI、IME 轉送、透過既有 `NexoraEditorCore` registry 接一個真正的 panel、以及建立在既有
recovery-journal 資料層（`ProjectWorkspace::RecoverWorkspace`，`Engine/Editor/README.md` 裡已標記
完成）上的 crash-recovery UX。

範圍外：ED-M1 到 ED-M7 剩下的圖形化工作（Content Browser、Scene View gizmo、PIE Game View、
specialized-tool panel、build/profile frontend、production hardening）——這份計畫只把 ED-M0 本身
推到圖形化驗收。完整的無障礙實作也不在範圍內；依照 ADR-0001，這份計畫的 Phase 5 只接 ImGui 本來
就暴露出來的管線（IME 定位），不包含 ED-M7 之後要另外展開的第二層 accessibility tree。

## 4. 動手寫程式碼前需要先確認的依賴決定

- **什麼**：vendor Dear ImGui（docking 功能/branch）的原始碼，最可能透過 CMake `FetchContent`
  釘住特定 tag（不是追蹤一個持續變動的 branch），跟這個 repo 現在拉 Vulkan headers 的方式一致
  （這次 session 的 build 輸出裡看到的 `_deps/nexora_vulkan_headers-src`），而不是 git submodule。
- **授權**：MIT，跟本 repo 自己的 `LICENSE` 一致——沒有摩擦（寫 ADR 的時候已經確認過）。
- **新的 CMake module**：`Engine/EditorImGui`（名稱還可以再討論），擋在一個新的 feature option
  後面，例如 `NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL`（預設 OFF，等 Phase 1 落地後再重新考慮），在
  `Config/Modules/modules.json` 裡依賴 `Editor`、`Presentation`、`Window`、`RHI`。`EditorApp`/
  `Apps/Editor` 只在這個 option 開啟時才多這個連結依賴，option 關掉時現有的 headless CLI 路徑照樣
  能動。
- **對 CI 的影響**：多一條 Linux CI leg（option 開啟）建置，之後再視需要加 Windows/macOS leg；
  在真的有 native rendering 之前，先用一個 offscreen/headless 的 ImGui smoke test（見 Phase 1）
  讓它可以在現有的 Linux gate 上測試，不需要真的有 display。

如果這個形狀不是你要的（不同的 vendoring 機制、不同的 module 名稱/位置、不同的預設值），請在
Phase 1 開始之前先講——後面所有內容都是建立在這個假設上的。

## 5. 分階段計畫

### Phase 1 — Vendor ImGui + 最小 offscreen smoke test（這裡可以驗證，Linux）

- 加上 `FetchContent` 宣告、新的 `EditorImGui` module 骨架、module-graph 條目、feature option，
  全部預設 OFF，不影響現有 build。
- 先寫 Vulkan renderer backend（這個雲端 session 唯一真的能跑的 native RHI backend）：把 ImGui
  的 draw data 轉譯成 `Nexora::RHI` 的 texture/pipeline/command-list 呼叫，渲染目標用 offscreen
  render target，做法跟現有 renderer contract test 用的 `ExecuteTriangleFrame` 一樣——不需要真的
  視窗或 display。
- Gate：新增一個 contract test，把一個 ImGui frame（哪怕只是 `ImGui::ShowDemoWindow`）渲染到
  offscreen 的 `Nexora::RHI` validation-device 跟 Vulkan-device texture 上，斷言沒有 validation
  error、draw call 數不為零，比照 `window_presentation.contracts` 現在對 fake/offscreen surface
  生命週期測試的做法。

### Phase 2 — 真正的視窗 + input 轉譯

- 把 Vulkan backend 接到真正的 `Nexora::Presentation::RenderSurface`（依照 WP-M3，就是 Showcase
  已經在用的那個可重用 surface owner），而不是 offscreen texture。
- 把 `Nexora::Window` 的事件轉譯進 `ImGuiIO`：pointer 位置、滑鼠按鍵、（X11 修正已經落地的）正確
  軸向的滾輪量、鍵盤、以及送進 `ImGuiIO::AddInputCharacter` 的文字/組字事件。
- Gate：只能靠 target-host 證據（真正的視窗、真正的 input）——這跟
  `Window_Presentation_Roadmap.md` 裡 WP-M1 要求 Windows runner 是同一個等級；Linux/X11 這裡可以
  開發、可以做部分 smoke test，但完整的互動驗證需要真的有 display，這個雲端 session 沒有。

### Phase 3 — Docking + 第一個真正的 panel

- 開啟 ImGui 的 docking branch 功能集；定義初始 dock layout 跟穩定的 panel ID，重用
  `ProductShell::Panels()`/`IsStablePanelId`（`Engine/Editor/README.md`），不要另外發明一套平行的
  panel-identity 機制。
- 把一個真正的 panel 完整接到 live 的 `NexoraEditorCore` 狀態上——Console 或一個最小的 Hierarchy
  view 是最小、最正確的選擇，因為兩者都已經有一個 portable 的資料來源（`SceneDocument` 的 node
  list），不需要額外的 Editor Core 新工作。
- Gate：panel 要反映 live 的 `SceneDocument` 狀態，且能把一個使用者動作（例如選取一個 node）
  round-trip 回 `SceneDocument::Select`。

### Phase 4 — Theme / DPI

- Theme：ED-M0 驗收只需要一套 first-class theme 就夠，milestone 的 gate 沒有要求 per-user 主題。
- DPI：把 `Engine/Window` 的 Win32 backend 本來就會算的 DPI-aware sizing（這次 session review
  Win32Window.cpp 時看到的 `AdjustWindowRectExForDpi`）轉送進 ImGui 的 font atlas scale 跟 style
  scale。

### Phase 5 — IME 接線

- 把 `Engine/Window` 的組字完成文字事件（就是這次 session 修的 Win32 IME null/負值那段管線）轉送
  進 `ImGuiIO::AddInputCharacter`，並實作 `io.SetPlatformImeDataFn` 把原生 IME 候選字視窗定位在
  ImGui 的輸入游標上。
- Gate：Windows 上（僅限 target-host）一個文字欄位能接受 IME 組字輸入，且不會讓 Linux 上既有的
  `Engine/Window` IME contract 退步（X11Window 已經處理的部分之外，Linux 沒有額外的 IME 概念要
  接）。

### Phase 6 — Crash-recovery UX

- `ProjectWorkspace::RecoverWorkspace` 跟 recovery journal 在資料層已經存在
  （`Engine/Editor/README.md`）。這個階段只加 UI：啟動時如果偵測到 recovery journal，在正常
  shell 渲染前先跳出一個「復原或捨棄」的對話框。
- Gate：session 中途把 process 砍掉再重開會提供復原選項；選擇捨棄會照既有資料層 contract 的保證
  把 journal 丟掉。

### Phase 7 — 無障礙方向（只做管線規劃，不做實作）

- 依照 ADR-0001，這個階段是展開範圍，不是交付：確認 Phase 3 已經在用的 panel/command registry
  （`ProductShell::Panels()`、穩定的 panel/command ID）是否足以驅動未來的第二層 accessibility
  tree，把找到的缺口記錄下來留給 ED-M7 處理。這裡不會蓋出任何 accessibility tree。

## 6. 驗證與完成定義

- Phase 1 跟 Phase 3（offscreen render、docking/panel 邏輯）可以在這個 repo 現有的 Linux gate 上
  驗證。
- Phase 2、Phase 4（DPI）、Phase 5（IME）、Phase 6（crash UX）都需要至少一個平台的真實視窗
  target-host 證據；Linux/X11 能拿到有 display 的 Linux host 能提供的那部分覆蓋，但 Windows/macOS
  的驗收明確不在這個雲端 session 能力範圍內，符合這個 repo 一貫「不虛報沒跑過的平台覆蓋」的規則。
- ED-M0 整個 milestone 不會因為這份計畫就被標記為驗收完成，要等
  `Editor_Roadmap.md` 的 ED-M0 gate 裡每一項（不只是這份計畫的各 Phase）都有通過的證據——這份計畫
  本身不授權更新那個 milestone 的狀態。

### 實作證據

- `NexoraEditorImGui` 擁有 context、stable-ID dockspace、Hierarchy 在左／Console 在下的初始
  layout、theme／DPI policy、pointer、button、wheel、key、text、focus ingestion，以及目前的
  public-RHI validation submission scaffold。
- `NexoraEditor --graphical` 建立 public `RenderSurface`、消費其 borrowed events，並驅動 UI 與
  recover／discard lifecycle。
- Hierarchy 顯示 live `SceneDocument::Nodes()`，並透過 `SceneDocument::Select` 回寫 selection。
  Win32 擁有 candidate-window positioning；不支援的 host 會明確回報。ED-M7 accessibility handoff
  記錄於 `Engine/EditorImGui/README.md`。
- Graphical application 現在會傳入 live scene 與 surface 擁有的 extent／DPI snapshot；resize 時會
  重建 offscreen validation target。public RHI 與 validation submission 現在會保留 ImGui
  vertex／index buffer、indexed offset、clip rectangle 與 font texture binding。在 Phase 1 或
  Phase 2 通過前，仍須完成 native backend 實作並連接 acquired presentation backbuffer。

## 7. 風險

- Docking branch 的維護：ImGui 的 docking 功能過去長期活在跟 mainline release 分開的 branch 上；
  照 §4 釘住特定 tag、每次升級再重新評估，會比追一個持續變動的 branch 便宜。
- Vulkan 上 font atlas + descriptor-set 的生命週期，是視窗 resize 時很常見的 validation error 來
  源；Phase 1 先做 offscreen 的做法，就是要把這個問題跟視窗 resize 的互動隔開，留到 Phase 2 再處理。
- Scope creep 風險：一旦有了真正的視窗，很容易忍不住想做超出 ED-M0 範圍的東西（例如提早開始做
  ED-M1 的 Content Browser）。這份計畫的各 Phase gate 就是為了把工作範圍鎖在 ED-M0 自己的驗收
  標準內。
