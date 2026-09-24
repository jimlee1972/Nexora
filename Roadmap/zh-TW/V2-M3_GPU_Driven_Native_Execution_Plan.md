# V2-M3 GPU-Driven Rendering — Native Backend 執行計畫

> 版本：v1.0｜狀態：提案計畫，尚未開始｜更新：2026-09-24｜對應：
> `跨平台3D_Engine_V2_完整規劃書_v1_4.md` §V2-M3

## 1. 目的

V2-M3 的 gate 有四項已勾選（portable command batching、no-readback contract diagnostics、
RenderGraph queue/barrier ownership、CPU-reference correctness comparison），還有一項未勾選：
**native DX12/Vulkan/Metal target-tier parity on target hosts**。這份文件規劃要補上那一項所需的
工作。這是一份計畫，不是 milestone 進度更新：目前文件裡的東西都還沒實作，下面各階段實際落地並
通過各自的 gate 之前，roadmap 進度百分比不會變動。

## 2. 現況基線（對照原始碼逐一確認過，不是只憑 roadmap 文字）

`Engine/Renderer/src/GPUDrivenPipeline.cpp` 的 `RecordGPUDrivenExecution()` 是唯一會驅動真實
（非測試）GPU-driven 路徑的呼叫點：它呼叫 `compute_commands.Dispatch(...)` 做 culling/compaction，
呼叫 `graphics_commands.DrawIndirect(...)` 做壓縮後的 draw。兩者都是 `rhi::CommandList`
（`Engine/RHI/include/Nexora/RHI/Device.h`）的 virtual method，*base* 實作在沒有被 backend
override 的情況下會直接丟例外（`"compute dispatch is unsupported"` /
`"indirect drawing is unsupported"`）。逐 backend 確認結果：

| Backend | `Dispatch` | `DrawIndirect` | 證據 |
| --- | --- | --- | --- |
| `ValidationDevice`（portable CPU reference） | ✅ 已 override | ✅ 已 override | `renderer.v2_gpu_driven` 會完整跑過 culling/Hi-Z/compaction/indirect-generation 整條 pipeline，且是 deterministic 的。 |
| `VulkanDevice` | ❌ 未 override（會丟例外） | ✅ 已 override（`vkCmdDrawIndirect`） | `renderer.contracts`（`Tests/Renderer/RendererTests.cpp::VerifyNativeBackend`）會在真實 Linux Vulkan 上透過一個極簡的 triangle frame 跑 `DrawIndirect`——**不是**透過 `RecordGPUDrivenExecution`，也從未呼叫過 `Dispatch`。 |
| `D3D12Device` | ❌ 未 override（會丟例外） | ❌ 未 override（會丟例外） | `Engine/RHI/src/D3D12Device.cpp` 裡這兩個 symbol 出現次數都是 0。 |
| `MetalDevice` | ❌ 未 override（會丟例外） | ❌ 未 override（會丟例外） | `Engine/RHI/src/MetalDevice.mm` 裡這兩個 symbol 出現次數都是 0。 |

簡單講：完整的 GPU-driven pipeline（`RecordGPUDrivenExecution`）**目前只跑過 CPU reference**。
Vulkan 只有來自另一個無關、更簡單的 smoke test 的 indirect-draw 證據。Compute dispatch 從來沒有在
任何 native backend 上執行過。D3D12 跟 Metal 現在完全無法執行這條路徑的任何部分——呼叫進去會直接
丟例外。這跟 `Engine/Renderer/README.md` 自己的敘述一致：「Native Vulkan compute pipelines、
native queue/timeline integration、DX12/Metal execution 與 target-host parity 都還是 open gate，
不能從 validation-backend 或 Vulkan-indirect 的覆蓋率去推論。」這份計畫是第一次把這件事拆成有順序
的工程步驟寫下來。

## 3. 範圍與不做的事

範圍內：在三個 native backend 上補齊（缺的部分）`Dispatch`/`DrawIndirect`、把
culling/Hi-Z/compaction/indirect-generation 各階段實際需要的 compute shader 寫出來、讓
`RecordGPUDrivenExecution` 在不改動的情況下可以對每個 native device 執行、以及 target-host
驗證。

範圍外（屬於之後另外的工作，這份計畫不動）：V2-M3「then add」那批後續項目（temporal upscaler
interface、compute skinning、meshlet metadata）——這些要等 target-tier parity 之後才做，不是之前；
超出 `RenderGraph` 既有 ownership-barrier tracking 涵蓋範圍的 async compute queue 排程；任何
V2-M4 以後的 milestone。

## 4. 依賴與限制

這份計畫**不會**引入新的第三方依賴——用的都是 `Engine/RHI` 現在已經連結的
Vulkan/D3D12/Metal API，加上已經被 `NEXORA_ENABLE_SLANG` 擋住的 Slang shader 工具鏈（見
`build.shader_contract`、`Tools/Build/ValidateShaderContract.cmake`）。新的 compute shader 原始碼
會放在 `Shaders/` 底下，跟現有的 `Triangle.slang` 放一起，一樣要通過 shader-contract 驗證。這代表
這份計畫落在 CLAUDE.md 要求「先講清楚再做」的「新依賴/改 CI」範疇*之外*——不會踩到那條線。

