# Prefab rebase local-label compiler repair — 2026-10-13

Beads: `nexora-nqn.13`. Base: `be2273c927181688f340392feb140f0feca086b1`.

## Observed failure

PR569 and PR570 push/PR Windows Development jobs all failed during EditorImGui compilation:
C2220 promotes C4456 under `/W4 /WX`; `PrefabIsolationPanel.inl:639` declares a choice `label`
inside the scope of the existing row `label` at567. The four immutable failed jobs are
114939900982,114940094653,114944664698 and114944937390. Tests did not run in those jobs.
Their corresponding Linux/macOS, display, packaging, linkage and other selected jobs passed;
this does not replace a fresh Windows gate for the repair.

Rename the inner local to `choice_label` and its BeginCombo use. No widget string/ID, decision,
consent guard, document ownership, history, persistence, ABI or dependency changes. Keep warning
admission unchanged and exercise the existing suites rather than adding a name-mirroring test.
Root README remains unchanged; overall graphical target acceptance remains0/8.

## Validation

The required complete Linux gate passed **244/244**, zero failures/skips, in **822.86s**:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

Native Prefab isolation/rebase/override input, Console, Play users and all prior regressions passed.
No linkage boundary changed, so a new local Shipping build was not required for this lexical fix.
The changed `.inl` passed clang-format; documentation routing passed16/16.

Using the actual compilation-database definitions/includes for the complete EditorImGui translation
unit, Clang19 `-fsyntax-only -Wshadow -Werror=shadow` passed for the patched source. Substituting
only the immutable original panel source reproduced the error at639 and prior declaration567.
This counterfactual demonstrates the lexical failure and repair without changing warning policy.
It is a Linux compiler check, not a local Windows/MSVC execution claim.

Fresh hosted Windows/all-selected-job results and merge identity are tracked in the repair PR and
Beads. Historical PR569/570 Windows compile failures remain failures and are not rewritten as passes.


Validated panel SHA256: `8c810ae5e19400600be213c6de304d7495b3daa77629a80f750d4c114ffdee0e`.
