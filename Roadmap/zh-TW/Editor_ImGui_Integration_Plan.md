# Editor ED-M0 Dear ImGui 整合計畫

> 版本：v1.3｜狀態：施工中；target-host 證據待完成｜
> 更新：2026-09-24｜對應：`Editor_Roadmap.md`（ED-M0）、
> `ADR-0001-Editor-UI-Framework.md`、`Window_Presentation_Roadmap.md`

> **Repository 稽核（2026-09-24）：**施工狀態為**進行中**。下方打勾的 foundation 已存在於
> source 與 contract test，但 **WP0～WP8 尚無任何一包通過 exit gate**。Retained GPU resource、
> 直接渲染至 borrowed presentation target、project-owned layout persistence、DPI font-atlas rebuild
> 與 recovery failure contract 已實作。自動化 X11 kill/relaunch recovery 已有覆蓋；physical-display
> 與 Windows target-host evidence 仍待完成，因此 foundation 打勾不得解讀成 ED-M0 已驗收。

## 1. 目標、驗收邊界與目前事實

ADR-0001 已選定支援 docking 的 Dear ImGui。本文件是 AI agent 完成 ED-M0 時必須遵守的施工規格，
不得另造第二套 window、presentation 或 editor data model。只有 Editor 經 public `RenderSurface` 開啟、
顯示可用的 GPU-backed docked shell、吃到真實 input、正確處理 DPI 與 Windows IME、在正常編輯前提供
recovery，且具備可重現的自動化與 target-host 證據，ED-M0 才算驗收。

Repo 已有 feature-gated `NexoraEditorImGui`、釘版 Dear ImGui docking dependency、context ownership、
input translation、stable-ID docking、live Hierarchy、recovery modal、DPI/theme policy、generation-checked
texture、completion-tracked resource ring，以及經 backend-neutral presentation contract 的 Vulkan／DX12／
Metal native draw recording。這些仍只是 foundation，因 real-display Linux 證據與 Windows DPI／IME
證據仍缺少，因此 ED-M0 保持未完成。

### 已確認實作 checklist

- ✅ Graphical shell 是 optional 且隔離於 `NexoraEditorImGui`；Editor Core 不相依 Dear ImGui。
- ✅ Dear ImGui 固定為 `v1.91.9b-docking`、已啟用 docking，並停用 unmanaged `imgui.ini`
  persistence。
- ✅ `NexoraEditor --graphical` 建立一個 public `RenderSurface`，並消費其 `WindowEvent` stream
  與即時 `FrameInfo` extent／DPI state。
- ✅ Host 擁有一個 `ImGuiContext`、呈現 stable-ID Hierarchy／Console panel、建立 initial dock
  layout，且 Hierarchy selection 會經 `SceneDocument` round-trip。
- ✅ Portable RHI draw-contract overload 會上傳 vertex/index、套用 scaled scissor、保留 index/
  vertex offset，並由 validation device 測試。
- ✅ Key/modifier、pointer、wheel、focus、Unicode text、DPI 與 IME candidate callback 已有施工
  foundation。
- ✅ Recovery UI 只呼叫 `ProjectWorkspace` recover/discard operation、保留 failure，並提供
  exactly-once result consumption。
- ✅ Production surface overload 會送出 backend-neutral textured/indexed `UiDrawData`；Vulkan、
  DX12 與 Metal implementation 直接記錄 native GPU draw，不再呼叫 `CompositeRgba8`。
- ✅ Production surface 與 validation path 已實作 pipeline、sampler、generation-checked texture、
  bounded upload ring 與 completion-protected retirement。
- ✅ Project-owned layout persistence、DPI font-atlas rebuild，以及 recovery failure／exactly-once
  contract coverage 已存在。
- [ ] Physical-display Linux graphical validation 與 Windows DPI／IME target-host acceptance
  evidence 均已有記錄且通過。Automated X11 rendering 與 kill/relaunch recovery 已納入 feature-on
  Linux gate。

### 「完成」的定義

同一個 commit 必須同時滿足：

