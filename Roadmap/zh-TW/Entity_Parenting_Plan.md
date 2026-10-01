# Entity Parenting（Transform 階層）— 計畫

> 版本：v1.1｜狀態：**方向已由負責人核准（沿用 Unity／Unreal 慣例）；✅ 階段 1、2、3（資料模型）與
> 4（GPU scene 同步）已完成；Editor roadmap 現已有圖形化 tree／filter／selection／reparent／reorder
> foundation，rename、virtualization 與 gizmo 仍待完成**｜更新：2026-10-02｜對應：
> `Transform_Rotation_Scale_Plan.md`、`Editor_Roadmap.md` §ED-M2

## 1. 目的

Unity 與 Unreal 的 transform 都是相對於父物件：移動、旋轉或縮放父物件，整棵子樹都會跟著動。Nexora 的
entity 沒有階層，所以每個 `runtime::Transform` 都是世界座標，而 Editor 的 `SceneDocument` 另外存了一份
只供顯示、不影響任何位置的父子欄位。World／local gizmo 模式、pivot，以及「把劍掛到手上」都需要真正的
階層。負責人要求沿用 Unity／Unreal，讓使用者可以沿用既有習慣。

## 2. 已驗證的現況

- `runtime::Entity` 沒有 parent。`SceneDocument`（Editor）存 `{id, parent, name}` 節點，id 就是 entity id；
  `Reparent` 會拒絕循環，但只修改這份中繼資料，也不能復原。
- Editor 場景檔（`NEXORA_EDITOR_SCENE 1`）寫出 `node <id> <parent> <name>` 行與一份 runtime 快照。由於從沒有
  任何東西套用 parent，它們的 transform 是以世界座標編寫的。
- 刪除 entity 只會刪除該 entity。`GameWorld` 只會釋放它實際刪除的那些 id 的音效／物理／角色綁定。
- 渲染與物理碰撞體目前都不讀 entity transform。角色控制器會讀寫 entity 位置。Zig／C wire 傳遞的是位置。

## 3. 決定（Unity 慣例）

- **`Transform` 是 local**（相對於父物件）；根物件的 local 等於 world。world transform 依父鏈即時計算。
- **父子必須在同一個場景**；循環與以自己為父都會被拒絕。
- **`SetParent(entity, parent, keep_world = true)`** 對應 Unity 的 `SetParent(parent, worldPositionStays)`：
  預設保持世界姿態、重新計算 local；`keep_world = false` 時保留 local，物件隨新父物件移動。
- **刪除 entity 會一併刪除子孫**（Unity 對 GameObject 的 `Destroy`）。所有被刪 entity 的綁定都會釋放，
  Editor 的復原會還原整棵子樹。
- **旋轉的子物件位於非等比縮放的父物件底下會產生剪切（shear）**，平移／旋轉／縮放的 transform 無法表示。
  如同 Unity 的 `lossyScale`，`WorldTransform` 回傳最接近的 TRS（逐分量縮放），`WorldMatrix` 回傳精確的 4x4
  仿射矩陣。在這類父物件底下以 `keep_world` 重新掛接也有同樣限制。
- **快照升級為 `NEXORA_SCENE 3`**，在 entity id 之後加入 parent id；v1 與 v2 載入時所有 entity 都是根物件。
- **Editor 以 runtime 階層為唯一資料來源。**舊的 Editor 場景檔會以 `keep_world = true` 套用原本只供顯示的
  parent 來遷移，因此不會有物件移位。

## 4. 階段

1. ✅ **Runtime 核心與 Editor 統一。**parent 欄位、含驗證的 `SetParent`、
   `Parent`／`Children`／`WorldTransform`／`WorldMatrix`、回報被刪 id 的連帶刪除、快照 v3、可復原的
   `SceneEditor::SetParent` 與能還原子樹的刪除復原、`GameWorld` 包裝，以及 `SceneDocument` 改從 runtime 讀取
   parent。本階段有角色控制器的 entity 必須是根物件；階段 2 已解除此限制（見 §5）。
