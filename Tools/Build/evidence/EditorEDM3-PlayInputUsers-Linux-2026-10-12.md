# Logical graphical Play input users — Linux cloud, 2026-10-12

Beads: `nexora-rd7.2.3`. Base: `11d7a1a98f86903a613096f54b32f0a79fb47c43`.

One native keyboard/mouse source is explicitly routed to a selected logical user0–3 through the
existing Gameplay V3 capture_input ABI. No struct layout, declaration or capability bit changes.
Game selection is session-only and releases capture; host selection clears held state and preserves
acquisition discard. Unselected users receive neutral owning snapshots with equal sequence. Invalid
capture preserves output, invalid forwarding releases focus, and scope/unload reset identities.
Existing profiles remain shared; no user selection is written into project settings.

Executable tests exercise actual rendered selector clicks, unchanged/change capture transitions,
callback lane separation, invalid/null calls, copied snapshot lifetime, default-user compatibility
and focus revocation before any GUI frame. Existing native user-zero Game fixtures remain regression
gates. The new logical selection is proved by rendered ImGui IO plus actual host callbacks, not a
claim of simultaneously attached physical users or a new gamepad/relative-pointer backend.
Full ED-M3 and target-host acceptance remain open.

## Executed validation

The complete Linux Development gate passed **244/244**, no failures/skips, in **834.59s**:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

Seven focused checks passed7/7 in12.60s: Play host callbacks, rendered input selector/focus,
native Game view, actual native input and project profiles, camera selection, and1x/2x/read-only
binding scopes. Full-suite native input and project input passed again. The first selector fixture
read unsettled popup geometry; the established post-release settling frame corrected the fixture
while preserving all guards and assertions. Detach and unload reset tests passed.

Shipping Monolithic configure/build passed with Editor stripped and testing OFF:

```bash
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Ten changed C++ files passed clang-format dry-run; documentation routing passed16/16. GCC14.2,
Dear ImGui, Xvfb and Mesa lavapipe used the existing external SDK paths. Windows/macOS/physical
multi-device acceptance did not run locally. Root README and the stable C ABI headers are unchanged.
Hosted workflow status is recorded separately in the PR and Beads.

## Validated source digests


- `Apps/Editor/PlayGameplayModule.h`: `c0d1d43f4236d724331fa51bb1642d92c514a31e9179799b44714600ccfdebe9`

- `Apps/Editor/PlayInputForwarding.h`: `413f30efd854bec0c7b7b689883ed321eabf7f7e4c8feb4e43c1bb53b0e135ee`

- `Engine/EditorImGui/src/EditorImGui.cpp`: `3f1e2ae8e810922d08583f249598e7c7aeeb6b6b00bb5e5cb87ae5794466ec51`

- `Tests/EditorImGui/PlayGameplayModuleTests.cpp`: `c844e6ee9b99010c7451aa000f860122664b5a9abd0fed76fda2f9221cefbe31`

- `Tests/EditorImGui/PlayInputStateTests.cpp`: `48d645746af96f5b58c634f039ec1539543ac8bb9eb7c7ea9838c2892dc4b6cc`

- `Tests/EditorImGui/GameInputBindingsTests.cpp`: `fc4afda77e92e0cf2c4d00a663969540a087a7dab6aeb8e4ca60f6bfc6317198`
