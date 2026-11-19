# ✅ Preserve keyboard press modifiers — Linux qualification

The unchanged native Plugin Manager case fails in the six-input full gate (560/561, 7133.51 s):
Ctrl+Alt+E changes the Scene tool to Rotate and never opens Extensions. The original logs and failed
project are retained before changes. A subsequent diagnostic uses the original 100-ms shortcut and
12-second operation deadline with an external X11/GUI observer; instrumentation affects scheduling
and supplies causal evidence only, never acceptance.

The actual application receives E-down with X11 modifiers12 and Window modifiers5. The next GUI
frame consumes E-down together with later Control/Alt releases: E is newly pressed while GUI
modifiers are zero. The global shortcut declines and the ordinary E tool activates. This is a
product batching defect. Changing the input producer to hold modifiers longer was rejected.

The UI owner now admits a queue prefix ending before a modifier transition that follows a new
keyboard press. Unconsumed vendor input stays ahead of the untouched tail in the existing owned
queue. Event order/IDs and vendor code remain unchanged. The helper respects production trickling,
focus cancellation and disabled-keyboard behavior; text/modal/permission admission remains in the
existing command paths. See the [host contract](../../../../Engine/EditorImGui/README.md#modifier-preserving-input-frames).

An unchanged external probe ELF fails 24 partitions against the old runtime, then stops on the
plain-E negative guard before 2x runs; against the corrected runtime it passes all **128** scenarios
(32 poll/frame partitions × 1x/2x × writer/read-only), including plain E before later modifiers.
The registered regression additionally checks focus loss and an active text field. Standard focused
CTest runs without observers/preload and passes **3/3 in 112.67 s**: original Plugin Manager 34.80 s,
Scene input 13.36 s and modifier regression 64.48 s. The native fixture, physical gesture and deadlines
remain byte-identical. The final eleven-source Linux gate passes **562/562 in 7170.52 s**,
zero failures/skips/retries, after configure/build. Its original Plugin Manager case passes in 40.87 s,
modifier regression passes in 65.75 s and changed Scene/Project widgets 15.48 s. Final-head review and own-PR
merge remain required.

See the [structured evidence](Modifier-Chord-Linux.json). This does not establish the cause of
historical pointer or F5 failures, and does not raise complete Editor acceptance.
