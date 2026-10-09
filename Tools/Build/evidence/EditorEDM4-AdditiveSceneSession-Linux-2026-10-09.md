# Additive authoring document ownership — Linux cloud evidence

`AdditiveSceneSession` borrows one current workspace and Editor World and owns up to sixteen live
SceneDocuments/FileSessions plus a deterministic owned/reference dependency graph. Hosts serialize
calls, stop Play and drain background readers before membership changes or coordinated publication.
Active switches preserve document identity, generation, selection, opaque payloads and per-document
Undo/Redo. Existing primary owners may be borrowed and must outlive the session; detachment preserves
them. Newly admitted records are released after their file/document owners, including failed source
or dependency admission. Runtime Editor teardown never rewinds the monotonic allocation watermark
and rejects Play Worlds.

Source paths remain distinct across all open owned/reference documents, including portable ASCII
case and filesystem aliases. The owner routes Save As through that association check, excludes
references from source edit/file mutation access and Save All, and rejects existing global entity ID
collisions without rewriting unknown bytes. Dependency changes are complete and acyclic; required
scenes cannot be removed. Current project/document tokens and live World lifecycle states are checked;
unloading/unloaded composition members reject dependency admission/replacement and Save All.
Permissions remain a trusted host policy, not a native plugin sandbox.

## Actual acceptance

Real file-backed tests switch two independently dirty documents, preserve exact source baselines,
selection, opaque component bytes, tokens and independent Undo/Redo, save both, and independently
reopen owned/reference sources. They reject stale tokens, duplicate/ASCII case-colliding destinations,
Save As over another open file, missing dependencies, cycles, unresolved recovery, old project scope
and read-only source writes. Sixty-four rejected source/collision admissions preserve a foreign
sentinel and release their new World records. A genuine sixteen-document owner rejects its
seventeenth admission, rejects unnamed Save All and releases every owned record. Reader teardown,
borrowed-primary detachment, pending/unloaded foreign attachment, live-composition publication,
persistent-record release and exhausted-ID behavior are exercised.

The first focused run failed because its reader fixture accidentally requested a second writer while
holding the writer lease. The fixture now explicitly opens ReadOnly; production lease behavior and
assertions were retained. Confirmation passed **1/1 in 0.03s**, then the lifecycle-focused run passed
**1/1 in 0.02s**. The initial full graphical/native build passed 393 steps and
**217/217 in 503.70s**, zero skips; minimal Shipping passed 74 steps.

After adding the live composition/dependency/publication checks and integrating reviewed batch
recovery fixes at **addee6ea19faf5752fa3dd5664aafb1904d46caf** atop accepted overview main
**8e9b153966c1b3c94aefae4ae197b59fb08b6387**, focused acceptance passed **1/1 in 0.02s**.
The final graphical/native incremental build passed 281 steps and **217/217 in 495.87s**, zero
skips; minimal Shipping passed 14 incremental steps. No native input/helper/assertion/deadline
changes belong to this owner patch.

~~~sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_BUILD_SHOWCASE=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON \
  -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
~~~

GCC 14.2, CMake 3.31.6, Ninja, Slang 2026.18, Zig, Xvfb/xdotool and Mesa software Vulkan were used.
Touched C++ formatting/diff checks pass; Linux CI requires the owner test registration. Final-head
hosted checks and accepted dependency integration are required before merge. This Linux execution
claims no Windows/macOS or physical-host acceptance. The C++ API requires rebuilding consumers and
changes no scene/gameplay schema. Graphical tabs, persisted composition and full ED-M4 remain open;
all complete Editor milestones remain 0/8.
