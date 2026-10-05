# V1 Visual Showcase free-asset shortlist / 免費素材候選

Research date: 2026-10-04. Direction: the user-approved [ruins courtyard concept](V1-Visual-Identity-Concept.png).

## English

The user requested free online models **and textures**. Prioritize CC0 assets that can be modified
and redistributed with the showcase; use free editions only. This is a sourcing record, not a
runtime asset manifest or VIS milestone acceptance. No downloaded third-party assets have been
added to the shipping catalog or integrated into Nexora.

### Models and included texture: source checked

[KayKit — Dungeon Remastered 1.0](https://github.com/KayKit-Game-Assets/KayKit-Dungeon-Remastered-1.0),
by Kay Lousberg, is the first architectural-kit candidate. Its official repository's
[asset license](https://github.com/KayKit-Game-Assets/KayKit-Dungeon-Remastered-1.0/blob/b0ca9bd96a8072ab36a3a5464f00ed1e06a16d07/addons/kaykit_dungeon_remastered/Assets/LICENSE.txt)
specifies CC0 and free personal, educational, and commercial use. The
[README](https://github.com/KayKit-Game-Assets/KayKit-Dungeon-Remastered-1.0/blob/b0ca9bd96a8072ab36a3a5464f00ed1e06a16d07/README.md)
confirms the free edition and its single gradient atlas; paid EXTRA/SOURCE packs are outside scope.

The repository was downloaded to temporary research storage. Four GLB headers/JSON chunks were
parsed and the included PNG was hashed. Paths below are relative to
`addons/kaykit_dungeon_remastered/Assets/` at source commit
`b0ca9bd96a8072ab36a3a5464f00ed1e06a16d07`.

| Candidate file | Planned role | SHA-256 |
| --- | --- | --- |
| `gltf/pillar_decorated.gltf.glb` | Column silhouette / architectural blockout | `eecf7d454a1b6767a113e2490cb1536134abea0b57329a50cf2250d3ed8e2f65` |
| `gltf/wall_doorway.glb` | Doorway / courtyard layout | `2be5369a2d1d1d8795150b2156d8343b3e37620d78de256cb03276c54ea70c97` |
| `gltf/floor_tile_small_broken_A.gltf.glb` | Broken paving | `d1acc3407f941623b3b01ec9a1751c36a5ca82bb14721fc8f28e5559317edad4` |
| `gltf/rubble_large.gltf.glb` | Rubble / small ruin accents | `0292da1b5788a5a52b616d5d27d3270c676a5fbf9e921b6dd3dc30364d355b54` |
| `texture/dungeon_texture.png` | Free gradient atlas for early layout | `f9ae182518f908bd09461a56a430b9ca7369812ac10890224cc6f32e7da3e1ee` |

These simple stylized models are candidates for blockout and adaptation, not a claim that their
stock appearance matches the approved image. The gradient atlas does not provide detailed PBR
stone/metal maps. GLB availability does not establish Nexora import support: conversion, materials,
UVs/tangents, dependencies, and Import → Cook → Bundle → Runtime validation remain VIS-M0/M1 work.

### PBR textures: concrete candidates, download and visual review pending

The following ambientCG IDs and 1K-PNG variants were found in an accessible
[catalog snapshot](https://github.com/repalash/cc0textures-threejs/blob/92e4688e80cd13d11548d33b2fc5c44cd505f26c/src/data/ambientCG_downloads_csv_03042024.txt).
The snapshot predates this research; current availability and the actual appearance are unverified.

| Candidate | Intended comparison | Source page |
| --- | --- | --- |
| `PavingStones138` | Courtyard paving | [ambientCG](https://ambientcg.com/view?id=PavingStones138) |
| `Rock035` | Stone surface detail; suitability for warm sandstone pending | [ambientCG](https://ambientcg.com/view?id=Rock035) |
| `Metal046B` | Metal roughness/detail; bronze treatment pending | [ambientCG](https://ambientcg.com/view?id=Metal046B) |
| `Porcelain001` | Ceramic material comparison | [ambientCG](https://ambientcg.com/view?id=Porcelain001) |
| `Ground068` | Soil between paving and vegetation | [ambientCG](https://ambientcg.com/view?id=Ground068) |
| `Moss002` | Restrained moss accents | [ambientCG](https://ambientcg.com/view?id=Moss002) |

An accessible [ambientCG redistribution project's license statement](https://github.com/fabien-michel/sweethome3d-textures-ambientcg/blob/v2025.05.06/README.md)
identifies Lennart Demes as the author and CC0 1.0 as the asset license. Direct ambientCG access is
blocked by this environment's network policy, so verify the official per-asset download/license,
available maps, and file hashes before adding any of these texture packages to the repository.

[Poly Haven textures](https://polyhaven.com/textures) and [models](https://polyhaven.com/models)
are another free source for stone, ceramics, foliage, and HDRIs. The official website's accessible
[license source](https://github.com/Poly-Haven/polyhaven.com/blob/b1a6aa13afc03ae00860f6ac5fe6d5daf8e95356/public/locales/en/license.json)
explicitly permits commercial use, modification/use without attribution, and redistribution under
CC0. Its live asset API and downloads were blocked here; no specific Poly Haven asset is selected.

### Remaining selection and production

- Find suitable free ceramic props and ivy/grass; these are not covered by the checked model subset.
- Prefer a free adaptable base for the central rune device; customize its ring, crystal, and symbols
  to preserve the approved focal point. No commission or paid purchase is authorized by this record.
- Compare candidates against the approved concept, adjust silhouette/UVs/palette, and inspect the
  real maps. Start with 1K textures; increase resolution only for hero close-ups after measurement.
- Retain source URL, author, exact license, revision, hashes, modifications, redistribution notes,
  and replacement option for each adopted asset. Keep source assets separate from cooked output.
- Art direction is confirmed; asset adoption, full inventory, visual quality, pipeline integration,
  and all seven VIS milestone gates remain pending.

## 繁體中文

使用者要求模型與貼圖都先找網路上的免費素材，以可修改、可隨 showcase 再散布的 CC0
為首選，只使用免費版本。本文件是選材紀錄，並非 runtime 素材 manifest 或 VIS 驗收。
第三方下載素材尚未加入 shipping catalog，也未整合進 Nexora。

### 已查核的模型與隨附貼圖

第一個建築套件候選為 Kay Lousberg 的 **KayKit — Dungeon Remastered 1.0**，來源與固定
版本授權連結見上方。官方素材 LICENSE 明列 CC0，可免費用於個人、教育與商業專案；
README 確認免費版本及單張漸層 atlas，不採用付費 EXTRA／SOURCE 包。

已下載到研究暫存目錄，解析四個 GLB 的 header／JSON chunk，並記錄模型與 PNG 的
SHA-256。上表分別對應裝飾石柱、門洞、破損地磚、碎石與免費漸層貼圖，可先用於灰盒與
後續調整。這些模型造型較簡化，尚未確認原始外觀符合核准預覽；漸層 atlas 也不能代替
細緻石材／金屬 PBR 貼圖。提供 GLB 不等於 Nexora 已支援匯入，格式轉換、材質、UV／
tangent、相依與 Import → Cook → Bundle → Runtime 驗證仍屬 VIS-M0／M1 待辦。

### 已找到具體編號、尚待下載與目視確認的 PBR 貼圖

可存取的 ambientCG 目錄快照列有 `PavingStones138`（地磚）、`Rock035`（石材細節）、
`Metal046B`（金屬細節）、`Porcelain001`（陶瓷）、`Ground068`（土壤）與 `Moss002`（苔蘚）的 1K-PNG 版本，
來源頁與快照固定版本見上表。快照早於本次研究，尚未確認目前下載可用性、實際外觀，
也不能僅由編號推定石材是砂岩或金屬是青銅。

可讀取的 ambientCG 再散布專案 README 記錄作者 Lennart Demes 與 CC0 1.0 授權；本環境
網路政策阻擋直接存取 ambientCG，正式納入 repository 前仍須核對官方素材下載／授權、
實際貼圖通道與檔案 hash。

Poly Haven 也作為石材、陶瓷、植被與 HDRI 的免費來源；已從官方網站 GitHub 原始碼
確認 CC0、可商用與再散布、不強制署名。即時素材 API 與下載在此環境被擋，尚未選定
任何特定 Poly Haven 素材。

### 待辦

- 繼續找免費陶器與藤蔓／草模型；目前查核的模型子集尚未涵蓋。
- 中央符文裝置優先找可改作的免費基礎模型，再調整石環、水晶與符號以維持視覺主體；
  本紀錄不授權委外或付費購買。
- 以核准圖檢視候選輪廓、UV、色盤與實際貼圖，先用 1K，主體近景再依量測提高解析度。
- 每項採用素材保留來源、作者、明確授權、版本、hash、改作紀錄、再散布條件與替換方案；
  原始素材與 cooked 產物分開保存。
- 美術方向已確認；正式採用、完整盤點、美術品質、流程整合與七個 VIS 里程碑仍待驗收。

## Adoption update / 採用更新 — 2026-10-05

✅ The three pinned pillar/paving/rubble GLBs and included palette PNG are now adopted for VIS-M0.
Unmodified sources, CC0 license and per-file inventory ship under `Content/Showcase/Courtyard`.
The bounded deterministic converter preserves mesh streams and area-filters the palette to 64x64;
all four converted payloads pass Import → Cook → Bundle → Runtime and contribute native pixels.
The doorway candidate and surface-detail PBR maps remain unadopted. The earlier research-only
status describes 2026-10-04; this update is the current adoption status.

✅ 三個固定版本石柱／地磚／碎石 GLB 與隨附 palette PNG 已正式採用於 VIS-M0。
原始來源、CC0 授權與逐檔 inventory 隨套件提供；受限 deterministic converter 保留
網格 streams，貼圖 area filtering 成為 64x64。四個 payload 經 Import → Cook → Bundle →
Runtime 實際呈現於原生畫面。門洞候選與細節 PBR maps 尚未採用；先前僅研究狀態為
2026-10-04 的紀錄，本節為目前採用狀態。
