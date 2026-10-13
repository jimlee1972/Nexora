# Play Pause focus admission — Linux cloud, 2026-10-13

Beads: `nexora-rd7.2.5`. Base: `d757c7a3feeff8e6bdd434f6807d7de38fa0c955`.

An actual Runtime counterfactual on the base reports `after_pause=1 while_paused=1 after_resume=1`:
Pause retained input focus, paused sessions accepted a new grant, and Resume retained it. The native
UI forwarding adapter masked this portable Runtime policy gap by explicitly revoking its own input.

Pause now releases Runtime focus; grants and `AcceptsInput` require Playing. Resume leaves focus
released until explicit reacquisition. Manual Step still executes one fixed callback while paused;
tick counts, apply-back and isolated World ownership are unchanged. Debugger-break and Runtime-failure
recovery obey the same admission rule. This is a stricter public C++ Runtime input policy, without a
layout, C ABI, module dependency or linkage boundary change. Caller-owned device buffers still require
their own release, which the native forwarding adapter performs when paused.

The existing Runtime M6 fixture checks Pause/Step focus release, rejection of paused grants, Resume
without focus, explicit running reacquisition, debugger/failure recovery and unchanged authoring.
The actual Gameplay V3 fixture checks paused explicit focus admission and neutral forwarding without
a GUI frame. No existing assertion or warning gate is removed.

## Validation

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

Configure and the 420-action build passed. The complete Linux Development suite passed **244/244**,
zero failures/skips, in **820.17s**, including Runtime M6, actual Gameplay V3 callbacks and native Game
input. The temporary harness against the fixed Runtime/header reports
`after_pause=0 while_paused=0 after_resume=0`; the base counterfactual reports all three as 1.

The four changed C++ files passed clang-format dry-run; documentation routing passed16/16 and changed
Markdown links/bilingual pairs passed. Existing simulation/Undo/authoring conservation assertions and
warning gates remain intact. No linkage boundary changes require Shipping for this patch.

Windows/macOS and physical-device acceptance remain separate host workflows; fresh hosted PR status
is recorded in the PR and Beads. Graphical milestone acceptance stays0/8; the root README is unchanged.
