# ADR-0008：可選的 Editor immutable mesh geometry residency

## 決策

新增 `SceneDrawData::retainMeshGeometry` 與預設 false 的
`ISurface::SupportsSceneGeometryResidency()` capability，由 `RenderSurface` 轉送。
Editor Scene／Game 只在 backend 支援時要求最佳化。Vulkan 提供第一份 native adapter；
DX12／Metal 保留既有 draw，並明確拒絕尚未支援的要求。Public C++ consumer 須重新建置。
Stable C／Zig、serialized asset identity、shader 與 module dependency 不變。

Adapter 仍在 cache admission 前驗證完整目前提交。每個 batch 的 referenced contiguous
vertex interval 與 locally rebased index slice 構成 owning immutable key。Fingerprint
快速排除候選，真正命中仍須核對完整 vertex／index bytes。Combined upload 位置、instance
與 material 不是 identity。相同內容可跨 asset ID 共用 buffer；reimport 與 frozen Play
版本可同時存在。資產不能只靠 ID 或缺失的目前 CPU payload 繪製。Submesh range 可佔獨立 entry。

## Ownership 與上限

第一份 adapter 每次提交最多處理 64 個 batch、保留 64 個 entry。更大的 draw 使用既有
combined upload；重複 batch range 共用 admission。常駐 native memory allocation requirement
上限 32 MiB，另有最多 8 MiB 的 staged candidate。Key 另持有 CPU payload，以各自 native
allocation 為上限。這些是 cache 專屬限制；既有 frame upload、target 與 texture 有獨立預算。
Host-visible coherent immutable buffer 不代表 device-local VRAM。

Allocation、mapping、copying 與 GPU completion 先於 eviction／publication；LRU planning
不改動 ownership。目前 unsubmitted draw 已選用的 resource 會被 lease，不能淘汰；其他
victim 的先前 submitted user 必須完成才能釋放。Admission 失敗保留 resident，該 batch
回退至已驗證的 legacy geometry。Resize 排空 frame resource，但保留 immutable mesh entry；
shutdown 排空後釋放所有 key／buffer。

Instance、material、lighting 與 tone 每次 draw 都重新複製。全部 batch 常駐時，frame buffer
省略 combined geometry prefix；在該位置寫入 instance data 會清除舊 allocation-bound
prefix key，避免切回 legacy 時錯誤命中。Main、shadow 與 reflection draw 綁定同一份選用的
immutable geometry；native index 做 local rebasing，public range 仍是 borrowed submission
中的 absolute index。

## 驗證與後續工作

Portable tests 涵蓋跨 layout canonical identity、hash collision 下的完整核對、owning
payload、entry／memory 上限、LRU／unsubmitted lease、abandoned plan 與 teardown。
真正 Vulkan tracing／pixels 涵蓋 reorder、geometry 變更、instance／material 更新、配置
失敗回退／重試、legacy 切換、budget eviction、completion 失敗、resize 與清理。
執行完整 Linux Development／Shipping 及 hosted CI。DX12／Metal adapter、更完整的 residency
policy 與完整 target-host ED-M2 驗收仍待完成。
