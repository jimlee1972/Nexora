# ADR-0005：Editor scalar PBR 材質資產

日期：2026-10-08。狀態：實作方向；驗收另行記錄。

## 決策

將 ADR-0004 的 application-owned 材質轉換延伸至 Editor authoring。Renderer 保有材質驗證與
切線產生；Presentation 保有 shared native PBR pipeline、binding 與 protecting-frame resource
lifetime。Editor 匯入 immutable owning CPU 材質資產，在 authoring thread 發布並建立暫時的
繪製 palette。匯入、catalog 發布與 document 編輯都不執行原生 GPU 工作。

第一版 `.nmaterial` 是有界、可編輯且不受 locale 影響的 whitespace-token 格式：

```text
NEXORA_MATERIAL 1
base_color 0.8 0.2 0.1
metallic 0
roughness 0.5
occlusion 1
emission 0 0 0
```

Token 必須依此順序；可使用任意空白，但不接受註解或額外 token。Source 上限為 64 KiB。
Base color、metallic、roughness、occlusion 必須是 [0,1] 的有限值；emission 是 [0,65504] 的
有限 linear RGB。只支援 opaque PBR。Scalar fields 為 canonical；typed asset 同時具有其精確衍生的 Renderer `MaterialSchema`。
發布會拒絕不一致參數、不支援的 shader／profile／features 與非法 scalar 值；
不序列化 native descriptor、texture、任意 shader、graph 或 IBL policy。成功匯入保有 persistent
UUID metadata 與 owning payload，跨 indexing、background／synchronous reimport 與 Content
Undo 保留。取消、不支援 source version、非法值與過期發布保留最後的有效 live artifact。
Workspace typed material 數量上限為 4096。

## 參照與相容性

不重新解釋 `Runtime::MaterialComponent::shader`，完整保留 legacy 64-bit 值。改以 Editor-owned
opaque document component `editor.material.asset` 保存參照，reserved TypeId 為
`0x45444d41544c0001`。17-byte payload 先存 byte version 1，再以明確 little-endian 順序儲存
UUID 的 high、low uint64。Zero UUID、錯誤 type／name、截斷與不支援版本維持 unresolved；
其 owning bytes 仍可保存，且本版指派 UI 不得覆寫。既有 scene container 與 stable C/Zig schema
不變。未來 Runtime／cook consumer 需要另外明確整合。

Catalog 只在目前非零 project generation 解析 UUID；snapshot 在 reimport／unload 後仍擁有材質
資料。Inspector 指派要求單選且具有 live Mesh Renderer、可寫 project／content、相符的 entity／
document／project generation，以及 Content 與 catalog 中相同的 live typed payload。成功指派
使用既有 opaque-component Undo transaction。多選與移除參照另行處理。缺失參照保留且誠實顯示。

## 繪製與剩餘 gate

Scene View 將有效 scalar asset 轉換為至多 64 個 borrowed `SceneMaterial` slot，保留 slot zero
作 neutral fallback。每個 material UUID 在一個 frame palette 中只出現一次。超過預算使用
fallback，不改寫 authored 參照。有效材質使用白色 instance tint 保留 authored 顏色；ground、
proxy 與 gizmo 保有既有 tint。沒有 resolved 材質時保留 legacy Lambert path。PBR 使用
Renderer 產生的切線（包含穩定 degenerate-UV fallback）與精確 affine instance matrix。

Editor 在 module graph 與 CMake 宣告直接 Renderer 相依。新增 C++ payload／catalog／UI boundary
需要重建 consumer。本切片不代表 persistent per-asset GPU geometry residency、texture／shader
編輯、shipping material asset、physical-GPU 輸出或完整 ED-M2 多 DPI authoring
驗收已完成。

## Play snapshot 擴充

Game View 在 clone Play 前凍結 scalar 值與 mesh entity 的 UUID 指派。Owning palette 沿用
64-slot 上限與 neutral fallback；catalog generation 不符會在 mutation 前拒絕 Start。
Reimport／delete／重新指派不會改變目前 Play palette。Game frame 複製 palette、post-tick
精確 affine geometry 與選定 camera 的世界位置；Renderer tangent 失敗會保留 geometry 並
退回 Lambert shading。Stop 釋放 snapshot，新 Start 才讀取目前 authoring。Runtime 新建立的
entity 使用 neutral slot。Texture／shader 編輯、動態 material-reference mutation、多個原生
canvas 與 physical 驗收仍未完成。[Linux 證據](../../Tools/Build/evidence/EditorEDM3-GameMaterials-Linux-2026-10-09.md)。