1. `NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF` 保留 headless Editor，且不 fetch/link Dear ImGui。
2. Feature ON 時，`NexoraEditor --graphical --project=<path>` 只使用一個 `RenderSurface`；Editor Core
   看不到 native graphics API 或 OS window handle。
3. Native path 把 vertex/index data、projection constants、scissor rectangles、font/user textures、alpha
   blending 與 resource transitions submit 到 acquired backbuffer。Production 不再使用 CPU compositor
   （如有充分理由，可只留下明確命名的 test oracle）。
4. Resize、minimize、out-of-date/suboptimal surface status、DPI change、focus loss、shutdown 都遵守
   `RenderSurface` recovery state machine，不 leak 或使用 in-flight resource。
5. Hierarchy selection 經 `SceneDocument` round-trip；recovery failure 可處理；持久化的是 stable panel ID，
   不是 visible label 或 ImGui ID。
6. Linux automated gate 通過；Linux/X11 real-display 證據覆蓋 render/input/resize/recovery；Windows
   target-host 證據覆蓋 per-monitor DPI 與 IME composition/candidate position。未驗證平台必須明列，
   不得由「可編譯」推定通過。

## 2. 固定架構決策

以下決策在 ED-M0 期間視為已定案。若施工需要更改，必須先更新 ADR-0001（或新增 superseding ADR）、
本文件的雙語版本，以及受影響的 contract README，再合併程式碼。

| 主題 | 決策 | 理由／後果 |
| --- | --- | --- |
| UI framework | Dear ImGui docking，固定 `v1.91.9b-docking`。 | Immediate-mode UI 適合 custom RHI；不可追 moving branch。記錄確切 tag 與 MIT license。 |
| 取得方式 | CMake `FetchContent`，只有 `NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON` 才可觸及。 | Dependency 保持 optional；configure-off 在無網路/cache 時仍須成功。本 milestone 不加 submodule 或 system package requirement。 |
| Module 邊界 | `NexoraEditorImGui` 只依賴 public `Editor`、`Presentation`、`Window`、`RHI`；Editor Core 不依賴 ImGui。 | UI ownership 留在 portable document/transaction model 外；`Config/Modules/modules.json` 是權威來源。 |
| Window/presentation | Application 擁有唯一 `RenderSurface`；UI host 只在 frame 內借用。 | 禁止 GLFW/SDL、第二個 swapchain、private native handle 或 backend-specific window creation。 |
| Renderer | 只做一個基於 public RHI 的 backend，不分別複製 ImGui Vulkan/DX12/Metal renderer。Platform-specific code 留在 RHI/Presentation 下。 | 避免三份 renderer 漂移，也能使用 validation-device test。若 public RHI 不足，應擴充 generic RHI 並寫 contract，不可 downcast。 |
| Multi-viewport | 開 docking；OS-level ImGui multi-viewport 延後。 | 額外 platform window 需要 multi-surface ownership 設計。ED-M0 只需 docked main window，`ViewportsEnable` 保持關閉。 |
| Context | 一個 `EditorImGuiHost` 擁有一個 `ImGuiContext`；所有呼叫在 window-owner thread 串行執行。 | Draw data 只借用到下一次 `BeginFrame` 或 context 銷毀；background thread 不得呼叫 ImGui。 |
| ID/persistence | `ProductShell` panel/command ID 是 semantic identity；`###stable.id` 分離顯示名稱。 | 改名／在地化不可破壞 layout 或未來 accessibility semantics。Layout 由 Editor Core 以版本化 workspace 保存，不使用 unmanaged global `imgui.ini`。 |
| Input | `WindowEvent` 是唯一 input source。Key event 帶完整 modifier snapshot；text event 帶 Unicode scalar。 | 不可直接 poll Win32/X11/Cocoa；文字輸入與 shortcut key 分開。 |
| DPI/font | 座標是 logical UI unit；framebuffer scale 與 surface extent 決定 pixel。選定 DPI bucket 改變時重建 font resource。 | 只用 `FontGlobalScale` 是暫時 scaffold，無法保證清晰輸出。Style 必須由 immutable base 導出，避免累積縮放。 |
| IME | Text 由 `WindowEventType::Text` 進入；candidate positioning 經 `RenderSurface::SetImeCandidatePosition`。 | 不向 ImGui 暴露 native window handle；Windows target-host 證據必須存在。 |
| Texture | `ImTextureID` 對應 generation-checked Editor UI texture registry；font atlas 使用保留 entry。未知／過期 ID 使用 diagnostic fallback 並回報錯誤。 | 不可 reinterpret 任意 pointer/raw RHI handle；ED-M1 thumbnail 重用 backend 前必須具備。 |
| Recovery | Data layer 擁有 journal operation；UI 在正常編輯前提供 recover/discard，失敗時 modal 保持開啟。 | UI 不自行刪除或改寫 journal；每次 choice 只消費一次。 |
| Accessibility | Stable panel/command ID 是 semantic seed；native accessibility 留給 ED-M7。 | Dear ImGui 沒有 accessibility tree。ED-M0 記錄 handoff 並提供 keyboard-operable recovery/basic shell，不得宣稱 screen-reader 完成。 |
| Failure policy | 預期的 surface state 跳過／重建 frame；invalid contract 回 typed status/error；programmer invariant 在 Development assert。 | 禁止靜默 fallback 到 private renderer、exception 穿越 module boundary、minimized 時 busy loop。 |

