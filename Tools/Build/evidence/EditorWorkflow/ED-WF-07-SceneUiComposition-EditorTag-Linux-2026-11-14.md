# ED-WF-07 Editor Sprite translation rejects Scene references

Date: 2026-11-14 (UTC). Supporting task `nexora-whr.7.8.1`; parent `nexora-whr.7.8` remains open.

The existing `DrawGameTwoDSprites` adapter accepts indexed Sprite quads and emits ordinary ImGui
image commands. After the native Scene-reference extension, it still interpreted only `textureId`
and silently discarded a nonzero `sceneTexture` field. An otherwise valid packet carrying that
unsupported tag could therefore append geometry instead of rejecting the packet.

The adapter now rejects every nonzero Scene reference during its complete prevalidation pass.
The existing real Game preview fixture injects both complete and partial references into its last
batch while retaining valid texture IDs. Both reject without changing draw-list vertex counts;
the subsequent ordinary packet still submits successfully and its duplicate still rejects.
The [Presentation contract](../../../../Engine/Presentation/SceneUiComposition.md) and
[Editor contract](../../../../Engine/EditorImGui/README.md) describe this boundary. No owning ImGui
Scene image binding or configured packaged source Play is enabled by this guard.

## Reproduction and final acceptance

An external copy of the real fixture added the bad-tag assertion and ran against the original
`dbb3ad7` Editor library. It exits **1** with `Scene texture reference was silently discarded by
the Sprite translator`. The identical external executable exits **0** after replacing only the
production Editor library with the fixed build. Source, executable/library hashes, commands and
logs are retained in the [machine receipt](ED-WF-07-SceneUiComposition-EditorTag-Cloud.json).
The initial standalone setup mistakes (missing local fixture include and executable-relative
library path) are retained separately. ALSA no-device messages occur in both runs; they are not
the regression assertion.

The extended committed fixture passes 1/1 in **1.94s** before the complete gate. Fresh full Linux
Development passes **527/527**, zero failures/skips, in
**1889.90s**. All 11 configure/build/test/native gates pass, including actual Shipping
Full native composition, Monolithic five-test consumers (now including the Editor Sprite
translator), and TwoD-OFF/testing-OFF graphical Editor/native Player builds.

The two changed C++ translation units additionally compile with GCC 15.2 using their actual
configured flags. The initial 339-variant matrix and its 17 native implementation/build/fixture
hashes remain unchanged; this supplement freezes those files plus the two Editor files (19 total).
Both original `dbb3ad7` evidence files remain byte-for-byte unchanged and continue describing their
original acceptance. The full local engine uses GCC 14.2. Hosted CI must validate the new published
head separately; the earlier `dbb3ad7` checks do not validate this follow-up.

The repeated actual Development and Shipping native fixtures each pass 12 frames, 24 Scene draws,
48 UI commands and two resizes. These are Linux/X11/software Vulkan observations, not local
Windows/macOS/mobile or physical-product acceptance.

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development --output-on-failure
ctest --test-dir build/linux-development -R '^editor\.game_twod_preview$' --output-on-failure
python3 Tools/Build/ValidateApiM6.py .
python3 Tools/Build/TestDocumentationCI.py
```

The receipt retains the full configuration and linkage runner. Existing packaged Editor source
Play remains guarded. Main Player/Editor source services, production reload, full ED-WF-07/08,
Workflow **5/9**, Editor **0/8**, TwoD **9/11**, physical/clean-host acceptance and the unconfirmed
residency issue `nexora-ko9` remain open/unchanged.
