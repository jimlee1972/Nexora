# Transform 旋轉與縮放擴充 — 計畫

> 版本：v1.0｜狀態：**僅為計畫；本文件內容皆未實作、也未核准**｜更新：2026-10-01｜對應：
> `Editor_Roadmap.md` §ED-M2（gizmo）、`Engine_API_Foundation_Roadmap.md`

## 1. 目的與需要的決定

`runtime::Transform` 目前只有位置（`double x, y, z`）。Editor 的 portable viewport 數學
（`ViewportMath.h`）已能做 picking 與沿軸拖曳，但在 entity 真正擁有旋轉與縮放之前，旋轉 gizmo、
縮放 gizmo、world／local 空間、pivot 規則與負縮放處理都無法被定義與測試。本計畫先記錄這項變更
會影響的範圍，讓它可以被當成**一個決定**來審閱，而不是逐檔案才發現。本文件不是開始實作的授權。

**需要的決定：**核准（或修改）§4 的建議方案與 §6 的階段。

## 2. 已驗證的現況

- `runtime::Transform`（`Runtime.h`）為 `{double x, y, z}`，使用 defaulted 相等比較。
- 場景快照是文字 schema `NEXORA_SCENE 1`；每個 entity 寫出 `id x y z camera light mesh_renderer …`，
  載入時會拒絕非有限的位置。`runtime.v1_m4_vertical_slice` 斷言存檔／讀檔往返為位元組相同。
- Zig／C gameplay 邊界透過 `GameplayTransformWire` 交換 transform：三個緊密排列的 double，以
  `"Nexora.Transform"` 的穩定 component ID 為鍵。`GameplayHostBridge.h` 明確說明此 wire format
  刻意獨立於 `runtime::Transform` 的記憶體佈局，因此新增成員本身不會改變 C ABI。
- `GameWorld`／`SceneEditor`／`PlaySession`（apply-back 的 `TransformApplyDiff`）以及 Editor 的
  `GizmoTransaction`、`UnknownComponentStore`、`ViewportMath` 都以 by-value 方式使用
  `runtime::Transform`。
- Renderer 端已有完整數學：`math::Transform` 搭配 `Compose`／`Decompose`（`Math.h`），而
  `GPUScene::UpdateTransform` 接受 `math::Matrix4`。
- 在 `Math.h` 之外提到 `Transform` 的檔案約 25 個（Runtime、Editor、Renderer GPU scene、
  Presentation、Showcase、Zig module 與測試）。

## 3. 限制

- 公開 C ABI 與 Zig wire format 不可悄悄改變。任何新增都必須是加法式且有版本。
- 既有 v1 快照必須能繼續載入，旋轉為單位、縮放為 1。
- 新欄位必須維持往返決定性（固定浮點格式、不受 locale 影響）。
- 驗證必須在 loader 與 setter 中拒絕非有限值、無法正規化的 quaternion，以及為零或非有限的縮放，
  與目前驗證位置的方式一致。
- 不得宣稱圖形 Editor 有進展：在真正的圖形驗收出現前，Editor 仍為 0/8。

## 4. 方案

**A. 原地擴充 `runtime::Transform`（建議）。**加入 `rotation`（單位 quaternion，預設單位）與
`scale`（預設 1,1,1），位置成員維持在最前。單一 component 仍是唯一資料來源，`Compose`／`Decompose`
已能對應到 renderer。代價：所有 by-value 使用者都需重新編譯；`Transform{x, y, z}` 這類 aggregate
初始化仍可運作，是因為新成員有預設值。

**B. 在 `Transform` 旁新增獨立的 `Rotation`／`Scale` component。**不更動既有程式碼，但兩個 component
必須保持一致，gizmo 與 undo 必須原子地寫入兩者，Zig wire 也需要第二個 component ID。相同結果但
活動零件更多。

**C. 只存矩陣。**儲存 `Matrix4`。會失去作者意圖（Euler／quaternion、負縮放歧義），Editor 欄位也會有
損耗，不建議。

## 5. 方案 A 的影響

| 區域 | 變更 | 相容性 |
| --- | --- | --- |
| `Runtime.h` | 加入有預設值的 `rotation`、`scale` | 對 x,y,z 的位置式初始化維持 source 相容 |
| 快照 | 寫出含額外 token 的 `NEXORA_SCENE 2`；繼續讀取 `1`（單位旋轉／單位縮放） | v1 照常載入；除非重新存檔，不會改寫 v1 檔 |
| Zig／C wire | `GameplayTransformWire` 維持不變；為旋轉／縮放新增**有版本的**新 wire struct 與 component ID | 既有 module 繼續運作，無 ABI 破壞 |
| `SceneEditor`、`PlaySession`、`TransformApplyDiff` | 比較並套用所有欄位 | 衝突規則延伸到旋轉／縮放 |
| `GizmoTransaction` | 快照並還原完整 transform | cancel 語意不變 |
| `ViewportMath` | 加入繞軸旋轉角度、沿軸縮放比例、local／world 軸、pivot | 僅 CPU，測試方式同既有數學 |
| Renderer GPU scene | 透過 `math::Compose` 建立 `Matrix4` | 已支援 |
| 文件 | Runtime、Editor、Renderer README；Editor roadmap 列；根 README（雙語） | — |

## 6. 階段（每階段以證據結尾，而非宣稱）

1. **資料模型。**擴充 `Transform`、驗證與預設值；單元測試涵蓋單位預設值，以及拒絕
   NaN／inf／零縮放／退化 quaternion。Gate：`linux-development` 與完整功能組態通過；以 `-Werror`
   對 Windows 與 macOS 做 Zig 交叉編譯。
2. **持久化。**快照 v2 writer、v1 reader、決定性往返，並把惡意輸入案例加入
   `editor.parser_robustness`。Gate：v1 fixture 仍可載入，並重新存成文件所述的 v2 形式。
3. **邊界。**新增有版本的 Zig／C wire component；ABI layout 測試；既有 Zig module 不變且仍通過。
   Gate：Zig gameplay 測試與 ABI layout gate。
4. **Editor 數學。**旋轉／縮放 gizmo 數學，含 world／local／pivot 與負縮放規則，以及多選 pivot。
   Gate：決定性測試與書面決策表。
5. **文件與狀態。**更新 contract README 與雙語 roadmap／README 文字。圖形 Editor 仍為未驗收。

## 7. 風險

| 風險 | 緩解 |
| --- | --- |
| 比較或複製 `Transform` 的程式碼出現無聲的行為改變 | defaulted 相等現在包含新欄位；階段 1 稽核所有 `==` 與 by-value 複製 |
| 浮點格式差異破壞位元組相同的快照 | 固定格式（`max_digits10`、classic locale）並加往返測試 |
| 反覆編輯造成 quaternion 漂移 | 設定與載入時正規化；測試長序列編輯 |
| 負縮放翻轉 winding 與 gizmo 手性 | 在任何 gizmo 細修前先訂規則並測試 |

## 8. 不在範圍內

階層／parent transform、動畫 retargeting、physics body 同步，以及任何圖形 Scene View。這些需要各自的計畫。

## 9. 需要負責人回答的問題

1. 方案 A、B 還是 C？
2. 快照是在第一次存檔時遷移到 v2，還是除非旋轉／縮放非預設值才維持 v1？
3. Inspector 用 Euler 角、儲存用 quaternion，是否確認？