## 3. Ownership、lifetime 與 frame contract

必要的 ownership graph：

```text
NexoraEditor process
  ProjectWorkspace / World / SceneDocument / ProductShell（application 擁有）
  RenderSurface（application 擁有並 drain）
  EditorImGuiHost（擁有 ImGuiContext 與 renderer cache）
    font atlas + UI texture registry + per-frame upload allocations
    借用的 WindowEvent span、models 與 acquired frame target
```

以下 frame 順序是強制 contract：

1. `RenderSurface::BeginFrame()` pump event，並 acquire 或描述 next target。
2. 用 `RecoveryAction` 解讀 `SurfaceStatus`。Abort 就退出；skip 不開始 ImGui render submission；
   recreate/resize 必須在記錄新 frame resource 前完成。
3. Borrowed event span 只 feed 一次，按 queue 順序處理 focus、pointer、button、wheel、key/modifier、
   text、close、resize、DPI。
4. 在 `BeginFrame` 後讀 `FrameInfo()`；把目前 logical extent、DPI、framebuffer scale 傳給 `SetDisplay`。
   Zero extent 代表 suspended/minimized：等待 event，不 allocate。
5. 依序且各一次呼叫 `BeginFrame(delta)`、建立 dockspace/panel/modal、`EndFrame()`。
6. 將 `ImDrawData` 轉成 RHI command：upload buffer、bind projection/pipeline/texture、套用 `DisplayPos` 與
   `FramebufferScale` 後 clamp scissor、依 callback policy 執行 callback，再以 `IdxOffset` 加
   `VtxOffset` draw。
7. Transition 到 presentation state、submit，最後只呼叫一次 `RenderSurface::EndFrame()`。
8. Upload/font/descriptor resource 只能在保護其最後使用的 completion value 完成後 retire。退出時先
   wait/drain，再 destroy host，最後 destroy surface。

Borrowed span、`ImDrawData`、acquired image handle 不可跨 frame cache。`ProjectWorkspace`、
`SceneDocument`、`ProductShell` 必須活過 `DrawProductShell` 呼叫。更改這些規則時，須同步更新
`Engine/EditorImGui/README.md` 與所屬 RHI/Presentation README。

## 4. 依序施工計畫

AI agent 必須依順序執行 work package。每個 package 都要以 focused test 與 `git diff` review 收尾；
不可把全部工作塞進一個無法 review 的變更。Checkbox／狀態只反映 repo 事實，不表示意圖。

### WP0 — Baseline 與 reproducibility audit

**狀態：部分完成。**

1. 讀 `CLAUDE.md`、Editor、EditorImGui、RHI、Window、Presentation README、ADR-0001 與兩份相關
   roadmap；編輯前先記錄 contract 衝突。
2. Configure/build/test `linux-development`；若更動 RHI ABI、module dependency、exported header 或
   link boundary，另跑 `linux-shipping`。
