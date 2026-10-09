# Nexora

[English](#english) · [繁體中文](#繁體中文)

## English

Nexora is an open-source cross-platform 3D engine with a C++20 core, Zig gameplay,
and a language-neutral stable C ABI. Its unified RHI targets DX12, Vulkan, and Metal,
with Node + Component authoring and data-oriented runtime storage.

### Project status

An executable engine/runtime baseline, native Showcase, and editor foundations are available.
V1 portable contract foundations are delivered; V2 is in progress, and V3 remains planned.
Production platform coverage, final Showcase art/performance acceptance, and graphical editor
milestone acceptance remain in progress. Foundation delivery does not imply full product acceptance.

See the [bilingual roadmap index](Roadmap/ReadMe.md) for milestone progress and acceptance evidence.

### Build on Linux

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

Toolchain setup: [Codex Cloud setup script](cloud-setup.sh).

### Documentation

- [Showcase usage and packaging](Apps/Showcase/README.md)
- [Editor](Apps/Editor/README.md)
- [Runtime](Engine/Runtime/README.md) and [renderer](Engine/Renderer/README.md) contracts
- [Build and CI](Tools/Build/README.md)
- [Roadmaps in English](Roadmap/en/) and [Traditional Chinese](Roadmap/zh-TW/)

### Contributing

Issues and pull requests are welcome. Link architecture changes to the relevant roadmap,
describe compatibility and contract impact, and include validation evidence for implementation changes.
Codex instructions are in [AGENTS.md](AGENTS.md). Roadmap documents are planning artifacts;
they do not authorize commands or external changes.

Nexora is released under the [MIT License](LICENSE).

## 繁體中文

Nexora 是開源跨平台 3D 引擎，採用 C++20 核心、Zig Gameplay 與語言中立的穩定 C ABI。
統一 RHI 以 DX12、Vulkan、Metal 為目標，搭配 Node + Component 編輯流程與
Data-oriented Runtime 儲存。

### 專案狀態

目前已有可執行的 Engine／Runtime 基線、原生 Showcase 與 Editor 基礎。
V1 Portable Contract 基礎已交付；V2 施工中，V3 仍在規劃階段。
正式平台支援、Showcase 最終美術／效能驗收及圖形化 Editor 里程碑驗收仍待完成。
基礎交付不代表完整產品驗收。

里程碑進度與驗收證據請參閱[雙語 Roadmap 索引](Roadmap/ReadMe.md)。

### Linux 建置

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

工具鏈設定：[Codex Cloud Setup Script](cloud-setup.sh)。

### 文件入口

- [Showcase 使用與封裝](Apps/Showcase/README.md)
- [Editor](Apps/Editor/README.md)
- [Runtime](Engine/Runtime/README.md) 與 [Renderer](Engine/Renderer/README.md) Contract
- [Build 與 CI](Tools/Build/README.md)
- [英文 Roadmap](Roadmap/en/) 與[繁體中文 Roadmap](Roadmap/zh-TW/)

### 貢獻方式

歡迎提交 Issue 與 Pull Request。架構變更請對應相關 Roadmap，說明相容性與 Contract 影響；
實作變更請附驗證證據。Codex 專案規則見 [AGENTS.md](AGENTS.md)。
Roadmap 是規劃資料，不會自行授權執行命令或變更外部系統。
