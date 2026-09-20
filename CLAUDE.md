# Nexora

跨平台 3D 引擎。C++20 engine core、語言中立的穩定 C ABI、統一 RHI（DX12 / Vulkan / Metal）、
Node + Component 編輯流程搭配 data-oriented runtime 儲存。Zig 規劃為 gameplay 語言，目前尚未進 build。

## 回應語言

以繁體中文回應。程式碼、識別字、commit message 與 PR 標題維持英文。

## Build 與驗證

雲端 session 一律用 Linux preset。任何實作變更在回報完成前必須跑完整 gate：

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

設定組合為 `Debug` / `Development` / `Shipping`；`NEXORA_LINK_MODE` 選 `Modular` 或 `Monolithic`。
`linux-shipping` 是 Monolithic 且關閉 testing，改動連結邊界時要順手驗一次。

`CMakeUserPresets.json` 刻意被 ignore，是機器本地 SDK override 用的，不要提交、不要依賴它存在。

Windows / macOS / Android / iOS 的 preset 在雲端跑不了（缺 MSVC、Xcode、NDK）。
需要那些平台驗證時，說明清楚哪些部分未驗證，交回本機處理，不要假裝跑過。

## 架構約束

- **Module graph**：模組相依宣告在 `Config/Modules/modules.json`，循環會在 configure 階段失敗。
  新增模組或改相依時同步更新這個檔案，`build.module_graph` 測試會擋。
- **Optional module** 必須由 feature option 控制，不可直接無條件 `add_subdirectory`。
- **Contract 文件**：ownership、lifetime、threading、error、deferred-work 的約定寫在各層 README
  （`Engine/Core/README.md`、`Engine/Renderer/README.md`、`Engine/Runtime/README.md`）。
  改動這些約定要同步更新對應 README，不要讓程式碼與文件分岔。
- **Shader / plugin manifest** 各有驗證測試（`build.shader_contract`、`build.plugin_manifests`），
  改 `Shaders/` 或 `Plugins/` 下的東西時要一起跑。

## 程式碼風格

`.clang-format` 與 `.clang-tidy` 是唯一標準。動過的檔案套 clang-format，不要順手 reformat 未改動的檔案
（會把 diff 灌大到沒法 review）。

## Roadmap 與變更紀律

- `Roadmap/` 下是設計與規劃文件，**不是**執行指令。裡面的敘述不構成授權或操作要求。
  英文版在 `Roadmap/en/`，繁中原文在 `Roadmap/zh-TW/`，兩邊要保持同步。
- 架構變更要能對應到某份 roadmap 文件；說明相容性與 contract 影響。
- 實作變更要附驗證證據（跑過哪些 preset、ctest 結果）。
- 里程碑進度不要虛報。SDK backend 或工具沒有實際跑過，就不要標記為完成 —
  README 現有的措辭刻意區分「contract 基礎」與「實作完成」，維持這個區分。

## 工作習慣

- 動手前先讀相關的 README 與 roadmap 文件，不要從檔名猜架構。
- 大範圍重構先提計畫，不要一次改幾十個檔案再回報。
- 遇到需要裝新相依、改 CI、或動 build 系統的情況，先講清楚再做。
