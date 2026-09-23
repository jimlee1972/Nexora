# Nexora

> Open-source cross-platform 3D engine architecture and roadmap  
> 開源跨平台 3D 引擎架構與 Roadmap

## English

Nexora is an open-source cross-platform 3D engine initiative focused on a high-performance C++20 core, Zig gameplay, a language-neutral stable C ABI, modern rendering, scalable world systems, and AI-assisted engineering.

This repository contains an executable C++20 engine/runtime baseline in addition to its architecture and implementation plans. The roadmap documents are available in English under `Roadmap/en/`, with their original Traditional Chinese editions preserved under `Roadmap/zh-TW/`.

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

The repository now builds and tests Foundation, Core, RHI, Renderer, Runtime, API samples, a Zig gameplay consumer, and a headless `NexoraShowcase`. The milestone sections below describe the implemented portable contract foundations and explicitly call out platform or production backends that remain future work.

#### Engine API status

The Engine API is **not fully complete** against the definition of done in the [Engine API Foundation Roadmap](Roadmap/en/Engine_API_Foundation_Roadmap.md). The public README therefore does not mark the whole API as complete.

| Track | Status | Available now / remaining gate |
| --- | --- | --- |
| API-M1 Math and geometry | In progress | Core math, geometry, transforms, layout tests, and an executable sample are available; broader SIMD/ARM coverage and external coordinate golden tests remain. |
| API-M2 Foundation data types | Complete for the roadmap scope | UTF-8 strings/views, buffers/spans, UUIDs, names, results, parsing, generational handles, and caller-owned or opaque engine-owned C ABI buffers are implemented and tested. |
| API-M3 VFS and file I/O | Complete for the roadmap scope | Directory, memory, read-only package, and bundle backends; streams, ranged/async reads, mapping, watches, atomic writes, Shipping host-mount restrictions, and >4 GiB sparse-offset gates are implemented. |
| API-M4 Engine services | Complete for the roadmap scope | Monotonic/game/fixed time, versioned deterministic random, configuration, logging, jobs, events, and profiling-marker emission are implemented and tested. |
| API-M5 World/game facade | In progress | Handle/value-based entity, scene, transform, camera, light, mesh-renderer, asset-reference, and input access are available; physics, character, and audio entity integration remains. |
| API-M6 Bindings and versioning | In progress | Versioned C ABI host tables and real `GameWorld` component wire paths are available; event subscription, tick control, and the remaining conformance/compatibility gates remain. |

#### Zig gameplay and Showcase status

Zig is no longer only a planned language direction. The repository builds a Zig 0.14.0 gameplay object and ABI smoke consumer. The current headless/static ZS-M1 verification slice has C++ own `main`, engine/world lifetime, fixed and variable updates, offscreen rendering, transactional reload, and shutdown, while Zig mutates a live entity Transform through the public V3 ABI. See the [Showcase README](Apps/Showcase/README.md) for the supported workflow.

This is **not the completed Zig Showcase roadmap**: dynamic module discovery, native window/swapchain presentation, the API-driven gallery rooms, broader failure coverage, and packaged Development/Shipping distribution (ZS-M2 through ZS-M5) remain open. The headless report labels native presentation `CONTRACT ONLY` rather than presenting it as implemented.

### Important note

The files under `Roadmap/` are design and planning artifacts. Their prose is not an instruction to run commands, grant access, or change external systems. Operational changes are made only from an explicit request in the project workflow.

### Contributing

Issues and pull requests are welcome. Please keep architecture changes traceable to a roadmap document, describe compatibility or contract impact, and include validation evidence for implementation changes.

### Codex Cloud

Use [`cloud-setup.sh`](cloud-setup.sh) as the Codex Cloud environment setup script. It installs the
Linux toolchain, CMake, and Zig version used by this repository, then warms the development preset.
Repository-specific agent instructions, validation gates, and commit/PR conventions are defined in
[`AGENTS.md`](AGENTS.md).

### License

Nexora is released under the [MIT License](LICENSE).

## 繁體中文

