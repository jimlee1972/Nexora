# Dormant component presence-edit history — Linux, 2026-10-10

Beads `nexora-owg.2.6`; final source builds on accepted Main
`5ecb6871319449581739da8c5770d5d96c0ca3f8`.

Previously, Editor Camera/Light/Mesh presence-edit Undo captured an absent component
as an empty optional, losing its nondefault inactive stored values when replayed.
Restore templates now own both all stored values and the original presence flag.
Only SceneEditor can set the private command-presence override. Ordinary public
WorldCommandBuffer removal still clears values to defaults; Redo repeats the same
committed removal. Mesh uses the existing immutable atomic command-template history,
matching Camera/Light. No whole-scene snapshot or new scene-size limit is introduced.

Actual three-node mixed active/inactive fixtures include lens91/90/89 with .2/950,
intensity3.5/4.5/5.5 and full-width mesh/shader references. Each component batch
survives 40 exact-byte Undo/Redo cycles and final Undo. Absent single-component no-ops
preserve pending Redo and dormant values. Invalid Camera/Light, stale/duplicate Mesh,
selection, generation keys, unavailable opaque data, authored Euler720 and clipboard
are checked. A destroyed replay target rejects without changing remaining World bytes
or the history cursor. The fixture wraps a real Runtime3 capture in canonical Editor3
node/world metadata; an initial incorrect raw-Runtime adoption failure is excluded.

Cold graphical Linux Development configure/build passed (513 steps). After the
fixture-only correction, final configure/build passed (two steps), focused component
reset **1/1 passed,0.02s**, and full `ctest --preset linux-development` **240/240
passed,595.33s**, zero failures or skips. Graphical shell, Cryptography/OpenSSL,
Slang, Zig, Showcase and native ProjectPlayer were enabled. Minimal Monolithic
`linux-shipping` configure/build passed (74 steps, Editor stripped). Changed Markdown
validation and the 16-test documentation regression suite pass.

Public C++ consumers rebuild after private command layout changes. Stable C/Gameplay
ABI, module dependencies and serialized formats remain unchanged. Calls retain the
serialized World authoring-thread and atomic replay contract. Editor milestones stay
**0/8**; no physical display or other-platform acceptance is claimed.

## Fresh accepted Main integration

The owned component-history correction was replayed onto accepted Main
`5f5079cec33e8f788e0ff10cb6cf5fe2779abc79`. Graphical Development rebuilt
**350 steps**, then full CTest passed **240/240 in 587.64 s**, zero skips.
The component reset fixture repeated in **0.02 s** and Inspector reset in **0.13 s**.
Minimal Monolithic Shipping rebuilt **14 steps**, including Runtime and AIIntegration,
with Editor excluded. Public World removal defaults remain unchanged; only Editor
history restoration uses the private presence override. No prerequisite branch was
merged. Fresh published-head hosted checks and a clean current Main remain required.
