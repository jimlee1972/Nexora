# Nexora production branding

The only approved reference is `References/nexora-approved-preview.png` from `1046ab60`.
Maintained SVG paths reproduce its upper lowercase wordmark and independent n silhouette,
including the shoulder gap and cyan upper-left x segment. Flat #47D9F2 cyan and #F5F5F5 near-white
replace the reference's raster texture. No replacement font or noisy background removal is used.
The preview board is never an application icon.

- `Source/nexora-wordmark.svg` and `Source/nexora-editor-icon.svg`: editable transparent paths.
- `Export/nexora-wordmark.png`: 2320 × 500 straight RGBA wordmark.
- `Export/nexora-editor-icon.png`: independent 1024 × 1024 transparent master.
- `Export/nexora-editor-icon-{size}.png`: directly rasterized 16/24/32/48/64/128/256 PNGs.
- `Export/nexora-editor.ico`: seven independent PNG-compressed RGBA icon images.
- `Export/Nexora-Brand.zip`: SVG sources, PNG/ICO exports, review sheet and SHA-256 manifest.
- `Export/nexora-brand-review.png`: black/white/dark review backgrounds; review only.

Rebuild with `python Tools/Brand/ExportBrand.py` after installing its authoring requirements.
Production CMake builds use committed exports and require no rasterizer or font. The standard
library gate `python Tools/Brand/VerifyBrand.py` decodes PNG alpha and checks transparent borders,
clean cyan edge pixels, the n counter, all actual ICO dimensions/payloads and ZIP hashes.
Windows admission reads real PE resources as data and compares the embedded wordmark and icon
images byte-for-byte with these validated exports. Runtime never loads artwork from a checkout,
working directory, user drive or network.

Brand SVG/JSON line endings are pinned to LF in `.gitattributes`; a Windows autocrlf checkout
roundtrip is admitted against the same ZIP/hash gate. PNG/ICO/ZIP remain binary.