雲端 session 沒辦法產出 Windows/DX12 跟 macOS/Metal 的 target-host 驗收證據（沒有 MSVC、沒有
Apple 主機）；下面各階段中，Vulkan 的部分可以在這裡實作跟本機驗證，但 DX12/Metal 對應的部分需要
在原生主機上跑過、留下證據，跟 `Window_Presentation_Roadmap.md` 的 WP-M1/WP-M2 在視窗那條線上
已經確立的模式一樣。

## 5. 分階段計畫

### Phase 1a — Vulkan compute dispatch，形狀層級（已完成）

- ✅ 實作了 `VulkanCommandList::Dispatch`（`vkCmdDispatch`），比照現有
  `VulkanCommandList::DrawIndirect` 的做法；在 `VulkanDevice::Submit` 接上
  `DeviceDiagnostics::compute_dispatches` 的彙總，跟 `DrawCalls()`/`IndirectDraws()` 現有的彙總
  方式一致。
- ✅ 新增了 `GPUDrivenPipelineTests.cpp::TestNormalPathOnVulkan`，對一個真正的
  `rhi::CreateDevice(Backend::Vulkan)` device 完整跑 `RecordGPUDrivenExecution`（在 Vulkan 不存在
  時透過 `IsBackendAvailable` 乾淨跳過），斷言 `diagnostics.compute_dispatches == 1 &&
  diagnostics.indirect_draw_calls == 1 && diagnostics.draw_calls == 1` 跟
  `diagnostics.readbacks == 0`，跟同一個檔案裡既有的 validation-backend 斷言完全對應。已驗證：
  `linux-development`（35/35 ctest）跟 `linux-sanitizers`（ASan+UBSan，35/35 ctest）。
- 這一步關掉了這份計畫一開始點出的那個缺口（`Dispatch` 在 Vulkan 上會丟例外），也證明了真正的
  `RecordGPUDrivenExecution` 路徑——不只是無關的 triangle-frame smoke test——確實會在真實的
  Vulkan 硬體/驅動上 dispatch 跟 indirect-draw。
- **這一步還沒證明的事**：`RecordGPUDrivenExecution` 的 `Dispatch`/`DrawIndirect` 呼叫只吃單純的
  數字（`candidate_count`、`indirect_command_count`）——沒有綁定任何 scene 資料、view 參數或輸出
  buffer 到這次 dispatch 上。下面的 compute shader 工作需要真正的、GPU 看得到的輸入/輸出
  buffer，這比這份計畫原本設想的還要大一塊前置工作；見 Phase 1b。

### Phase 1b — 前置需求：RHI 裡的 buffer 資源與 compute-pipeline 建立（尚未開始，需要先確認）

在開始 Phase 1a 的過程中對照原始碼確認：RHI **完全沒有辦法建立、上傳、綁定或讀回 GPU
buffer**，而 `Device::CreatePipeline` **無條件只會建出 graphics pipeline**（寫死的
vertex+fragment stage、`vkCreateGraphicsPipelines`），沒有 compute 路徑。具體來說：

- `Types.h` 已經宣告了 `BufferHandle`/`BufferTag`/`BufferDescriptor`/
  `BindingType::StorageBuffer`——但 `Device` 或 `CommandList` 裡完全沒有任何東西會建立、銷毀、
  綁定或 map 它。這些是沒人用的骨架，很可能就是為了這件事先埋下去、卻從來沒做完。
- `VulkanDevice.cpp` 已經載入了需要的原始 Vulkan function pointer（`vkCreateBuffer`、
  `vkGetBufferMemoryRequirements`、`vkBindBufferMemory`、`vkMapMemory`/`vkUnmapMemory`、
  `vkCreateDescriptorSetLayout`、`vkCreateDescriptorPool`、`vkAllocateDescriptorSets`、
  `vkUpdateDescriptorSets`），但只拿來內部用在一個固定用途的東西上：一個綁在 descriptor set
  0/binding 0、給那個寫死的 triangle pipeline 用的 64-byte uniform buffer。這些都沒有公開暴露，
  也不夠通用到可以給 compute shader 的 scene/view/output 資料綁一個任意的 storage buffer。
- 一個真正的 culling compute shader 至少需要：一個唯讀的 scene object 資料 structured/storage
  buffer、一個裝壓縮後 instance 輸出的 storage buffer、一個裝 indirect command 輸出的 storage
  buffer，加上一個小的 view/frustum 參數 uniform buffer——好幾個不同型別、不同大小的 buffer，
  不是現在這個寫死的單一 uniform binding。

