# ADR-0006：Runtime 擁有的 cooked static project

狀態：接受有界的 StaticView 前置功能；完整 ED-M6 仍待完成。

## 背景

Portable build frontend 只發布 caller 提供的 artifact manifest，尚未 cook 實際 Editor
專案，也沒有任意專案 geometry／material 的 consumer。Showcase package 無法驗證此流程。
Shipping 必須透過 public Runtime data 消費內容，不得依賴 Editor。

## 決策

在 asset-pipeline feature 下定義 owning schema-1 mesh、scalar PBR、scene codec、有界
StaticView package，以及可選且只依賴 Runtime 的 `NexoraProjectPlayer --verify-package`。
重用 AssetCooker／NXAB 與 BundleBuilder integrity validation。保留完整 asset UUID、原有
mesh resource derivation、World snapshot version 3、legacy shader ID 與 opaque bytes。
拒絕 resource collision、未知 material version、未解析 dependency，以及未明確綁定 scalar
material 的非零 legacy shader。其他 opaque component 保留但不執行。

Codec field 明確使用 little-endian；現有 NXAB 使 package 只支援 little-endian host。
FNV checksum 偵測損壞，不提供 authenticity。不改 stable C／Zig ABI；public C++ consumer
必須重編譯。上限與 ownership 詳列於
[codec](../../Engine/Runtime/CookedSceneAssets.md) 與
[package](../../Engine/Runtime/ProjectPackage.md) contract。Hierarchy 透過索引與迭代計算
包含 mirror／shear 的精確 matrix，不為每個 binding 遞迴走訪 ancestor。

## 影響與驗收

Editor 現提供有界 owning Runtime capture，以及從 capture 與明確提供的 owning imported
OBJ／scalar PBR asset 產生封裝的純 `CookStaticProject` producer。Exact closure 重用共用
codec／cooker／package validation，不查找 source 或執行 publication；詳見
[producer contract](../../Engine/Editor/StaticProjectExport.md)。拒絕完整 UUID／resource collision、
未知 reserved binding、缺失 asset，以及沒有 scalar override 的非零 legacy shader。
Scene text 與 inactive opaque bytes 保持原樣。

實際 CLI 可在移除 source content 後驗證 owning package，明確回報 StaticView 與 inactive
component。Verification 不開啟原生視窗。可選 native StaticView 現對 public NativePBR geometry／
material／affine instance 執行有界 admission，選取 authored camera／light，並完成真實
draw／present／resize／drain，不依賴 Editor 或 source content。Gameplay loading、compile、deploy
與 signing 仍未接上；current-state／stale／cancel 檢查、atomic publication 及 Build／deploy／log UI
是後續工作。

所有完整 ED milestone 保持未勾選。交付須通過完整 Linux Development、Monolithic Shipping、
optional-feature stripping 與實際 CLI consumption；命令與結果寫入交付證據。
