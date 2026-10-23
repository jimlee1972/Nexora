# SP-02 diagnostic font

`ui_fixture.nxfont` is a raster atlas baked from **Noto Sans CJK TC Regular**, release
Sans2.004: https://github.com/notofonts/noto-cjk/tree/Sans2.004 . The source OpenType SHA-256 is
`dce08bd4fd91aa8aa76ed8fea4b694c2dfb8550f67871e326843212ddbeb88b4`.
The accompanying OFL.txt is the source distribution's SIL Open Font License 1.1.
Use `Tools/TwoD/BakeFont.py --pixels 32 --width 512` with `ui_corpus.txt` to regenerate.
Pixels and glyphs are actual raster output. This small corpus tests rendering and does not
establish final game typography, complete subtitle coverage or acceptance of diagnostic icon art.

Source copyright: © 2014-2021 Adobe (http://www.adobe.com/).
