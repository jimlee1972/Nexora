# Courtyard engineering blockout / 庭院工程灰盒

Date: 2026-10-05. Scope: first implementation slice of VIS-M0, not finished art.

| Element | Source / author | License / redistribution | Production / replacement |
| --- | --- | --- | --- |
| Paving, columns, broken rear arch, side walls, pedestal | Nexora Showcase procedural geometry; repository contributors | Repository LICENSE; ships as source-generated geometry | Cube/segment blockout; replace with adapted free architectural models |
| Central ring | Original 20-segment procedural tube | Repository LICENSE | Engineering silhouette; final carved stone/bronze treatment pending |
| Crystal placeholder | Original `nexora.showcase.mesh.v1` mesh from Runtime asset generation | Repository LICENSE | Existing Import → Cook → Bundle → Runtime payload; report retains content hash; replace with faceted hero crystal |
| Ceramic / vegetation placeholders | Original capsule and tube geometry | Repository LICENSE | Static spatial markers; final ceramic models, foliage maps and wind pending |
| Third-party final models/textures | Free-asset shortlist in `Roadmap/art/Free-Asset-Sourcing.md` | No third-party asset adopted in this slice | Download, license/hash inventory, conversion and art review pending |

No purchased or commissioned content. Engineering geometry has no separate source download or
external dependencies. Production cost is implementation work; no art-production time estimate
is claimed. These placeholders must not be described as adopted KayKit/ambientCG assets.

Fixed camera order: wide (0), material close-up (1), motion finale framing (2). `B` restores each
camera deterministically; `9` re-enters at wide. `F4` hides all UI. All shots target the same central
device; the close-up and finale prepare future material/motion integration and do not demonstrate
those features yet. Baselines retain backend/build provenance and software-rasterizer limitations.

本切片僅提供原創程序化工程灰盒、固定鏡頭與既有資產流程代表性網格，不宣稱最終美術、
PBR、真實陰影、發光或風動完成。第三方免費素材尚未正式採用；模型／貼圖的下載、授權、
hash、格式轉換及目視確認仍待完成。沒有採購、委外或已確認的美術工期。