Nexora 是一個開源跨平台 3D 引擎計畫，聚焦於高效能 C++20 核心、Zig Gameplay、語言中立的穩定 C ABI、現代化渲染、可擴展世界系統，以及 AI 輔助工程流程。

目前 repository 除了架構與施工規劃，也已包含可執行的 C++20 Engine／Runtime 基線。英文版 Roadmap 位於 `Roadmap/en/`，並保留 `Roadmap/zh-TW/` 下的繁體中文原文，方便貢獻者交叉參照。

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

目前已可建置及測試 Foundation、Core、RHI、Renderer、Runtime、API sample、Zig gameplay consumer 與 headless `NexoraShowcase`。下方里程碑章節會列出已實作的 portable contract foundation，並明確標示仍待完成的平台或 production backend。

#### Engine API 狀態

依照 [Engine API 基礎 Roadmap](Roadmap/zh-TW/Engine_API_基礎_Roadmap.md) 的 Definition of Done，Engine API **尚未全部完成**，因此本 README 不會把整體 API 標成完成。

| Track | 狀態 | 現有能力／剩餘 Gate |
| --- | --- | --- |
| API-M1 Math 與幾何 | 施工中 | 已有核心 math、geometry、transform、layout tests 與可執行 sample；仍缺更廣的 SIMD／ARM coverage 與外部座標 golden tests。 |
| API-M2 基礎資料型別 | Roadmap scope 已完成 | UTF-8 string/view、buffer/span、UUID、name、result、parsing、generational handle，以及 caller-owned 或 opaque engine-owned C ABI buffer 均已實作及測試。 |
| API-M3 VFS 與檔案 I/O | Roadmap scope 已完成 | 已實作 directory、memory、唯讀 package 與 bundle backend、stream、range/async read、mapping、watch、atomic write、Shipping host-mount 限制及 >4 GiB sparse-offset gate。 |
| API-M4 Engine services | Roadmap scope 已完成 | Monotonic/game/fixed time、含版本的 deterministic random、configuration、logging、jobs、events 與 profiling-marker emission 均已有實作及測試。 |
| API-M5 World/game facade | 施工中 | 已有 handle/value-based entity、scene、transform、camera、light、mesh-renderer、asset reference 與 input access；仍缺 physics、character 與 audio 的 entity integration。 |
| API-M6 Bindings 與版本化 | 施工中 | 已有 versioned C ABI host table 與連到真實 `GameWorld` 的 component wire path；仍缺 event subscription、tick control，以及其餘 conformance／compatibility gates。 |

#### Zig Gameplay 與 Showcase 狀態

Zig 已不只是規劃中的語言方向。Repository 會建置 Zig 0.14.0 gameplay object 與 ABI smoke consumer；目前 headless/static ZS-M1 verification slice 由 C++ 擁有 `main`、Engine／World lifetime、fixed 與 variable update、offscreen rendering、transactional reload 及 shutdown，Zig 則透過公開 V3 ABI 修改真實 entity 的 Transform。支援的操作流程請參閱 [Showcase README](Apps/Showcase/README.md)。

這**不代表 Zig Showcase Roadmap 已全部完成**：dynamic module discovery、native window/swapchain presentation、API-driven gallery rooms、更完整的 failure coverage，以及 Development／Shipping packaged distribution（ZS-M2～ZS-M5）仍未完成。Headless report 會把 native presentation 標成 `CONTRACT ONLY`，不會冒充已實作。

### 重要說明

`Roadmap/` 下的檔案是設計與規劃資料，其內容不會自動成為執行命令、授權要求或外部系統變更。任何操作都只依據專案流程中的明確請求執行。

### 貢獻方式

歡迎提交 Issue 與 Pull Request。架構變更請對應到 Roadmap 文件，說明相容性或 Contract 影響；實作變更請附上驗證證據。

### Codex Cloud

請將 [`cloud-setup.sh`](cloud-setup.sh) 設為 Codex Cloud environment 的 setup script；它會安裝本專案使用的
Linux toolchain、CMake 與 Zig 版本，並預先 configure development preset。Codex 的專案指示、驗證 gate，
以及 commit／PR 規範定義於 [`AGENTS.md`](AGENTS.md)。

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