3. Graphical feature OFF 與 ON 各 configure 一次；確認 OFF 不 populate `nexora_imgui`，ON resolve 到
   pinned tag；validation note 記錄 resolved revision。不可 commit `_deps`、build tree 或
   `CMakeUserPresets.json`。
4. 盤點現有 test（`editor.imgui_contract`、presentation contract、module graph），把後續每一個驗收條件
   對應到 test 或 target-host checklist。

**Exit gate：**乾淨的 baseline result 與明確 gap list。若 configure 需要網路但沒有 cache，回報環境限制；
不可靜默關閉 feature。

### WP1 — 讓 public RHI 足以表達 ImGui

**狀態：source 與 validation contract 已實作；native target-host validation 仍待完成。**

1. 對照 `ImDrawVert`/`ImDrawIdx` 與每個 `ImDrawCmd` field，稽核 public RHI capability。必須能表達
   dynamic vertex/index upload、orthographic constants、alpha blending、depth test/write off、cull-none、
   scissor、sampled RGBA texture、indexed base-vertex draw。
2. 只新增缺少的 generic primitive，不得加入以 ImGui 命名的 RHI type。定義 texture upload row pitch、
   sampler/filter/address mode、descriptor lifetime、shader visibility、completion/fence ownership；若 contract
   有變，同步更新 `Engine/RHI/README.md`。
3. Generic primitive 先在 validation device 實作及 contract-test，再做 Vulkan。DX12/Metal 必須在各自
   target host 可編譯後才可視為 portable；未實跑前 acceptance 不勾選。
4. 經既有 shader pipeline 加入 deterministic shader source/reflection；禁止在 `EditorImGui.cpp` 內嵌
   backend-only ad-hoc bytecode。

**Exit gate：**validation-device test 證明 state transition、binding、scissor、indexed offset 與 resource
retirement；Vulkan offscreen frame 沒有 validation error。

### WP2 — 實作 retained GPU renderer resource

**狀態：source 與 validation contract 已實作；native target-host validation 仍待完成。**

1. 在 `EditorImGuiHost` 下建立 renderer-owned state：pipeline、sampler、font texture/view、descriptor
   binding、有限大小的 per-frame vertex/index upload buffer ring。知道 device/format 後才 lazy-create
   stable resource；禁止每 frame create/destroy pipeline 與 font texture。
2. Upload 真正 RGBA32 font atlas，把 registry ID 設進 `io.Fonts->TexID`，並在 context/font/DPI generation
   改變時重建。舊 generation 只能在 GPU completion 後 retire。
3. Flatten 或 stream 全部 draw list 且保留 per-list base offset；projection 由 `DisplayPos` 與
   `DisplaySize` 算出；支援 16/32-bit `ImDrawIdx`。
4. 每個 command 都要：處理 reset-render-state callback、明定 application callback policy、轉換/clamp
   clip rectangle、跳過空／越界 clip、bind generation-checked texture，並呼叫
   `DrawIndexed(ElemCount, ..., IdxOffset, VtxOffset)`。
5. Shader output 與 surface format 的 premultiplied/non-premultiplied blending 要一致；明確處理 sRGB 與
   UNORM。用 pixel-readback golden case 覆蓋 color、alpha、font UV、overlapping clip、non-zero display
   origin、non-zero vertex offset。
6. Allocation 必須有上限與可觀察性：需要時 geometric growth，completion 後重用；暴露 vertex、index、
   draw call、reallocation、rejected texture metrics。

**Exit gate：**重複 offscreen frame 產生穩定 pixel 與 allocation count；resize/font rebuild 不 leak；
sanitizer/validation 沒有 stale handle 或 out-of-bounds。

### WP3 — 把 renderer 接到 acquired presentation image

**狀態：Vulkan、DX12 與 Metal source 已實作；target-host validation 仍待完成。**

1. 為 renderer 增加最小的 public `RenderSurface` frame-target access，優先選 callback/encoder 或只在
   `BeginFrame` 到 `EndFrame` 間有效的 borrowed RHI target descriptor。禁止暴露 `VkImage`、
   `ID3D12Resource`、`MTLTexture` 或永久 image handle。
2. 定義 initial/final state、format、extent、generation，以及 resize/recreate 時的 invalidation。
   Presentation 繼續擁有 acquisition、synchronization、present。
