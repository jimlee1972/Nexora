# ED-M2 counted-array Inspector controls: Linux evidence

Work started 2026-10-20; final main integration 2026-10-21. Beads `nexora-gu7.5.2`.
Core prerequisite PR676 passed all38 exact-head hosted checks and squash-merged as
`e5fd644c17060ef7718f727244194ab45c6aa0c2`, tree
`3d159d68c440d96e6c7bde57275097e6df5fbfe5`. This UI delivery does not modify that Core wire contract.

## Controls and authority

The owning version-two counted-tail observation supplies mixed length/count headers with Add/Clear
and common element rows with Insert/Remove/Up/Down and existing typed property editors. Fixed row
height and clipping preserve complete data. Add uses each target's own tail length and a valid
typed default, including the first nonzero enum choice and color alpha1. Whole-selection capacity,
indices, writer/Play/modal/recovery policy, catalog revision, selection and exact source bytes are
revalidated before one Core authoring transaction. Failures retain data/history and report rejection;
structural changes revoke numeric drafts. No native object/vector pointer, async callback, document
format or UI IO is introduced. Private geometry probes own type/path/action labels and positions.

## Actual product controls

The real ImGui 1x/2x pointer, physical modifier-key and text-event suite covers Add/Insert/Remove/
Up/Down/Clear, mixed lengths/values, atomic keyboard Undo/Redo, capacity/index refusal, partial
candidate allocation failure, character-by-character no-partial typing, Enter/Escape/invalid value,
metadata replacement and read-only/focus handoffs followed by Enter. Old drafts cannot revive.
All supported typed Add defaults are exercised through actual widgets at1x. Unknown prefix/other
component and following-tail values survive. Ctrl+S routes through the production SceneFileSession;
fresh World/file-session/UI reopen verifies exact bytes and interpretation.

## Native software-display workflows

`LinuxReflectedArrays.py` seeds a production workspace/scene fixture with schema-two array bytes
and one unavailable opaque component, then performs every edit through real Window/X11 input.
Presented hover/held/released pixels synchronize buttons; there are no structural-edit retries.
Add/Insert/Remove/Up/Down/Clear and numeric editing pass, including exact whole-file Add Undo/Redo,
Ctrl+S, fresh writable/read-only native processes, read-only refusal and exact unknown prefix/
unavailable-component conservation after every save. Metadata never changes. Fixture-setup failure
also leaves its isolated temporary directory empty. Registration uses the existing Linux graphical
Xvfb/xdotool condition, with a120-second test bound and unchanged other deadlines.

The final native test source passes on isolated Debian Xvfb21.1.16, xdotool3.20160805.1 and Mesa
lavapipe25.0.7. `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` explicitly enables validation;
Vulkan error assertions remain. The unchanged legacy `LinuxReflectedInspector.py` also passes
actual native edit/Undo/Redo/save/reopen/read-only and corrupt/valid metadata restoration against
this same Core/UI binary. That restoration checks metadata, not native plugin objects.

```bash
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation \
python3 Tests/EditorImGui/LinuxReflectedArrays.py \
  --editor build/linux-development/Apps/Editor/NexoraEditor \
  --fixture build/linux-development/Tests/Editor/NexoraReflectedInspectorTests \
  --xvfb /path/to/isolated/Xvfb --xdotool /path/to/isolated/xdotool
```

Software display execution does not certify physical GPU/DPI/IME, glyph quality, screen readers,
native plugin restoration, production performance or complete ED-M2.

## Complete gate and actual-main integration

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development -R 'editor.(reflected_array_ui|reflected_arrays|reflected_inspector|authoring_reopen_ui_1x|authoring_reopen_ui_2x)$'
CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
ctest --preset linux-shipping
```

Fresh Development configure/build passes320 integration steps. The focused regex selects four
registered legacy/UI/authoring tests:4/4 pass in0.79s. Full serial Linux gate passes297 registered,
285 actual passes,12 existing environment skips,zero failures in512.14s. Deep60.83s and original
100k215.34s retain scale1 and180/300-second deadlines. All14 frozen Core/UI/native/helper/CMake
hashes remain unchanged; full LastTest is archived externally. The base local gate does not put
isolated Xvfb tools on PATH, so the new optional native registration is unavailable there; its
exact script passes through the explicit command above. Hosted Linux enables that registration.

Minimal Monolithic Shipping configure/build previously passed9 metadata integration steps over
the complete102-step build on unchanged UI/Core runtime sources. This profile strips Editor and
disables tests; Shipping CTest registers none. The added Linux test registration does not change
Shipping linkage. Slang/Zig are disabled locally. Documentation16/links/bilingual/changed-line
format/diff checks pass; repository-root README stays unchanged.

The actual accepted Core squash is integrated normally in merge commit
`b0b7739fee57277f56f1e72af11b9bcc07ee38f2`. The only conflicts were bilingual delivered-status
paragraphs, resolved retaining both Core evidence and the now-validated UI. The complete file tree
after this merge is EXACTLY the tested pre-merge tree
`335653c39273d62c4af88620cc4f96da5d2f4fe0`; no runtime/test/document bytes changed. Core's six
implementation/test files match actual main. Only this evidence is finalized afterward.
Applicable exact-head hosted UI checks must pass before squash merge. M2-03 parent completion
requires this UI merge; ED-M2 and physical acceptance remain open.
