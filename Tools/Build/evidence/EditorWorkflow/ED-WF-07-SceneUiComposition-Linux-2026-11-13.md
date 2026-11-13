# ED-WF-07 fenced Scene images in one UI submission

Date: 2026-11-13 (UTC). Supporting task `nexora-whr.7.8.1`; parent `nexora-whr.7.8` remains open.

An explicit offscreen LDR `sampledByUi` producer exposes a typed surface/frame/view value through
`SceneTexture()`. Indexed UI commands sample the fenced Scene attachment inside the existing one
`RenderUi` pass, preserving ordinary texture IDs, C/Gameplay/Zig wires, Sprite and GUI ordering.
Foreign, stale, partial, unprepared, instanced and malformed references reject before UI uploads.
The existing single UI submission rule remains enforced. This is a native composition boundary;
configured packaged Editor source Play remains guarded pending its own integration and acceptance.

The [contract](../../../../Engine/Presentation/SceneUiComposition.md) defines ownership, owner-thread
phases, clipping and publication duties. Native resources remain completion protected. Vulkan
records attachment-write to shader-read synchronization; DX12 retains the sampled target through a
later disjoint direct view; Metal samples its owned texture in the UI encoder. Conservative image
clip union allows one disjoint direct LDR/non-shadow Scene afterward. Ordinary exclusive Scene,
HDR, FP16 TwoD and arbitrary integer texture workflows keep their contracts. C++ consumers rebuild.

## Actual acceptance

Full Linux Development passes **527/527**, zero failures/skips,
**2028.11s**. All11 configure/build/test/native gates pass: Development, actual Shipping
Full native consumer, Monolithic four-test consumers and TwoD-OFF/testing-OFF graphical Editor/native
Player build. The task-owned deferred Shipping consumer links the same native fixture against
actual Shipping libraries without adding production test targets or changing repository presets.
The [machine receipt](ED-WF-07-SceneUiComposition-Cloud.json) records commands, all test results,
17 frozen implementation/build/fixture hashes and artifact hashes.

Each actual Development and Shipping native fixture presents12 frames, records24 Scene draws,
48 UI commands and12 Scene composites, with two resizes and one UI pass per frame. Actual X11 pixels
check green Scene image, red textured quad, yellow UI occlusion, blue background and a disjoint red
3D view. Four asymmetric image/native coverage points verify orientation and independent per-view
uploads; the empty image pixel checks the selected linear/sRGB transfer within two 8-bit codes.
Foreign/stale references reject before texture uploads or draws. Duplicate UI and overlap reject;
CPU admission additionally rejects a third view. Drain is idempotent and clears reference access.
These are software Vulkan/Xvfb
observations, not physical display or product performance acceptance.

The shared fixture additionally runs12 frames in the existing Linux/Windows Vulkan and DX12 PBR
test and the Metal Scene test. Existing PBR assertions remain, followed by separate new submission
deltas. Local Linux execution and applicable hosted exact-head platform checks are distinct gates;
this receipt does not claim local Windows, macOS, Android or iOS execution.

All **339** actual configured Presentation-consuming compile-command variants
additionally pass GCC15.2. This is a compilation matrix; the full local engine uses GCC14.2.

The initial CPU fixture omitted a required aggregate field and its compiler failure is retained.
The full-configuration preflight initially registered502 tests because the earlier focus cache
kept Showcase disabled. No tests ran in that attempt. Explicitly restoring Showcase and all full
feature options registers527; the registration mismatch and configure log are retained separately.
The strengthened asymmetric native oracle initially assumed every clear channel was below80,
but actual sRGB clear pixels are44,60,85. Captured/viewed pixels showed image/native coverage agreed.
The corrected oracle compares independent native coverage plus the selected transfer, without a
renderer change or deadline increase. An external control inverts only Scene image UV.v and
correctly fails with the unchanged renderer. Its source, command, binary/library hashes, capture
and failed log are retained, including the first standalone X11 link setup error.

## Reproduction

```bash
cmake --preset linux-development -DNEXORA_BUILD_SHOWCASE=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_ZIG_EXECUTABLE=/workspace/work/engine-goal-20261112/tools/lib/python3.12/site-packages/ziglang/zig -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development
ctest --preset linux-development --output-on-failure
ctest --test-dir build/linux-development -R '^window_presentation\.(scene_ui_composition|vulkan_scene_ui|vulkan_pbr)$' --output-on-failure
python3 Tools/Build/ValidateApiM6.py .
python3 Tools/Build/TestDocumentationCI.py
```

The hashed runner and Shipping consumer in the machine receipt retain exact linkage/OFF commands.
Main Editor source/Atlas admission, mixed configured Play, Replace/Additive remapping,
Pause/Step/Stop/restart, read-only project preservation and transactional publication remain parent
acceptance. Production reload and main packaged source services remain separate work. The guarded
source workflow, unconfirmed residency issue `nexora-ko9`, Workflow **5/9**, Editor **0/8**, TwoD
**9/11**, complete ED-WF-07/08 and physical/clean-host acceptance remain open/unchanged.
