# ED-M6 graphical StaticView export job — Linux cloud evidence

This slice publishes a cooked StaticView package from the current authoring scene. It does not
compile an executable, deploy a target, publish a site or certify physical display behavior.
Root README and the full Editor milestone count remain unchanged at 0/8.

## Ownership and publication

The authoring thread captures the prepared scene, complete Runtime snapshot, imported CPU
geometry/material values, source project/scene identity and content revision. One retained job
owns these values; workers never retain a World, document, content catalog or UI pointer. Existing
pure cooking and actual Runtime decoding precede sibling staging. Readback must match the complete
produced bytes. Publication requires a fresh access/recovery/project/content/scene/catalog check
and a single native replacement of .nexora/exports/static-view.nxproject.

Cancellation, stale authoring, unavailable access and producer failure preserve the last good
package. Foreign staging, aliases and changed staging metadata reject without overwriting those
entries. Cleanup removes only the owned regular stage under unchanged safe parents; an unremovable
or changed stage produces a bounded diagnostic for inspection. This is cooperative publication,
not isolation against a hostile concurrent filesystem writer. Cancellation is checked between
phases and 8192-byte read/write chunks; pure cooking/decoding cannot be interrupted internally.

The bounded status records actual byte count, checksum and verifier command only after publication.
It changes neither scene saved-baseline nor authoring Undo/Redo. Shutdown cancels export intake,
drains the shared import queue and joins the retained export before destroying borrowed owners.
Public C++ consumers must rebuild; C/Zig gameplay ABI and package wire schemas remain unchanged.

## Actual acceptance

Core fixtures import real OBJ/scalar PBR content and consume the package with Runtime. They cover
owning values, deterministic repeated bytes, opaque metadata/full UUIDs, dirty scene preservation,
queued and staged cancellation, changed authoring and untracked Runtime entities, changed content
and catalogs, read-only/recovery gates, project switching, occupied/changed staging, destination
failure, producer failure and shutdown. POSIX alias cases do not imply Windows symlink privilege.

ImGui tests drive actual controls at 1x and 2x, consume a real published package, cancel a real
staged job, retain the previous package, reject read-only requests and hide previous-source status.
The checked-in native acceptance opens the actual Vulkan Editor under Xvfb in a Unicode project,
uses Build/Export controls, reads the output with the standalone ProjectPlayer, repeats export,
adds an unsaved entity, exports the current scene, performs Undo and requests native window close.
It verifies source files remain unchanged and package identity/counts are correct.

## Commands and results

~~~sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON \
  -DNEXORA_BUILD_PROJECT_PLAYER=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
~~~

GCC 14.2, CMake 3.31.6, Ninja, Slang 2026.18, Zig, Xvfb and Mesa software Vulkan were used.
Initial complete graphical validation passed **205/205**, zero skips, **398.36s**, followed by
the 74-step Minimal Shipping build. The native workflow separately returned
NATIVE_EDITOR_STATIC_EXPORT_PASSED with initial_entities=1, unsaved_entities=2,
deterministic=true and source_preserved=true.

After registering that workflow in CTest and adding shutdown queue draining, the complete gate
passed **206/206**, zero skips, **404.03s**. The actual registered native export test passed in
**6.25s**, the core job test in **0.04s**, and the graphical export UI test in **0.04s**.
Minimal Shipping configure/build passed again. Hosted Linux display acceptance enables the Player
and requires all three export tests to be registered. Physical GPU and full deploy/log acceptance
remain separate Roadmap work.

Accepted Native Player main integration passed **208/208**, zero skips, **407.89s**, with
NEXORA_ENABLE_PROJECT_PLAYER_NATIVE enabled; Minimal Shipping passed. Hosted Linux Showcase
Shipping then exposed native window creation failure while short-lived xdotool clients probe
startup. Both shared Xvfb helpers allowed default last-client display reset. An actual native
root-property negative control proves that reset; -noreset preserves server state until owning
test cleanup. Both retained-display helpers pass that real regression without startup retry,
assertion removal or increased existing deadlines.

The first 209-test rerun retained a scene-file failure: Open acknowledged its new path, but the
following create shortcut did not reach the live document. The UI deliberately blocks it during
text input. Explicitly restoring Scene focus after path-dialog completion retains the original
live edit/save/Undo proof. Focused Showcase interaction, display lifetime and scene-file acceptance
passed **3/3 in 101.10s**. Final full graphical/native gate passed **209/209**, zero skips,
**413.40s**, and Minimal Shipping passed again. The registered native export still passes with
source and package identity preservation. Fresh hosted CI remains required before merge.
