# ADR-0004：原生 Showcase 材質與 HDR

日期：2026-10-05。狀態：VIS-M1 實作方向；仍須像素與主機驗收。

## 決策

Renderer／共享 shader policy 保留於 `Shaders/Nexora/Common.slang`；原生 Presentation adapter
擁有 GPU binding、target、barrier 與 fence retirement。Showcase 擁有場景內容，將 Runtime
UUID／version resource 轉為每次 submission 借用的 material slot。Runtime 不接收 native handle
或後端 shading policy，也不將共享 PBR math 複製到 Showcase／backend 字串。

先在現有 Lambert 路徑建立受限的逐批次 opaque base-color／texture 材質，再整合共享 Slang
PBR entry（normal／ORM／emission 與 tangent），接著加入 IBL 資源、linear HDR target 及
tone-map composition pass。各切片獨立驗收；多個 Lambert 色塊本身不代表 VIS-M1 完成。

## Submission 與相容性

`SceneMeshBatch` 追加 material slot，`SceneDrawData` 追加借用 material span；空 material
保留舊有 global light／color／texture 行為。空 batch span 使用 slot zero 畫整個 upload。
記錄 scene command 前驗證全部 range、finite opaque color 與 texture reference。初始上限：
64 材質、4096 batch／instance 及既有 geometry budget。無效 descriptor 不消耗該幀 scene submission。

Runtime identity 維持 UUID／version；draw slot 是暫時索引，不是資產 identity 或 GPU cache key。
應用指定 texture ID 代表 immutable generation，替換使用新 ID；舊 GPU resource 保留至所有
保護它的 frame 完成。Resize／recovery／shutdown 先 drain，再釋放 descriptor 與 target。
公共 C++ layout 要重建所有 consumer；C／Zig wire 與持久化 scene／content schema 保持相容。

## 後續 PBR／色彩設計

Tangent 為 XYZ direction 加 handedness，normal map 使用 tangent-space +Y。ORM：R=occlusion、
G=perceptual roughness、B=metallic。Base／emission 色圖僅做一次 sRGB decode，normal／ORM／
BRDF 資料保持 linear。缺圖使用明確 white／flat-normal／neutral-ORM／black-emission resource。
材質變換與 inverse-transpose normal 保留 exact affine instance 慣例。

IBL 來源及 derived irradiance／prefilter／BRDF LUT 隨 bundle 保存授權、轉換參數與 content hash。
採 tier-1 explicit binding 及受限資源。Native scene 輸出 linear floating-point HDR，exposure／
ACES／bloom 先於單次 display transfer conversion 及 UI composition；保留 legacy scene copy。
HDR target 精度不等於螢幕／swapchain 已協商 HDR10。

Upload／target 由 frame 擁有，texture generation immutable，配置採 transactional staging。
場景／材質改變時，不釋放仍被 submitted command 引用的 resource。共享 validation、獨立材質
原生像素、normal／roughness／orbit、missing-map fallback、resize／reload／lifetime 及後端比較
構成驗收證據；shader 編譯本身不足。硬體畫面與 GTX 960 預算仍為 VIS-M3／M6 獨立 gate。

共享直接光照 PBR 與硬體 sRGB 過濾已接通。IBL 切片加入有界線性 RGBA16F 資源、
Runtime bundle 中的來源／授權／轉換 metadata 相依、七個原生取樣資源及 80-byte 私有材質 packet。
公開 C++ consumer 必須重建；NXAB 與穩定 C／Zig schema 不變。後續 HDR slice 加入可選 RGBA16F frame target 與 GPU 曝光／ACES／顯示轉換，先合成再疊 UI。
外部 graph 使用真實浮點格式與 ShaderRead transition；獨立原生 RHI triangle allocator 仍明確拒絕浮點 target。
bloom 屬於 VIS-M3。
