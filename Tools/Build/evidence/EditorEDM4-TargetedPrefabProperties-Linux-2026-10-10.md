# ED-M4 targeted stable-field prefab property candidates — Linux, 2026-10-10

Beads: `nexora-pmb.1.12`. Validated on main `64b20ea7` and pending full
property-plan commit `37f21bdb`. Read-only candidate planning adds no source
publication or live mutation authority; graphical binding is separate.

Graphical Development configure/build passed (12 initial focused and 195 full
incremental steps). Final focused property-plan tests passed **1/1**, **0.03 s**.
Full Development tests passed **239/239**, zero skips, **596.67 s**. Minimal
Monolithic Shipping configure/build passed (5 steps).

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_CRYPTOGRAPHY=ON -DNEXORA_ENABLE_SLANG=ON \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON \
  -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
```

Actual independent Worlds allocate different Runtime IDs while stable node/field
identities match. Mixed name/position/Camera/opaque selection restores only selected
values, retaining current scale, authored rotation, hierarchy, names and unrelated
components. A disabled source Camera keeps stored FOV75, rather than resetting it
through a disable setter. Exact unknown bytes/type names restore independently;
selected absent-source unknown components are removed while unselected payloads
remain byte-identical. Rotation-only selection retains authored Euler720 while
position/scale stay current. Parent-only selection retains local transforms.

A selected parent subset that would create a cycle rejects as a whole; selecting
both compatible parent groups succeeds after staged detachment/reparent/order.
Every planning/rejection leaves live sources unchanged. Duplicate, unknown and
reserved metadata identities reject; empty selection is an exact canonical no-op.
Existing full-candidate corrupt/shape/identity tests remain intact.

Both complete sources validate before selection. At most4096 fully tracked nodes,
32768 selections and8MiB scene/runtime/output buffers are allowed; these are
logical bounds, not total allocator/process/history memory quotas. Canonical
Runtime version-three property groups preserve complete inactive stored data;
unknown record framing fails closed. Metadata/opaque bytes are never interpreted
as native objects. Official final document parsing/preparation validates the
candidate again. Target scene identity and entity IDs survive. Existing shared
field identities must agree; a missing field can be selected using its known
identity in the other source. Graphical selection, source apply, structural
identity reconciliation and reference rebase remain open. Public C++ consumers
rebuild; stable C/Gameplay ABI is unchanged. Full Editor milestones remain**0/8**.
Rebase to accepted main and require all current hosted checks before merge.
