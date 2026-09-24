# V2-M3 GPU-Driven Rendering — Native Backend 執行計畫

> 版本：v1.0｜狀態：施工中；Phase 1b 已完成，Phase 2 已開始｜更新：2026-09-24｜對應：
> `跨平台3D_Engine_V2_完整規劃書_v1_4.md` §V2-M3

## 1. 目的

V2-M3 的 gate 有四項已勾選（portable command batching、no-readback contract diagnostics、
RenderGraph queue/barrier ownership、CPU-reference correctness comparison），還有一項未勾選：
**native DX12/Vulkan/Metal target-tier parity on target hosts**。這份文件規劃要補上那一項所需的
工作。Phase 1a 與 1b 已完成，Phase 2 也已有 Linux Vulkan compute 證據，但 milestone 仍未完成；其餘階段全部落地並通過各自的
gate 之前，roadmap 進度百分比不會變動。

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

### ✅ Phase 1a — Vulkan compute dispatch，形狀層級（已完成，且已修正）

- ✅ 實作了 `VulkanCommandList::Dispatch`（`vkCmdDispatch`），比照現有
  `VulkanCommandList::DrawIndirect` 的做法；在 `VulkanDevice::Submit` 接上
  `DeviceDiagnostics::compute_dispatches` 的彙總，跟 `DrawCalls()`/`IndirectDraws()` 現有的彙總
  方式一致。
- ✅ 在確認真正驗證有沒有跑起來的過程中，順手抓到並修了一個原本就潛伏在那的真 bug：
  `VulkanDevice::CreateCommandList` 一律拒絕 `QueueType::Compute`（丟出
  `"Vulkan triangle backend only supports graphics queue"`），但底層的 command pool 不管拿到什麼
  queue type 都是建在同一個支援 graphics 的 queue family 上，而這個 family 在這個 backend 鎖定的
  每一種主機上都同時支援 compute。把檢查放寬成接受 `Graphics`/`Compute`，只擋 `Copy`（這個是真的
  不支援——沒有另外選一個 transfer queue family）。
- **修正這份計畫自己先前講錯的一件事**：這個 Phase 早先的版本加了一個測試
  （`TestNormalPathOnVulkan`），完整跑一次 `RecordGPUDrivenExecution` 對真實 Vulkan，並且在引入它
  的 PR 裡宣稱「已經確認它會走真的 Vulkan 路徑……不是被靜默跳過」。那句話是錯的。當時用來檢查的
  本機 build 是 `NEXORA_ENABLE_SLANG` 關掉的狀態（這個 repo 的預設值），在這個狀態下
  `VulkanDevice` 的建構子——它無條件要求 `NEXORA_SLANG_SPIRV_PATH` 這個環境變數，沒設就丟例外——
  永遠會失敗，所以 `IsBackendAvailable(Vulkan)` 永遠回傳 `false`，測試永遠靜默跳過。跳過跟真的
  跑過，exit code 看起來一模一樣，這正是一開始沒發現的原因。已經徹底 root-cause 並修正：把這個
  repo CI 用的、釘住版本的 Slang 工具鏈（`shader-slang/slang` v2026.18）裝進這個 session、用
  `-DNEXORA_ENABLE_SLANG=ON` 重新 configure（對應 `build.yml` desktop job 實際的呼叫方式，這個
  session 單純的 `cmake --preset linux-development` 預設不會這樣），並且把
  `renderer.contracts` 早就有的 `NEXORA_SLANG_SPIRV_PATH`/`NEXORA_REQUIRE_NATIVE_BACKENDS` CTest
  環境屬性也加到 `renderer.v2_gpu_driven` 上（原本只有前者有）。`renderer.contracts` 的耗時從
  接近 0.00 秒變成大約 0.1 秒，一旦真的在跑 Vulkan——這是往後任何「native backend 已驗證」的宣稱
  都值得順手檢查的訊號。
- **真的讓 Vulkan 跑起來之後，`TestNormalPathOnVulkan` 馬上就 segfault 了**——不是優雅地失敗，是
  真的當掉，用 `gdb` 確認是在 Mesa 的 `libvulkan_lvp.so`（這個 sandbox 用的軟體 Vulkan driver）
  裡面，在一個 driver worker thread 上，執行 queue 的時候炸的。根本原因：
  `RecordGPUDrivenExecution` 的 `Dispatch` 呼叫沒有綁任何 compute pipeline——這在它的「只管形狀」
  設計下是對的（見 §2），對 `ValidationDevice` 也無害（它本來就不模擬 pipeline-binding 狀態），
  但 `vkCmdDispatch` 在沒有綁 compute pipeline 的情況下是 Vulkan spec 定義的 undefined
  behavior，而這個 sandbox 沒裝 validation layer，沒辦法把它變成乾淨的錯誤而不是 driver crash。
  因為現在完全沒有辦法建立*任何* compute pipeline（Phase 1b 的範圍），現在沒有辦法安全地做這個
  呼叫。**移除了 `TestNormalPathOnVulkan`**，換成 `TestDispatchPreconditionsOnVulkan`，只測試
  `Dispatch` 自己的 precondition 檢查（例如拒絕 group count 為零）——這些檢查會在記錄任何東西
  之前就丟例外，永遠碰不到 driver。真正的 end-to-end native dispatch 測試要等 Phase 1b 提供可以
  綁的東西才能做。