2. ✅ **Gameplay 邊界。**有版本的 Zig／C parent 與 world transform wire（`"Nexora.Parent"`、`"Nexora.WorldTransform"` 與 `"Nexora.TransformV2"`）；父物件底下的角色控制器，沿用 Unity：控制器在世界空間移動，每次 tick 從 transform 當下的世界位置開始（移動中的父物件會帶著它走），結果再存回 local。
3. ✅ **Editor 工具（資料模型；圖形化工具屬 Editor roadmap）。**world／local 與 pivot gizmo 模式（數學已完成：`ViewportMath.h` 的 `GizmoAxes`、`ApplyGizmo`、`GizmoRoots`，與 Unity 一樣以世界 TRS 運算）、Hierarchy 拖曳重新掛接與兄弟順序（已完成：`SetSiblingIndex`／`SiblingIndex`、重新掛接後成為最後一個子物件、可復原的 `SceneEditor::Move`、`SceneDocument::Move`，以及依兄弟順序列出的 `Nodes()`）。
4. ✅ **渲染（GPU scene 同步；尚無應用程式透過它繪製）。**當渲染器開始讀 entity transform 時，必須使用 `WorldMatrix`。`RenderSceneSync`（`RenderSync.h`）就是這個讀取者：它把作用中場景的 mesh renderer 同步到 `renderer::GPUScene`，使用每個 entity 精確的 world matrix（含 shear）與保守的世界包圍球（半徑以實際上傳之 float 矩陣的 spectral norm 放大，再加上 shader 最壞情況的 float 誤差），因此移動父物件會更新所有被渲染的子孫。每次同步會快取矩陣，損壞的父鏈在第一次重訪時就判定失敗，成本與 entity 數量成線性；超出 float 範圍的姿態（包括中間加總會溢位者）不會進入 GPU scene。同步器以 `InstanceId` 綁定到它的 `GPUScene`，在相同位址重建或被 `Clear()` 的場景不會被誤認為持有其物件的那一個。攝影機也會跟著階層走：`CameraView` 以攝影機 world matrix 的位置與世界旋轉建立視角（與 Unity 一樣忽略縮放），`RenderSceneSync::RenderFrame` 會先透過該攝影機對 GPU scene 做剔除再送出。把應用程式的繪製迴圈接上它，屬於渲染器與 Editor viewport 的工作。

## 5. 階段 1 的限制（明文記錄，不隱藏）

- ~~有角色控制器的 entity 必須是根物件。~~ 階段 2 已解除：控制器會在世界位置與子物件的 local transform 之間換算。
- Zig／C transform wire 繼續傳遞 **local** 位置；對根物件而言就是世界位置，因此既有 gameplay module 行為不變。
- `PlaySession` 的 apply-back 仍只套用 transform；遊玩期間的 parent 變更不會套回，且遊玩期間 parent
  改變過的 entity 會回報為 apply-back 衝突（它的 local 值屬於另一個 parent，照抄會讓它移位）。
- ~~除了場景儲存順序外，沒有兄弟排序。~~ 階段 3 已加入 Unity 式的兄弟索引，以場景儲存順序保存（快照格式不需變更）。

## 6. 風險

| 風險 | 緩解 |
| --- | --- |
| 遷移後舊 Editor 場景的物件移位 | 以 `keep_world = true` 套用舊 parent，並加測試 |
| 同一批命令互相影響（先重新掛接再刪除） | 在修改任何東西之前，以模擬階層驗證整批命令 |
| 連帶刪除漏放綁定 | `GameWorld` 對每個回報的被刪 id 釋放綁定 |
| 快照中過深或損壞的階層 | 載入時拒絕不存在的 parent、以自己為父與循環 |
| 舊場景遷移失敗時留下載入一半的場景 | 載入前先在暫存 world 上預演遷移 |
| Play apply-back 跨 parent 變更照抄 local 值 | 將這類 entity 回報為衝突 |