3. 以 WP2 共用 command recording 取代 production `Render(RenderSurface&)` CPU loop；draw-list traversal
   最多只有一份實作。
4. 依 `RecoveryAction` 處理 `OutOfDate`、`Suboptimal`、`Occluded`、`Suspended`、device loss、close。
   Minimized 不可 spin；沒有 acquire 的 frame 不可呼叫 `EndFrame`。
5. 同一變更更新 `Engine/Presentation/README.md`、`Engine/EditorImGui/README.md`、module dependency、
   exported API test。

**Exit gate：**graphical executable 經 public surface present GPU-rendered ImGui；不再存在 per-frame
full-screen CPU RGBA buffer 或 readback/upload round trip。

### WP4 — 強化 input、docking、persistence 與 command routing

**狀態：portable core 與 versioned project-owned layout round-trip 已存在；target-host 證據未完成。**

1. 保留完整 key mapping（navigation/editing、punctuation、keypad、F1-F12、alphanumeric、左右 modifier），
   加入 press/release 與 modifier snapshot 的 table-driven test。
2. 測試 pointer leave/focus loss，確保 deactivation 後不殘留 stuck button/key。Wheel 固定
   horizontal=`value0`、vertical=`value1`，且只 normalize 一次。
3. Initial dock layout 固定為 Hierarchy 左、Console 下、center reserved。只在新 workspace/layout schema
   建立；之後 restore Editor-owned versioned layout。未知／缺少 panel ID 只診斷並忽略，不 crash。
4. Shortcut 先經 `ProductShell` command ID routing，再進 panel behavior。依 ImGui capture flag 決定
   gameplay/scene tool 是否收到 pointer/keyboard；不得以 visible label 當 command identity。
5. `ViewportsEnable` 保持關閉，增加 assertion/test，避免 ImGui upgrade 偷偷建立 native platform window。

**Exit gate：**synthetic-event automated test 通過、layout round-trip；real X11 session 證明 typing、shortcut、
drag docking、wheel axis、focus loss、close。

### WP5 — DPI、font 與 theme

**狀態：live extent/DPI forwarding、bucketed font rebuild 與 production GPU atlas upload 已有；
Windows 證據仍待完成。**

1. 定義小型 DPI bucket policy（例如 nearest supported scale 加 hysteresis）與 immutable base style；bucket
   改變時由 base 重算，禁止再縮放已縮放的 style。
2. 依 bucket pixel density 重建 atlas，同時維持 logical widget size；在 frame boundary 原子切換 font
   texture generation，延後銷毀舊 GPU resource。
3. Acquisition 後的 `FrameInfo` 是唯一真值。測 same-frame resize+DPI、monitor move、minimize/restore、
   fractional scale、rapid change。
4. 交付一套對比度足夠的 first-class dark theme；per-user theme editor 不屬於 ED-M0。

**Exit gate：**Windows 100%、125%、150%、200% screenshot/checklist 證明 text 清晰、hit target 正確、
無 cumulative scaling、無 stale extent frame。

### WP6 — IME 與 Unicode

**狀態：event forwarding 與 candidate callback 已有；target-host 證據未完成。**

1. 驗證 Unicode scalar（含 supplementary-plane）；無效 scalar 不可送進 `AddInputCharacter`。Key event
   不得重複產生 text event。
2. 每個 active frame 更新 `Platform_SetImeDataFn` 借用的 surface；frame/surface 結束即清除，避免 callback
   dereference 已銷毀 surface。
3. 把 ImGui logical cursor coordinate 經 viewport origin 與 DPI 轉成
   `SetImeCandidatePosition` 所要求的 client-pixel coordinate。
4. Windows 使用 Microsoft Pinyin 或另一個已安裝 IME 測試 composition start/update/commit、cancel、移動
   input cursor、切換 DPI/monitor、candidate positioning。Unsupported X11 行為不得當作 Windows 證據。

**Exit gate：**Windows recording/screenshot 與 checklist 證明 committed text 恰好一次，且 candidate 在至少
兩種 DPI 下位置正確。

### WP7 — Recovery UX 與基本 keyboard accessibility

