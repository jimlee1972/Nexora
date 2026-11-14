# Main Game Player source/animation/pool services — Linux Cloud, 2026-11-14

Local final-source acceptance passes the full Linux Development gate: **540/540**, zero failures/skips, **2671.41 s**, all **14 sequential gates** and **13 actual GNU15 compiler commands**. Exact-head hosted CI is required before delivery.

`GameSpriteSession` augments the same main Runtime World and authoritative GameSceneSession with
owning mutable Sprite/collider state, a stable GameplayBridge and bounded pool. It prepares owning
candidate metadata against a read-only exact World scope before native admission, then swaps only
after World commit. Retained gameplay styles, source-frame clocks, pause/speed/Clip owners and
pool owner/index/generation survive scene transitions. Rejected candidates retain live values.
Destroyed descendants release metadata/animation/query state. The existing C/Zig tables, codecs,
package bytes and source-free/StaticView host table remain compatible. No automatic clock or
playback, Tilemap/content registration or Runtime dependency on TwoD is introduced.

The actual main Editor producer builds a canonical SDK script with two explicit Clip roots. The
project is hash-conserved during Build, then deleted. A Unicode-relocated zero-argument Player
negotiates actual optional 2D/source APIs and uses real D/W/S/A input across five phases: authored
source plus pooled Sprite, explicit frame/Atlas change, Additive, unload and Replace. Immutable
references never overwrite gameplay mutations. Exact independent masks total 65,536 expected
Sprite/pool pixels across those phases, with a stable second capture in each and an unchanged
central 80x80 3D region against a separately built source-free baseline. Pool identity stays exact;
old replaced entity IDs reject; real SDK Stop/Destroy destroys the pool before native drain.

Focused CPU coverage uses actual main Host negotiation and legacy prefixes on that same World,
real Runtime prepare/commit/rollback, source count-only probes, exact affine/style retention,
paused clocks, persistent scene replacement, pool handles, missing source startup rejection,
foreign World/owner-thread guards and authored-subtree destruction. The first test compile used
`UiVertex.x` instead of its actual `position[0]` field; that compile failure is retained separately.
The initial successful native generation takes 81.24 seconds. Complete final-source validation follows.

The previous generated player-reload-native artifact tree (2,477,377,233 bytes / 2719 files) was
archived byte-for-byte and verified against every original SHA-256 before reclaiming its generated
paths; all prior failure and acceptance evidence in that tree remains recoverable. The first archive check used
inefficient random seeks through gzip and was interrupted; a complete sequential verification passed.

The first frozen fifteen-source full attempt failed the existing
`editor.linux_display_acceptance` after 19.51 seconds at Save after cancelling an unsaved Close.
It was deliberately interrupted with exit 130 after 21 completed passes and one failure; it is
not a full acceptance result. The original fixture unconditionally deleted its project/user state
and killed the main Editor without collecting its streams, so that failing state is unavailable.
The partial logs, original fixture, fifteen source snapshots and binary hashes are archived.
An unchanged-binary diagnostic using the same native input sequence and deadlines passed; it
does not establish a product, input-routing or resource cause. `nexora-49s` remains open.

The display fixture now drains its sole binary reader during the original pauses and retains
every failed project, user state, both main streams, native commands and traceback. One real
Escape and one processed Ctrl+S gesture share the original five-second save bound, including
readiness and every key phase. It adds no retry, direct save API or deadline extension; saved
entities, Play steps, layout/camera persistence and asset identity remain unchanged. The focused
case passed 44.42 seconds. A separate intentional exception after the real Save gesture verified
the retained failure files; that expected fault injection is not a product failure or acceptance.
The final generation includes this harness as its sixteenth source file.

This scope adopts only the main native Player. Configured packaged Editor adoption, source
watching/publication, complete strict Editor/Player reload with 32 replacements and joint
ED-WF-07/08 acceptance remain open. Workflow **5/9**, graphical Editor **0/8**, TwoD **9/11** and
physical/clean-host acceptance stay unchanged. The root README remains concise and unchanged.

## Executed final source generation

- Full `cmake --preset linux-development`, all-target `cmake --build --preset linux-development` and unfiltered `ctest --preset linux-development --output-on-failure`: 540/540, zero failures/skips.
- Actual GNU15.2 compilation of thirteen command variants across twelve producer/consumer sources, including existing PlayContext CPU/native variants, main Editor/Player, TwoDPlayer, UI, new session and animation/generation consumers. Original warnings remain enabled.
- Full Monolithic Shipping build; actual canonical SDK/deleted-source relocated five-phase native services acceptance using the Development Editor producer/Shipping Player profile; existing static animation-root native Build/pixel regression also passes.
- Actual Monolithic graphical Editor and native Player build plus seven CPU cases for main host/session, bridge, generation, animation, PlayContext, existing export and Runtime scene transitions.
- Actual TwoD-OFF/testing-OFF graphical Editor/native Player build and capability-4 refusal on the new dynamic fixture's package.
- Metadata gates validate stable API exports, documentation utilities, ten touched C++ files, two Python files/Zig syntax/format, thirteen Markdown files/three bilingual pairs, sixteen frozen sources and unchanged root README.

The machine receipt records every gate command/log hash, actual compiler argv, both native
receipts/captures, SDK Build commands/inventories, full LastTest hash and exact source hashes.
No native Windows/macOS/Android/iOS execution is claimed from this Linux computer; hosted
platform checks are separate and all must pass before the agent's PR can merge.
