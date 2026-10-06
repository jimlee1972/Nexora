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


`golden-sky.png` retains the unmodified original sky authoring output. Its opaque RGB colors
are converted to canonical linear binary32 radiance, downsampled to 512×256 and sampled with
U wrap/V clamp. The visible atlas and floating HDR IBL share its sun-aligned azimuth; the
engine's separate geometric HDR sun preserves radiance above display white. The PNG itself
is an LDR art source, not a native capture or a measured HDR photograph.
`golden-sky.png` 保留原創天空來源；線性化後的天空圖同時供 skybox 與 HDR IBL 使用，方位與
引擎太陽一致。PNG 本身是 LDR 美術來源，並非原生截圖或實測 HDR 照片。


The original crystal geometry is authored separately in `../source.json`: five radius/height
rings include an optional azimuth phase. The cook selects outward diagonals for nonplanar
quads and rejects any face whose plane cuts through the convex hull. Per-triangle normals
retain the faceted silhouette. This changes the original mesh; the retained image inputs
and their source hashes are unchanged.
原創水晶幾何另由 `../source.json` 描述五層半徑／高度與可選方位相位。Cook 選擇朝外的
非共面四邊形對角線，並拒絕切入凸包的面；每個三角形保留獨立法線。原始 PNG 與來源
雜湊保持一致。

The active stone pair is `sandstone-refined-albedo.png` / `sandstone-refined-height.png`, created with OpenAI image generation on 2026-10-06 against the retained original material and courtyard reference. The earlier `sandstone-albedo.png` / `sandstone-height.png` files remain unmodified archives. `source.json` identifies the active pair and exact hashes. These are authored LDR material inputs; native lighting and HDR remain engine computations.

目前使用 `sandstone-refined-albedo.png`／`sandstone-refined-height.png`，於 2026-10-06 以原始材質與庭院參考圖製作；原本的 `sandstone-albedo.png`／`sandstone-height.png` 保留且未修改。`source.json` 記錄目前來源與精確雜湊。這些是 LDR 材質輸入，原生光照與 HDR 由引擎計算。