**狀態：modal/data-layer call 與 automated X11 kill/relaunch recovery 已有；destructive discard
與 physical-display 證據仍待完成。**

1. 正常編輯可互動前偵測 journal。Recovery modal 取得 focus、限制 keyboard navigation，明確提供 Recover
   與 Discard；Discard 要有清楚破壞性文字，不能因 default button 取得 focus 就執行。
2. 只呼叫 `ProjectWorkspace::RecoverWorkspace`/`DiscardRecovery`。Operation 執行中防止 duplicate submit；
   失敗時保留 journal/modal、顯示可處理錯誤，允許 retry 或 safe exit。
3. Data-driven test 覆蓋 no journal、recover/discard success、recover/discard failure、恰好一次的
   `TakeRecoveryChoice`。Process-level test 在 journal durable 後 kill、relaunch 並驗證選擇。
4. 驗證 shell/modal keyboard traversal 與 visible focus。記錄 ED-M7 secondary accessibility tree 缺少的
   semantic data；plugin 不得 inspect ImGui widget tree。

**Exit gate：**自動化 failure path 通過；real-display kill/relaunch session 證明 recover/discard，且不損失
所選 policy 之外的資料。

### WP8 — Target-host matrix、證據、清理與 milestone 更新

**狀態：automated Linux X11 smoke/recovery 已實作；physical-display Linux 與 Windows 證據仍待
完成。**

1. Clean tree 執行 §6 完整 Linux gate。WP1/WP3 更動 linkage/API boundary，因此也跑 `linux-shipping`。
2. Real X11 display 執行 launch、font/text 可見、Hierarchy selection、docking、各類 input、resize/
   minimize/restore、recovery checklist；記錄 command、commit、backend/device、result、artifact location。
3. Windows 以 graphical feature build Development/Shipping，再執行 DPI/IME checklist。只有 surface 真正
   選擇 DX12 時才能宣稱 DX12，Vulkan result 不可代替。
4. 只有宣告 macOS Editor support 時 macOS/Metal 才成為必需 supported-backend parity；macOS target host
   實跑前只能列 unverified，不可列 passed。
5. 移除 production CPU compositor/dead scaffold、更新 contract README、同步雙語 roadmap、review final
   diff，最後才更新 ED-M0 狀態。只有 panel 存在不構成驗收。

**Exit gate：**每個必要 evidence row 都有 link/result，沒有任何 required row 寫「assumed」。

## 5. 技術問題與解法登錄表

| 問題 | 必要解法 | 禁止捷徑 | 驗證 |
| --- | --- | --- | --- |
| RHI 缺 renderer operation | 新增 backend-neutral RHI contract、validation implementation，再做 native implementation。 | EditorImGui include Vulkan/DX12/Metal header 或 downcast device。 | Validation trace 加 native validation。 |
| Swapchain image 由 Presentation 擁有 | 借用帶 generation/state contract 的 frame-scoped RHI target/encoder。 | 另建 swapchain 或暴露永久 native image handle。 | Resize/recreate stress 與 stale-generation rejection。 |
| GPU/CPU lifetime 不同步 | Completion-tracked ring buffer 與 deferred destruction。 | 除非 contract 保證完成，否則不可在 `Submit` 後立刻 destroy upload/font resource。 | Multi-frame validation/sanitizer stress。 |
| Font atlas 目前只是 placeholder | Upload 真 atlas pixel，每 generation 保留穩定 texture registration。 | Bind 1x1 texture 或每 draw 重建。 | Font pixel golden 與穩定 allocation metric。 |
| 任意 `ImTextureID` | Generation-checked registry 加 fallback/error。 | Cast pointer/raw handle。 | Valid、stale、unknown、destroyed texture test。 |
| Clip/offset bug | 套用 display origin/framebuffer scale、clamp、保留兩種 offset 與 index width。 | 假設 origin zero 或只有 16-bit index。 | Synthetic multi-list draw golden。 |
| DPI 模糊／累積放大 | Bucketed atlas rebuild；style 從 immutable base 產生。 | 只靠 `FontGlobalScale` 或重複 `ScaleAllSizes`。 | Multi-DPI screenshot 與 numeric style test。 |
| Shortcut/text 重複 | Key route command；只有 text event 加 character；遵守 capture/focus。 | 由 virtual key 推導文字。 | Unicode 與 shortcut collision test。 |
| IME use-after-free／位置錯 | Frame-scoped surface binding，加 logical-to-client-pixel conversion。 | 永久 cache native handle/raw surface。 | Destroy/recreate 與 multi-DPI IME test。 |
| Corrupt recovery journal | 保留 journal、顯示 error，依 data-layer policy 提供 retry/discard。 | Auto-delete、auto-recover、隱藏錯誤。 | Injected I/O/corruption test。 |
| Configure 時無網路 | OFF build 獨立；ON build 清楚失敗或使用核准的 pre-populated cache。 | Fetch unpinned branch 或靜默 build stub。 | Clean configure OFF/ON。 |
| Scope creep | 限制在 shell/Hierarchy/Console/recovery；延後 Content Browser、viewport gizmo、PIE、profiler。 | 因有 dock window 就把後續 Editor milestone 標完成。 | Roadmap review。 |