要補上這個缺口，代表要擴充共用的 `rhi::Device`/`rhi::CommandList` 抽象介面
（`Engine/RHI/include/Nexora/RHI/Device.h`），加上 buffer 的建立/銷毀/上傳跟一套
compute-resource-binding 機制，並且在現有的 graphics pipeline 建立路徑旁邊加一條 compute
pipeline 建立路徑。因為這些是加在共用介面上的 pure-virtual 新方法，**每一個** backend
（`ValidationDevice`、`VulkanDevice`、`D3D12Device`、`MetalDevice`）都至少要有個最小實作，build
才能繼續過關，即使現在真正需要能動的只有 Validation 跟 Vulkan。這是真正獨立、基礎性的一塊工作
——不是「寫一個 shader」——正好就是這個 repo 一貫規則要求動手前先討論的那種
RHI-wide 介面變更，即使它沒有引入新的第三方依賴或 CI 變更。**尚未開始；在 Phase 1 真正的
compute shader 工作可以開始之前，需要先確認新 API 的形狀（buffer 生命週期/所有權模型、上傳
路徑——staging buffer 還是 host-visible mapping、binding 模型——固定 slot 還是通用的
descriptor-set builder）。**

### Phase 2 — Vulkan 上剩下的 compute 階段

- Hi-Z occlusion（針對既有 `HiZPyramid` contract 做 conservative test）、visible-instance
  compaction、material/mesh/LOD classification、indirect-command generation，各自寫成 compute
  shader，逐階段加入、每個階段都用同一套 CPU-reference 比對 gate 驗證。
- RenderGraph 整合：確認既有的 queue-ownership barrier tracking（`Engine/Renderer/README.md`
  §RenderGraph）在真正的 Vulkan queue 上（不只是 validation device 的邏輯追蹤）能正確排序這些新的
  compute pass 跟消費它們輸出的 graphics pass。

### Phase 3 — D3D12 backend

- 在 `D3D12Device` 的 command list 上實作 `Dispatch` 跟 `DrawIndirect`（目前完全沒有）：
  `ID3D12GraphicsCommandList::Dispatch`，以及搭配跟 `BuildGPUDrivenCommands()` 已經產生的
  indirect-buffer layout 一致的 command signature 的 `ExecuteIndirect`。
- 把 Phase 1/2 的 compute shader 搬過來（Slang 本來就針對多個 backend，照現有 shader-contract
  驗證的邏輯，確認輸出的 HLSL/DXIL 不需要改 stage semantic）。
- 這個階段實際的執行與 `CompareGPUDrivenResults()` gate 只能在 Windows host 上跑——雲端 session
  碰不到。程式碼跟 shader 可以在這裡寫、在這裡 review，但過不過的證據沒辦法在這裡產生。

### Phase 4 — Metal backend

- 在 `MetalDevice` 上實作 `Dispatch` 跟 `DrawIndirect`（目前完全沒有）：
  `dispatchThreadgroups`/`dispatchThreads` 跟 `drawIndexedPrimitives(indirectBuffer:)`。
- 跟 Phase 3 一樣的 shader 搬遷跟「只有 target-host 才有證據」的限制，這次是 macOS。

### Phase 5 — Target-tier parity 收尾

- 三個 native backend 都能執行同一套 pipeline、在各自的 target host 上通過
  `CompareGPUDrivenResults()` 之後，才勾選 V2-M3 剩下的 gate 項目，並更新
  `跨平台3D_Engine_V2_完整規劃書_v1_4.md` 的 gate checklist 跟進度百分比——在那之前不動。

## 6. 驗證與完成定義

- 每一條新的 native code path 都要通過 `CompareGPUDrivenResults()` 驗證，不是只「編得過、跑不會
  丟例外」就算數。
- Vulkan 的部分（Phase 1-2）在這個 repo 現有的 Linux CI／雲端 gate 上就能完整驗證。
- D3D12（Phase 3）跟 Metal（Phase 4）需要 target-host runner；這份計畫不會用 Linux-only 的證據去
  宣稱它們已驗收，符合這個 repo 一貫「不虛報未跑過的平台覆蓋」的規則。
- CPU reference（`BuildGPUDrivenCommands()`、`CompareGPUDrivenResults()`）的語意不在這份計畫的改動
  範圍內——是 backend 去符合它，不是它為了遷就某個 backend 而改。

## 7. 風險

- Compute shader 的正確性 bug 很容易引入、也很難在沒有 `CompareGPUDrivenResults()` gate 立刻抓到
  的情況下發現；每個階段都應該跟它自己的比對測試一起落地，不要延後補。
- D3D12 的 `ExecuteIndirect` command-signature 設定是很常見的 layout 對不上的 bug 來源；
  indirect-buffer layout 應該在 Phase 1（Vulkan）就定下來一次，之後照搬，不要每個 backend 各自
  重新定義。
- Async/queue-ownership 行為是 portable contract 的「邏輯追蹤」跟真實硬體 queue 最可能出現落差的
  地方；Phase 2 的 RenderGraph 整合步驟就是為了在 D3D12/Metal 的工作疊上一個沒驗證過的假設之前，
  先把這個抓出來。
