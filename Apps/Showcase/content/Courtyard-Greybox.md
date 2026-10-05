# Courtyard engineering blockout / 庭院工程灰盒

Date: 2026-10-05. Scope: first implementation slice of VIS-M0, not finished art.

| Element | Source / author | License / redistribution | Production / replacement |
| --- | --- | --- | --- |
| Paving, columns, broken rear arch, side walls, pedestal | Nexora Showcase procedural geometry; repository contributors | Repository LICENSE; ships as source-generated geometry | Cube/segment blockout; replace with adapted free architectural models |
| Central ring | Original 20-segment procedural tube | Repository LICENSE | Engineering silhouette; final carved stone/bronze treatment pending |
| Crystal placeholder | Original `nexora.showcase.mesh.v1` mesh from Runtime asset generation | Repository LICENSE | Existing Import → Cook → Bundle → Runtime payload; report retains content hash; replace with faceted hero crystal |
| Ceramic / vegetation placeholders | Original capsule and tube geometry | Repository LICENSE | Static spatial markers; final ceramic models, foliage maps and wind pending |
| Third-party final models/textures | Free-asset shortlist in `Roadmap/art/Free-Asset-Sourcing.md` | Three architectural meshes and one palette atlas now adopted; see `Content/Showcase/Courtyard/assets.json` | Download, license/hash inventory, conversion and art review pending |

No purchased or commissioned content. Engineering geometry has no separate source download or
external dependencies. Production cost is implementation work; no art-production time estimate
is claimed. Original placeholders remain distinct from the adopted KayKit architecture; ambientCG surface-detail maps remain pending.

Fixed camera order: wide (0), material close-up (1), motion finale framing (2). `B` restores each
camera deterministically; `9` re-enters at wide. `F4` hides all UI. All shots target the same central
device; the close-up and finale prepare future material/motion integration and do not demonstrate
those features yet. Baselines retain backend/build provenance and software-rasterizer limitations.

本切片僅提供原創程序化工程灰盒、固定鏡頭與既有資產流程代表性網格，不宣稱最終美術、
PBR、真實陰影、發光或風動完成。第三方免費素材尚未正式採用；模型／貼圖的下載、授權、
hash、格式轉換及目視確認仍待完成。沒有採購、委外或已確認的美術工期。

## Adopted VIS-M0 assets

The decorated columns, broken paving and rubble now use the retained CC0 KayKit sources; mesh
conversion preserves source normals/UVs, native placement/axis scales adapt the compact layout.
A 64x64 area-filtered version of the included palette atlas is loaded from the same cooked bundle.
Original source hashes and license are under `Content/Showcase/Courtyard`; conversion is verified
by CTest. Final surface-detail maps, hero crystal/runes and moving foliage remain later milestones.

已正式採用三個 CC0 KayKit 建築網格與隨附漸層貼圖，原始來源、授權、hash 與盤點隨套件
提供。網格保留 normal／UV，依庭院配置縮放與擺放；貼圖經 area filtering 成為 64x64
基線 palette，並透過匯入／cook／bundle／Runtime 載入。正式 PBR 細節與動態植被仍待後續。

## Hero authoring update / 主體美術更新 — 2026-10-05

The cube/capsule/fully-bronze-ring descriptions above preserve the original baseline. Current
native art uses an original Runtime-loaded faceted crystal, stone wedge ring with bronze fittings
and emissive diamond runes, round stepped plinths, hollow lathed vessels, sky geometry and six
original 64² detail maps. Source, converter, repository license and derived hashes accompany the
hero payloads. Existing downloaded CC0 architecture remains separately attributed. The vegetation
markers are still static pending VIS-M4; physical final-art approval remains open.

上方保留最初灰盒基線；目前原生美術改為 Runtime 載入的原創多面晶體、石材楔形環、青銅扣件、
發光菱形符文、圓形階梯台、空心旋轉陶器、天空幾何及六張原創 64² 細節貼圖。來源、converter、
repository 授權與 derived hash 隨 hero payload 提供，既有 CC0 建築另列出處。植被仍是静態標記，
等待 VIS-M4；目標硬體最終美術審查仍未完成。