- 已驗證（這次是真的開了 Slang）：`linux-development`（36/36 ctest，含只有
  `NEXORA_ENABLE_SLANG` 開啟時才存在的 `build.shader_crosscompile`）。也重新驗證了沒開 Slang 的
  一般 `linux-development` preset（這個 repo 的預設值）依然乾淨地降級（35/35，少了那個多出來的
  shader-crosscompile 測試，符合預期）。
- Slang 開著的 `linux-sanitizers` 是 35/36：`renderer.contracts` 在 ASan 下失敗了，一個
  112-byte／2-allocation 的 leak，call stack 整個都在 `libNexoraCore.so` 裡面、在一個
  `core::JobSystem` 的 worker thread 上（`asan_thread_start` → `start_thread`，整條 trace 沒有
  任何 Vulkan/RHI 的 symbol）。已經確認這是既有問題，跟這個 Phase 的改動無關，不是這次工作引入
  的：用 `git stash` 退回前一個 commit、重新 build、重跑，Phase 1a 的 `Dispatch`/
  `CreateCommandList` 改動一個都不在的情況下一樣重現。之前沒被抓到，純粹是因為這是這個 session
  裡第一次 `renderer.contracts` 同時在 ASan 底下跑、又真的有在跑真實 Vulkan（這個 session 之前每
  一次 sanitizer run 都是 Slang 關著的）。這裡先不修——是 `core::JobSystem` 的 thread-lifecycle
  問題，跟 V2-M3 的 RHI/Renderer 範圍是不同的子系統——但先在這裡記下來，不悶著不講，因為這是一個
  真的、可重現的 ASan 發現，之後應該有人接手處理。
- **這次講精確一點，Phase 1a 真正確立的東西**：`Dispatch` 已經實作，它的 precondition 檢查已經
  用真實 Vulkan 驗證過；`CreateCommandList(Compute)` 現在能動了；`renderer.contracts` 那個
  triangle-frame 測試，只要 Slang 開著，本來就真的有在真實 Vulkan 上驗證 `DrawIndirect`（roadmap
  原本這部分的宣稱站得住腳）。`RecordGPUDrivenExecution` 真正的 culling/compute 輸出，完全還沒
  在真實硬體上驗證過——那完全是 Phase 1b + Phase 2 的工作。

### ✅ Phase 1b — RHI buffer 資源與 compute-pipeline 建立（已完成）

> **更新（2026-09-25）**：下面這幾段描述的是開始 Phase 1a 時找到的缺口。在那之後，另一條並行的
> 工作（`Editor_ImGui_Integration_Plan.md`，由另一個 agent session 推進）已經把真正的
> `Device::CreateBuffer`/`WriteBuffer`/`DestroyBuffer`、
> `CommandList::BindVertexBuffer`/`BindIndexBuffer`/`BindTexture`、`DrawIndexed`、
> `SetScissor`，跟一套正常的 submission timeline（`Submit` 回傳一個 completion value；
> `CompletedSubmissionValue`/`WaitForSubmission`）加進了
> `Engine/RHI/include/Nexora/RHI/Device.h`——已經在這次更新時對照 `main` 確認過真的在那裡。這關掉
> 了這個缺口「完全沒辦法建立/上傳/銷毀 buffer」的那一半。但**沒有**關掉 compute 專屬的那一半：
> `PipelineDescriptor` 沒變（依然沒有 compute 路徑，`VulkanDevice::CreatePipeline` 依然寫死
> `vkCreateGraphicsPipelines`），也依然沒有辦法把一個 buffer 綁成 compute shader 的 storage
> 資源（現在只有 vertex/index/texture binding，全部都是 graphics 導向——不意外，畢竟那份工作的
> 目的是 ImGui 渲染，不是 compute culling）。所以 Phase 1b 現在剩下的範圍變窄了、也變得比較實際：
> compute pipeline 建立跟 compute 資源（descriptor-set）綁定，重用新加的 buffer
> 建立/上傳/銷毀原語，不用重新發明一套。這一節剩下的內容維持原樣，留作當時實際驗證過的紀錄，不
> 事後改寫成看起來像是猜對了的樣子。

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
RHI-wide 介面變更，即使它沒有引入新的第三方依賴或 CI 變更。**已用 backend-neutral compute pipeline kind、四個固定 storage-buffer slot、host-visible upload 與明確標為 test-only 的 bounded readback seam 完成。固定 slot 讓本階段保持狹窄；通用 descriptor builder 仍是後續工作。**

### Phase 2 — Vulkan 上剩下的 compute 階段

> **更新（2026-09-24）：施工中。** Linux Vulkan 現在會建立真正的 compute pipeline，透過四個
> backend-neutral slot 綁定 candidate／visible／indirect／statistics storage buffer，dispatch
> `GPUDriven.slang`、等待 native completion，並比對 test-only readback 結果。正常路徑仍然
> readback-free，diagnostics 也會分開計算 dispatch 與驗收 readback。這是 native pipeline／
> binding／dispatch foundation 的驗收證據，尚不是下方完整 frustum／Hi-Z／LOD／sorted-bin
> 演算法的驗收。

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
