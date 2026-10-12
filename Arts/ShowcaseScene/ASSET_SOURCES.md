# Asset sources

The reconstructed scene uses the following Poly Haven assets under
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
The exact download URLs, publisher MD5 values, file sizes and verified SHA-256 values
are retained in [asset-manifest.json](asset-manifest.json).

| Asset | Authors | Use |
| --- | --- | --- |
| [Sandstone Cracks](https://polyhaven.com/a/sandstone_cracks) | Rob Tuytel | Limestone surface maps |
| [Kloppenheim 06 (Pure Sky)](https://polyhaven.com/a/kloppenheim_06_puresky) | Greg Zaal; Jarod Guest | Environment lighting and sky |
| [Mountainside](https://polyhaven.com/a/mountainside) | Dario Barresi; Rico Cilliers | Scanned valley cliffs |
| [Rock Moss Set 01](https://polyhaven.com/a/rock_moss_set_01) | Kless Gyzen | Scattered mossy stones |
| [Weed Plant 02](https://polyhaven.com/a/weed_plant_02) | Rico Cilliers; Rob Tuytel | Courtyard groundcover |

All 18 source files match the manifest. Image data and appended mesh assets used by
`ruins_scene.blend` are packed or local datablocks; they do not require the original
source folders at render time. Downloads needed for rebuilding remain locally in
`textures/` and `assets/`, and can be restored with `scripts/fetch_assets.py`.
The saved scene contains 16 packed image datablocks, including the concept reference.

The masonry, ring, altar, crystal, banners, pottery, ivy, grass and other procedural
geometry are authored by `scripts/rebuild_ruins.py`. The supplied concept image is
retained solely as a hidden reference and is never rendered as a scene backdrop.
The CC0 designation applies to the listed third-party assets, not to the supplied
concept image or the archived original draft.
