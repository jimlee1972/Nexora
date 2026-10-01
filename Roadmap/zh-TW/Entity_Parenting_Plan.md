# Entity Parenting（Transform 階層）— 計畫

> 版本：v1.0｜狀態：**方向已由負責人核准（沿用 Unity／Unreal 慣例）；階段 1 施工中，其餘階段尚未開始**｜
> 更新：2026-10-01｜對應：`Transform_Rotation_Scale_Plan.md`、`Editor_Roadmap.md` §ED-M2

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

1. **Runtime 核心與 Editor 統一（本次變更）。**parent 欄位、含驗證的 `SetParent`、
   `Parent`／`Children`／`WorldTransform`／`WorldMatrix`、回報被刪 id 的連帶刪除、快照 v3、可復原的
   `SceneEditor::SetParent` 與能還原子樹的刪除復原、`GameWorld` 包裝，以及 `SceneDocument` 改從 runtime 讀取
   parent。本階段有角色控制器的 entity 必須是根物件（見 §5）。
2. **Gameplay 邊界。**有版本的 Zig／C parent 與 world transform wire；父物件底下的角色控制器。
3. **Editor 工具。**以 `WorldMatrix` 為基礎的 world／local 與 pivot gizmo 模式、Hierarchy 拖曳重新掛接、兄弟順序。
4. **渲染。**當渲染器開始讀 entity transform 時，必須使用 `WorldMatrix`。

## 5. 階段 1 的限制（明文記錄，不隱藏）

- 有角色控制器的 entity 必須是根物件，已有父物件的 entity 也不能加角色控制器；控制器寫入的是世界位置，
  而子物件存的是 local。
- Zig／C transform wire 繼續傳遞 **local** 位置；對根物件而言就是世界位置，因此既有 gameplay module 行為不變。
- `PlaySession` 的 apply-back 仍只套用 transform；遊玩期間的 parent 變更不會套回，且遊玩期間 parent
  改變過的 entity 會回報為 apply-back 衝突（它的 local 值屬於另一個 parent，照抄會讓它移位）。
- 除了場景儲存順序外，沒有兄弟排序。

## 6. 風險

| 風險 | 緩解 |
| --- | --- |
| 遷移後舊 Editor 場景的物件移位 | 以 `keep_world = true` 套用舊 parent，並加測試 |
| 同一批命令互相影響（先重新掛接再刪除） | 在修改任何東西之前，以模擬階層驗證整批命令 |
| 連帶刪除漏放綁定 | `GameWorld` 對每個回報的被刪 id 釋放綁定 |
| 快照中過深或損壞的階層 | 載入時拒絕不存在的 parent、以自己為父與循環 |
| 舊場景遷移失敗時留下載入一半的場景 | 載入前先在暫存 world 上預演遷移 |
| Play apply-back 跨 parent 變更照抄 local 值 | 將這類 entity 回報為衝突 |