## 6. 驗證命令與證據格式

施工中執行 focused test，交付前執行完整 gate：

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development

cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

若 preset 開了 graphical shell，還要明確 configure 一次 feature-off build；現有 focused test 可用：

```bash
ctest --preset linux-development -R 'editor.imgui_contract|window_presentation.contracts|build.module_graph' --output-on-failure
```

不可用 Linux 宣稱 Windows/macOS validation。每筆 manual evidence 必須包含：

```text
commit: <sha>
host/os: <exact version>
window backend / RHI backend / GPU / driver: <values>
configuration and command: <values>
scenario: <acceptance checklist id>
result: pass | fail | blocked
artifacts: <log/screenshot/video path>
notes: <validation messages or limitation>
```

必要 automated coverage 包括 feature OFF/ON configure、module graph、context lifetime、event/key table、
dock/layout ID、各 recovery outcome、draw-list conversion、texture generation、clip/offset/index-width golden、
resize/recreate、resource retirement、重複 frame 且 allocation 有界。必要 human evidence 包括 output 可讀、
真實互動、docking、focus、DPI、IME、crash recovery；screenshot 本身無法證明 input/lifetime 行為。

## 7. AI 施工與變更紀律

- 一次只做一個 WP 與一個 architectural boundary。編輯前在 change note 寫明 invariant、files、expected
  test、rollback point。
- 新增 abstraction 前先搜尋既有 public abstraction；不可從 roadmap 猜 API，必須查 header/test。
  不可修改 generated build output 或 fetched dependency source。
- Patch 要可 review：RHI contract、backend implementation、Editor integration、evidence update 應各自使用
  conventional commit，除非 atomic compilation 確實要求合併。
- 只用 repo `.clang-format` 格式化碰過的 C++。English/Traditional Chinese roadmap 在同一 commit 同步。
- Ownership、lifetime、threading、error、deferred-work behavior 改變時，同 patch 更新 owning README；
  dependency 改變時更新 `Config/Modules/modules.json` 與 test。
- Gate 失敗不算完成。記錄確切 failure、保留最後 buildable commit；應 revert 目前 WP，不可加 private
  bypass。
- 不可只看 source 就勾 status。「Implemented」需要 automated result；「accepted」還需要上文指定的
  target-host evidence。

## 8. 明確 non-goal 與後續 handoff

ED-M0 不交付 Content Browser/thumbnail、Scene View、gizmo、Game View/PIE、inspector widget、specialized
tool、build/profile UI、detached OS-level ImGui viewport、user-authored theme 或 screen-reader accessibility
tree。WP2 texture registry 只是未來 panel 可使用的 backend contract，不代表可以提早施工。

ED-M7 handoff 必須列出 stable semantic ID、label、role/action/value gap、focus order、keyboard-only failure、
live-region 需求與候選 platform bridge。該文件可建議 secondary accessibility library，但採用 dependency
需要獨立決策與核准。在此之前，只能誠實宣稱「basic keyboard-operable ED-M0 shell；screen-reader
support 尚未實作」。
