# Courtyard authored image sources

These unmodified PNGs were created with OpenAI image generation during the authorized Nexora
showcase art session, using the repository's [visual concept](../../../../../Roadmap/art/V1-Visual-Identity-Concept.png)
as an art reference. They are material authoring inputs, not screenshots or native-rendering evidence.
They are contributed under the repository LICENSE; they are not third-party CC0 downloads.

`../source.json` retains the authoring briefs, origin, license and exact source SHA-256 values.
`PrepareCourtyardHero.py` verifies those hashes before deriving bounded Runtime payloads.
`CourtyardImageCook.py` decodes bounded RGB/RGBA8 PNG with standard Python only, uses integer
area filtering, and preserves ivy proportions and alpha-weighted edge colors. The ivy card is
vertically mapped so UV zero meets the stem root used by the existing wind shader. Sandstone
normal/AO/roughness maps are derived from the paired grayscale height and authored color;
metalness is zero. The bronze maps remain procedural original authoring.

本目錄保留本次 Nexora 展示場景美術工作中，以 OpenAI image generation 製作的原始 PNG。
這些是材質來源，並非引擎截圖或渲染驗證證據；依專案 LICENSE 提供，不宣稱是第三方 CC0 素材。
來源描述與 SHA-256 記錄於上層 `source.json`；標準 Python cook 會核對來源，再產生有界貼圖，
保留常春藤透明邊緣與比例，並以配對高度圖產生石材 normal／AO／roughness。
