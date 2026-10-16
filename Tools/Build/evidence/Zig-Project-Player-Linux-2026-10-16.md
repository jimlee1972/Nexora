# Zig project Player and main Editor 2D integration

Scope: engine-owned generic project hosting/export, selected Windows font preparation, and main
Editor Play services/Game View. Game rules, production art/font rights and physical device
acceptance are outside this cloud result. C/Zig wire layouts and durable storage envelopes do not
change. The sample host delegates to the common Player runner; it retains its fixture behavior.

## Validation environment

Managed Linux, GCC 14, Zig 0.14, Slang 2026.18, Xvfb and software Vulkan. Development uses the
Modular preset with graphical Editor, 2D, native ProjectPlayer and Zig enabled; Showcase is disabled.
Shipping uses the Monolithic preset with the existing Showcase enabled. Provisioned tool paths,
shader caches and all generated outputs stay outside version control. No Windows/MSVC executable
or physical sound/controller/display was run in this Linux environment.

## Acceptance

- `editor.play_twod_services`: 32 actual project-captured Play clone lifetimes, frozen assets after
  source/index owners die, scene plus pool extraction, UI clips, unique immutable identities,
  retained frame bytes after unload, retirement, input acquisition/revocation and authoring history.
- `editor.play_twod_native`: same public/main host plus actual native ImGui composition across 32
  sessions; checks red Sprite/UI pixels inside the Game rectangle, pixels outside its clip,
  successful draw/present and texture retirement without rejected textures. Software Vulkan is
  native API evidence, not physical GPU performance or Windows display acceptance.
- `gameplay.zig_services_consumer`: real Zig `ui.registerBakedFont` decodes the Windows baker
  format and registers through the actual UI table; truncated bytes and insufficient scratch
  preserve the font handle. Caller-owned scratch avoids an implicit allocator dependency.
- `twod.project_manifest`: real manifest/font binary layout, bounded corrupt inputs, path/field/ID
  admission and user-scoped save path policy. The tiny CJK codepoint fixture checks layout only.
- `twod.project_export` and `twod.project_export_native`: an independently built V3 UI-only module
  with no FixedUpdate, sprites or pool assumptions; Unicode source/output/save roots; immutable
  exported bytes/hash inventory; two-process permanent-slot restart; corruption, path and malformed
  asset rejection. Native mode presents actual UI frames on Linux Vulkan.
- `ZigProjectAcceptance.py --shipping`: exports the real separate Zig shared library and cooked
  Atlas/Clip/WAV fixtures through the general Shipping Player; validates reversed descriptor asset
  ordering/dependency closure, no fixed validation-host mode or forced reload, allocator balance
  and rejected corrupted Cook. The staged run removes development library search paths. Fixture
  assets remain explicitly labeled and are not production artwork.
- `ProjectAcceptance.py` also runs against the Monolithic Shipping executable, using the independent
  test module for save/restart and failure checks.

Final Linux results:

- Development configure and full build passed. The first complete CTest run exercised all 320
  cases: 318 passed, the new native-export test exposed a test-only driver search-path issue,
  and an existing Prefab native interaction case failed its Undo observation.
- Preserving the provisioned Mesa driver path fixed the native-export test. The unchanged Prefab
  case passed an isolated rerun (92.84 s). After the final clean build, the complete changed-feature
  and failed-case selection passed **21/21**, including the real Zig baked-font consumer, static/
  dynamic/wire wrappers, generic CPU/native project export, both Editor 2D cases and that Prefab
  interaction case. No local failure remains unresolved; this does not characterize a physical
  machine's frame-time budget.
- Monolithic Shipping configure/full build passed; the final incremental Shipping build and real
  separate Zig project export/run/corruption checks passed. The independent UI-only module also
  passed Shipping process restart/save/corruption checks. Changed-document validation and
  `git diff --check` passed.

Commands for reproduction:

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON
cmake --build --preset linux-development -j 4
ctest --preset linux-development
ctest --preset linux-development -R '^(gameplay.zig_|editor.play_twod_|twod.project_|editor.linux_prefab_isolation$)' --output-on-failure
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j 2
python3 Tests/TwoD/ZigProjectAcceptance.py --app build/linux-shipping/Apps/TwoDPlayer/NexoraTwoDPlayer --module build/linux-development/Gameplay/Zig/libNexoraZigTwoD.so --compiler build/linux-shipping/Tools/TwoD/NexoraTwoDAssetCompiler --output /tmp/zig-game-project-evidence --shipping
python3 Tests/TwoD/ProjectAcceptance.py --app build/linux-shipping/Apps/TwoDPlayer/NexoraTwoDPlayer --module build/linux-development/Tests/TwoD/libNexoraTwoDProjectFixture.so
```

The Windows Shipping CI job additionally builds the standalone Player and separate Zig DLL,
exports the generic fixture with explicit x64 MSVC CRT copies and executes a bounded CPU run.
Windows Development CTest includes a real GDI font corpus bake, nonempty alpha/metrics and honest
missing-glyph/non-BMP/overwrite rejection. Those hosted jobs must pass before merging; they do not
accept the user's physical machine. Target checks are in the [Windows list](../../TwoD/WindowsAcceptance.md).

## Remaining limits

One startup scene/1024 pooled instances in the standalone host; main Editor uses its selected Play
scene/4096 pool and read-only frozen Atlas/Clip closure. Sprite authoring persistence, Editor
Tilemap binding, seamless hot reload, dynamic shaping/IME, multiple simultaneous save writers and
production game content are not supplied. The font baker accepts a bounded BMP corpus and the game
owns UI layout, save schema/migrations/commit points and error presentation. Physical Windows
sound, gamepad plug/unplug, DPI/focus, clean-host startup and minimum hardware remain unaccepted.
M6/M7, ED milestone totals and API-M1–M6 percentages remain unchanged.
