# Runnable counted reflection SDK consumer — Linux, 2026-10-21

Beads `nexora-gu7.36.1`, supporting M6-07. Broader SDK/plugin operation/restart/backend documentation
and physical acceptance remain open.

The repository previously exposes counted reflection through tests/GUI only; Samples/Api contains
the Foundation consumer. `Samples/Editor/ReflectedArraySample.cpp` now uses public Editor headers
in an actual conditional executable. `EditorApiSamples` declares its Editor dependency and the
feature follows Editor availability; it does not require the graphical Shell. Shipping/global
headless stripping produces no target or test, with an explicit status for retained ON requests.

The window-free executable creates its own temporary writer project and Editor World, stores a
version-two portable count/tail layout alongside reserved bytes and another unavailable provider,
then executes authorized Append, element edit and Move. Real authority denial and stale source
observation rejection retain the previous bytes/history. Move is one Undo/Redo. A real scoped
SceneFileSession Save and a separate fresh read-only workspace/World reopen preserve the complete
expected count/tail/prefix and unrelated provider bytes. It loads no extension or native component
and introduces no service, upload, arbitrary output path or new stored format. Cleanup follows
owner/file-lock release. The README documents version, lifetime, threading, caller authority,
errors, rebuild and missing-backend behavior; native provider restoration is not claimed.

Initial actual sample target build succeeds in13 steps. The registered executable passes in.02 s.
Shipping configure/build passes nine integration steps over the prior full minimal product;
Editor and the sample target are absent and CTest registers no tests. This does not certify the
sample in an Editor-enabled Shipping configuration.

Required final `cmake --preset linux-development`, `cmake --build --preset linux-development`
and serial `ctest --preset linux-development` pass: **312 registered,300 actual passes,12
environment skips,zero failures,866.37 s** after327 full-product build steps. All62 frozen
combined code/test/helper source hashes match. Deep hierarchy/100k lifecycle/overview pass
63.70/287.15/73.92 s under unchanged limits and scale1. The 100k/overview run overlapped a
separate bounded target compilation; these are shared-cloud functional observations, not isolated
performance baselines. Bare local display/native cases remain skipped, with no physical claim.

The registered sample passes standalone in.02 s and together with the module graph2/2 in.07 s.
An actual Shell-disabled configure/build (no work) and registered sample pass in.30 s; Shell is
restored ON for the full gate. Shipping target enumeration confirms the sample target is absent,
with no Shipping CTests. `--help` returns0 without project creation. Documentation validation
initially passes16/16 in1.614 s; final documentation and exact-head hosted checks follow separately.
