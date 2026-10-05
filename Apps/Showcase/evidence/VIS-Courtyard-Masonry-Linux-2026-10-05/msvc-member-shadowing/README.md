# MSVC member-shadowing correction

The foreground sprig local `radius` is renamed `sprigRadius` to avoid MSVC /WX C4458
against the camera member. Exact source comparison after identifier normalization
proves all expressions and values unchanged. ✅ Linux configure/build and all 97 tests
pass (102.71 seconds), including 85 native PBR frames with core/sync validation.
Logs and source hashes are retained here. Earlier Shipping/movie stay tied to their
recorded production freeze; Windows CI builds the corrected source again.