Use `windows-development` or `macos-development` on those hosts. Android requires `ANDROID_NDK_ROOT`; iOS requires macOS and Xcode. CI runs a `cmake --preset` configure smoke for both `android-development` and `ios-development` on every push, plus a Zig gameplay object cross-compile for `aarch64-linux-android`/`aarch64-ios`; a full on-device build and hardware RHI backends for those two platforms remain future work. `CMakeUserPresets.json` is intentionally ignored for machine-local SDK overrides. The supported configurations are `Debug`, `Development`, and `Shipping`; `NEXORA_LINK_MODE` selects `Modular` or `Monolithic` linkage. Optional modules must be controlled by feature options, and module dependencies are declared in `Config/Modules/modules.json` so cycles fail during configure. `-DNEXORA_ENABLE_MIMALLOC=ON` fetches mimalloc via CMake `FetchContent` and switches `TrackingAllocator`'s backend to it; off by default, and CI builds and tests it on Linux, Windows, and macOS. `nexora::foundation::GetBuildInfo()` reports the engine version, ABI version, build configuration, and link mode; `GetBuildId()` separately reports a git-derived build ID so the existing by-value `BuildInfo` layout stays stable across the Modular link boundary.

### VS Code workflow (Windows / macOS / Linux)

VS Code with the [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension is the primary way to develop on Windows and macOS day to day (Linux works the same way). Opening `Engine.code-workspace` prompts to install the recommended extensions in `.vscode/extensions.json`, then CMake Tools reads `CMakePresets.json` directly (`cmake.useCMakePresets: "always"`) and only offers the presets whose `condition` matches the current OS — you never type a preset name by hand.

- **Configure**: happens automatically on open (`cmake.configureOnOpen`), or via the CMake Tools status bar / Command Palette.
- **Build / Test**: the `CMake: build` and `CMake: test` tasks in `.vscode/tasks.json` run against whichever preset CMake Tools currently has selected — the same tasks work unmodified on every platform.
- **Debug (F5)**: `.vscode/launch.json` has two configurations, `Nexora Host (Windows)` (uses the `cppvsdbg` debugger) and `Nexora Host (macOS/Linux)` (uses CodeLLDB, from the recommended `vadimcn.vscode-lldb` extension) — pick the one matching your OS from the Run and Debug dropdown once, then F5 builds and launches `NexoraHost` under the debugger via `cmake.launchTargetPath`.

Under the hood, CMake Tools drives every preset here with Ninja (declared once in the `base` preset) — that's an implementation detail the extension handles for you, not something you configure by hand. The `cmake --preset ...` / `cmake --build --preset ...` / `ctest --preset ...` commands above are exactly what CI runs and remain the terminal/scripting-friendly equivalent of the same workflow.


## V1-M1 core runtime

The first V1-M1 slice adds the process-level `NexoraCore` module: repeatable engine lifecycle, tagged allocation statistics and a frame arena, dependency-aware jobs with completion/cancellation, structured asynchronous logging with a bounded crash ring, bounded fixed ticks, timers, resource-conflict TaskGraph scheduling, typed immediate/deferred events, generational handles, and synchronous/asynchronous VFS reads. The owning-module, lifetime, threading, error, and deferred-work contracts are documented in [`Engine/Core/README.md`](Engine/Core/README.md).


## V1-M2 / V1-M3 rendering progress

The rendering foundation now includes a backend-neutral RHI, an optional Slang cross-compilation
and canonical-reflection gate, canonical shader fixtures, a strict validation device, asynchronous
pipeline caching, private DX12/Vulkan/Metal device implementations, and an executable
`Offscreen -> Main -> Present` RenderGraph workload. The V1-M3 gate runs the same workload through
the platform-native backend in CI; Slang supplies the DXIL/SPIR-V/MSL artifacts required by the
native shader paths. The native backends currently target the offscreen RHI contract; window-system swapchains
remain outside this milestone. Current scope and ownership are documented in
[`Engine/RHI/README.md`](Engine/RHI/README.md).
and [`Engine/Renderer/README.md`](Engine/Renderer/README.md).

## V1-M4 scene vertical slice

V1-M4 has an executable data-to-render vertical slice with versioned deterministic scene
snapshots, isolated editor/play worlds, dependency-ordered systems, deferred structural commands,
additive/persistent scene lifecycle, component extraction, and RenderGraph presentation.

## V1-M5 asset, cooker, and bundle pipeline

V1-M5 adds a hash-validated asset/cooker/bundle pipeline: UUID-addressed source/canonical/runtime
assets, a derived-data cache, dependency-cycle rejection before staging, reference-counted
generation pinning, residency accounting, and rollback to the prior good generation on any import,
cook, or verification failure.

## V1-M6 reflection, plugin host, scene editor, and prefab foundation

V1-M6 adds reflection metadata, a real cross-platform plugin loader (`dlopen`/`LoadLibrary`, not an
in-process comparison) that gates on a stable C ABI symbol and rejects a mismatch before any other
use -- including an optional second entry point a plugin exports to register services back into the
engine over a plain C callback -- a scene editor built on Create/Modify/Undo, and prefab
override/rebase with nested-prefab composition. It does not include a graphical Hierarchy/Scene
View/Game View/Inspector/Gizmo editor, which needs a windowing/rendering front end this repository
does not have yet.

## V1-M7 input, UI, and localization runtime

V1-M7 adds a device-neutral input/UI runtime: multi-device input routing with duplicate-sequence
rejection and stable pointer IDs, capture/target/bubble UI dispatch with pointer capture,
virtualized list windowing, and locale-resolution with a configurable fallback locale (checked
before falling back to the raw key). Unlike M8-M12 it has no feature-strip switch and always
compiles into `NexoraRuntime`.

## V1-M8 gameplay simulation

V1-M8 adds a backend-neutral Physics -> Character -> Navigation -> AI boundary: batched physics
queries, ground/wall/step resolution and teleport semantics, streamed navigation tiles with
stale-path invalidation, and a typed blackboard/behavior/perception AI foundation.
`CharacterController`/`StandardCharacterMotor` also honor a caller-supplied ground/destination
readiness flag, holding position and reporting `CharacterGroundState::StreamingPending` instead of
free-falling through geometry that has not streamed in yet. Automatically wiring that flag to
`LargeWorld`'s streaming manager remains gameplay/application-layer integration work, not part of
this foundation.

## V1-M9 presentation runtime

V1-M9 adds skeleton/clip animation blending with root motion extraction and a GPU skinning palette,
a voice-limited audio engine with bus routing and residency accounting, a struct-of-arrays particle
system, and a non-blocking timestamped video player with subtitle and seek support. Device-level
backends (a MiniAudio/platform audio adapter, hardware video decode, GPU skinning upload) remain
future work.

## V1-M10 large-world runtime

V1-M10 adds stable fixed-grid addressing, spatial lookup, separate cell/full-bundle/HLOD-bundle
streaming residency with occupied-cell pinning and portal prefetch, offline HLOD, clipmap terrain,
and instanced vegetation, all synchronous and caller-owned.

## V1-M11 mobile platform and native WebView runtime

V1-M11 adds an SDK-neutral app-lifecycle, permission, safe-area, and thermal/memory pressure-policy
contract, plus a native WebView host with deterministic handle destruction and validated pointer
routing. Android/iOS platform SDK adapters (WebView2, WKWebView, Android WebView) and physical
device execution remain platform-SDK gates outside this portable baseline.

## V1-M12 shipping, packaging, and hardening

V1-M12 adds a profile-driven packaging contract with independent plugin/shader/asset stripping,
staged bundle update with confirm/rollback, crash-report evidence capture, soak-growth monitoring,
and a four-platform device-evidence matrix. Signed installers, store update transports, and
physical multi-hour device soak runs remain release-infrastructure gates.

## Later runtime contracts

M7 through M12 above are executable contract foundations on this portable Linux baseline, not
claims that third-party SDK backends or production authoring tools are complete. See
[`Engine/Runtime/README.md`](Engine/Runtime/README.md) for the full milestone matrix, per-feature
strip flags, and exact test coverage.
