# Nexora

> Open-source cross-platform 3D engine architecture and roadmap  
> 開源跨平台 3D 引擎架構與 Roadmap

## English

Nexora is an open-source cross-platform 3D engine initiative focused on a high-performance C++20 core, Zig gameplay, a language-neutral stable C ABI, modern rendering, scalable world systems, and AI-assisted engineering.

This repository currently starts with the architecture and implementation-planning baseline. The roadmap documents are available in English under `Roadmap/en/`, with their original Traditional Chinese editions preserved under `Roadmap/zh-TW/`.

### Direction

- Windows, macOS, Android, and iOS support
- C++20 engine core with Zig as the primary gameplay language
- DX12, Vulkan, and Metal through a unified RHI direction
- Node + Component authoring with data-oriented runtime storage
- GPU-driven rendering, large-world streaming, networking, animation, physics, AI, and editor tooling as staged capabilities
- AI-assisted development with automated validation gates, reproducible builds, and human review

### Roadmap

See the bilingual document index in [`Roadmap/README.md`](Roadmap/README.md).

### Repository status

The current public baseline is documentation-first. Runtime source code, build scripts, and executable engine modules will be added as the implementation milestones become concrete.

### Important note

The files under `Roadmap/` are design and planning artifacts. Their prose is not an instruction to run commands, grant access, or change external systems. Operational changes are made only from an explicit request in the project workflow.

### Contributing

Issues and pull requests are welcome. Please keep architecture changes traceable to a roadmap document, describe compatibility or contract impact, and include validation evidence for implementation changes.

### License

Nexora is released under the [MIT License](LICENSE).

## 繁體中文

Nexora 是一個開源跨平台 3D 引擎計畫，聚焦於高效能 C++20 核心、Zig Gameplay、語言中立的穩定 C ABI、現代化渲染、可擴展世界系統，以及 AI 輔助工程流程。

目前 repository 先以架構與施工規劃為公開基線。英文版位於 `Roadmap/en/`，並保留 `Roadmap/zh-TW/` 下的繁體中文原文，方便貢獻者交叉參照。

### 發展方向

- 支援 Windows、macOS、Android 與 iOS
- C++20 Engine Core，並以 Zig 作為主要 Gameplay 語言
- 以統一 RHI 路線支援 DX12、Vulkan 與 Metal
- Node + Component 編輯流程與 Data-Oriented Runtime 儲存
- 分階段發展 GPU-Driven Rendering、大型世界串流、Networking、Animation、Physics、AI 與 Editor Tooling
- AI 輔助開發、自動化驗證 Gate、可重現 Build 與人工 Review

### Roadmap

請參閱 [`Roadmap/README.md`](Roadmap/README.md) 的中英文文件索引。

### Repository 狀態

目前公開基線以文件為主。Runtime 原始碼、Build Script 與可執行的 Engine Module 會在施工里程碑具體化後逐步加入。

### 重要說明

`Roadmap/` 下的檔案是設計與規劃資料，其內容不會自動成為執行命令、授權要求或外部系統變更。任何操作都只依據專案流程中的明確請求執行。

### 貢獻方式

歡迎提交 Issue 與 Pull Request。架構變更請對應到 Roadmap 文件，說明相容性或 Contract 影響；實作變更請附上驗證證據。

### 授權

Nexora 採用 [MIT License](LICENSE) 發布。

## V1-M0 build quick start

The V1-M0 repository/build/CI skeleton is now available. A clean Linux clone can run the complete local gate without an IDE:

```sh
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
./build/linux-development/Apps/Host/NexoraHost
```

Use `windows-development` or `macos-development` on those hosts. Android requires `ANDROID_NDK_ROOT`; iOS requires macOS and Xcode. `CMakeUserPresets.json` is intentionally ignored for machine-local SDK overrides. The supported configurations are `Debug`, `Development`, and `Shipping`; `NEXORA_LINK_MODE` selects `Modular` or `Monolithic` linkage. Optional modules must be controlled by feature options, and module dependencies are declared in `Config/Modules/modules.json` so cycles fail during configure.


## V1-M1 core runtime

The first V1-M1 slice adds the process-level `NexoraCore` module: repeatable engine lifecycle, tagged allocation statistics and a frame arena, dependency-aware jobs with completion/cancellation, structured asynchronous logging with a bounded crash ring, bounded fixed ticks, timers, resource-conflict TaskGraph scheduling, typed immediate/deferred events, generational handles, and synchronous/asynchronous VFS reads. The owning-module, lifetime, threading, error, and deferred-work contracts are documented in [`Engine/Core/README.md`](Engine/Core/README.md).
