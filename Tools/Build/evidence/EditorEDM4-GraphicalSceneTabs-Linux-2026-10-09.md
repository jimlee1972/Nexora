# Graphical additive scene documents — Linux cloud evidence

The native Editor now hosts up to sixteen owning document/file sessions in one Editor World.
The copied tab strip presents active, dirty and owned/reference state and transfers generation-bound
Select/New/Open owned/Open reference/Close/Save All requests. Both source and target tokens and
workspace/recovery/Play/export policy revalidate in the application. Widgets perform no scene IO.
Reference tabs permit inspection and selection while source authoring, ordinary Save and Play Apply
are disabled independently in widgets and host paths. Dirty tab close offers Save All, explicit
Discard and Cancel; the borrowed primary remains pinned. Unnamed owners require Save As.

Ctrl+Alt+N/O/Shift+O/S/W and PageUp/PageDown provide the corresponding document workflows. Active
switches retain independent selection/history/source identity and camera state, cancel stale gestures
and Inspector drafts and clear native preview caches. Coordinated Save All includes every owned file
and publishes Content updates only after success; references are excluded. Window close considers
all owned dirty documents, even with a clean active reference. Save All and Exit resolves unnamed
owners one at a time and preserves retry intents before coordinated publication. Membership changes
wait for stopped Play and export readers; shutdown cancels/drains jobs and unloads Play first.

The versioned private composition persists named roles, load order and active selection. Restore
stages all sources and freezes source authoring when the complete set cannot be restored, preserving
the bootstrap and all foreign source/metadata versions. Repair permits a complete reopen. Initial
composition metadata starts with multiple documents; once present it also records a later single
remaining document. Ordinary single-scene sessions preserve the legacy startup/fallback contract.
Legacy startup observation initializes before bootstrap so later Open can remember its destination.
Metadata Save never implicitly saves source documents, and unnamed/unresolved sets retain the old
composition. This is serialized trusted host policy, not a plugin sandbox, power-loss durability or
concurrent hostile-filesystem guarantee.

Runtime entity identities and unknown payloads remain unchanged. Identity collisions across existing
sources reject; references own inspection documents and never grant source write access. Built-in
hierarchy parents remain within their original scene; no opaque cross-source reference is rewritten
or inferred. The SDK supports deterministic dependency/load-order policy. A graphical dependency
editor, combined multi-scene canvas/Hierarchy and broader migration/prefab/provider UX remain separate
work; the current Scene canvas and Hierarchy inspect the active document.

## Actual acceptance

Real ImGui controls at 1x/2x verify owned requests, Unicode chooser paths, Select/New/Save All,
dirty-close Cancel/Discard/Save All continuations, actual token changes, nonactive target replacement,
invalid/mixed-project/oversized metadata, read-only/busy/frozen gates and rejection of queued edits
without later resurrection. Metadata copies outlive caller mutations. No source IO occurs in UI tests.

The actual X11 Editor driver creates distinct primary/second documents, makes both dirty, switches
without implicit writes, saves both and restores exact original bytes with one independent Undo in
each document. It creates/closes/reopens a reference, proves source editing/Save/Undo cannot alter
it, checks native window-close protection for an inactive dirty owner and saves that owner from the
active reference. Both real read-only and writer relaunches restore three documents, one reference
and its active selection with unchanged source/metadata bytes. A corrupt final source retains one
bootstrap, freezes authoring and preserves all versions; repair reopens the full set. Closing back
to one document updates the existing composition and restart does not resurrect closed members or
change/delete any source file. Existing native scene New/Open/Save As, Content relocation/Undo,
malformed legacy startup fallback, Save and Exit retry and per-file camera persistence remain tested.

Initial focused controls passed 1/1 in 0.09s and native integration 1/1 in 18.36s. Expanded composed
controls passed 1/1 in 0.11s and native reopen/failure policy 1/1 in 21.81s. The initial full build
completed 393 steps; its 220-test gate reported 219/220 in 516.67s with a real legacy startup regression:
composition bootstrap skipped RestoreStartup initialization, so Open loaded but RememberCurrent
could not acknowledge its path. The failure and original assertions are retained; Shipping did not
run after that failed set-e gate. No deadline, gesture, source or assertion was weakened.

After the startup/persistence correction, a two-step application build and both affected native tests
passed 2/2 in 61.55s (scene files 39.93s; additive tabs 21.62s). The added close-to-single/restart case
passed 1/1 in 24.95s. Final full configure/build completed 189 incremental steps and **220/220 in
531.34s**, zero skips; minimal Shipping completed 74 steps. The same full run includes all existing
native workflows. After all three composition review corrections, the renewed full build completed
239 steps and Linux passed **220/220 in 533.15s**, zero skips; minimal Shipping completed five
incremental steps. Rebasing the three graphical commits onto accepted composition main
**28af164c0025a854cf9a03363e0eca96d0550a99** produced an identical complete Git tree, verified against
the tested **30167a4e** tree. Only this evidence paragraph changes afterward. The integration
candidate containing corrected semantic comparison and graphical inspection independently passes
seven focused cases in 45.81s, including real native tabs/reopen and both new comparison scope cases;
its full gate is separate and is not claimed complete here.
native display/Scene/Game/import/export/input acceptance. English/Traditional Chinese supporting
roadmaps and module contracts are synchronized; the root README and complete milestone count 0/8
remain unchanged.

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

GCC 14.2, CMake 3.31.6, Ninja, Slang 2026.18, Zig, Xvfb/xdotool and software Mesa Vulkan were used.
Local focused commands select editor.scene_tabs, editor.linux_native_scene_tabs and the existing
editor.linux_scene_files. Dropping temporary foundation branch commits changes documentation only;
the tested functional source is identical. Linux CI requires both tab and native driver registrations.
Accepted composition integration and fresh exact-head hosted checks remain required before merge.
No local Windows/macOS or physical display result is inferred from Linux. Full ED-M4 stays open.
