# Signed manifest/native image admission — Linux acceptance, 2026-10-10

This slice builds on the actual OpenSSL-backed Cryptography/ExtensionTrust change.
The Editor owns canonical manifests, signed packages, current admission policy and sealed image
descriptors; Runtime continues to own native mappings/cooperative lifecycle and service revocation.
No project/package input enrolls trusted publisher keys. Prepared values own bytes and do not grant
later authority without current trust/policy/signature/digest revalidation.

## Actual acceptance

Final desktop-Linux backend correction and accepted bounded-runner integration:
full enabled Linux Development configure/build/CTest **233/233**, no skips, **570.46 seconds**;
minimal Shipping **5 steps** passed. Exported DLL constants use separate declarations. Android
explicitly selects unavailable immutable-native admission instead of inheriting the desktop Linux
memfd branch through `__linux__`; no Android or other target-host execution is claimed.

- `editor.signed_extension_admission`: actual signatures generated with RFC8032's publicly published
  test seed; every-byte truncation, unknown schema, overflow, duplicate/unsorted dependencies,
  unsafe IDs, permission bits and budgets reject. Real artifacts/signatures/key rotation/policy/
  ABI/target/dependency rejection preserve native history and execute no fixture constructors.
- Real compiled A/B modules initialize/register distinctly from sealed owning bytes. Retained
  snapshots expose unique proc/fd identities; actual pwrite/ftruncate fail with EPERM and all four
  required seals exist. Mutation of caller package copies cannot change admitted bytes.
- Cooperative unload removes service visibility, invokes actual module destruction and closes the
  image descriptor; trust/policy changes revoke/unload. Reload and 128 lifetime admissions succeed;
  the next admission neither initializes nor alters events. A trusted dishonest binary explicitly
  demonstrates post-admission ABI detection after OS initialization; this is not a pre-execution
  native-code inspection claim.
- Full enabled `linux-development` configure/build/CTest: **227/227**, no skipped tests,
  **547.96 seconds**; graphical shell, Slang, Zig, Showcase and native Project Player enabled.
- Minimal `linux-shipping` configure/build: **5 steps**, Editor/Cryptography stripped as intended.
- Explicit unavailable backend configure/build/test: **3/3**, **0.03 seconds**, proves fail-closed
  behavior. Actual OpenSSL native admission focus: **1/1**, **0.08 seconds** before the full gate. Final fixture lifetime ordering additionally passed 1/1 in 0.07 seconds.

The native code/dependencies/host linker environment remain trusted in-process components;
declared permissions are admission policy, not a sandbox. Other-platform immutable loading,
graphical installation/recovery and complete ED-M6/7 acceptance remain open. Progress stays **0/8**.

## Accepted build-console and inspector integration

On Crypto PR #477 head `6e6a419fbfca187da061123f66cc4ea5e452d8cb` and
accepted main `638e982e64a937d97d94c804a1becb4b2ed55301`, the admission
implementation and fixtures are unchanged from the full-validated version above.
The rebuilt Editor and affected test targets completed 33 steps; **9/9** focused
tests passed in **22.81 s**. These include actual Ed25519/SHA256, trust, native
admission, bounded processes, reflected Core/UI/native controls and real native
build-console interaction.

The dependent graphical manager integrates this same admission code. Its full
graphical Linux Development gate passed **241/241**, zero skips, **620.96 s**,
after 320 build steps; minimal Shipping built successfully (5 steps). This is
dependent-tree integration evidence, not a separate full run of the admission-only
tree. Published-head hosted checks remain independently required.

## Fresh accepted Main admission integration

The four owned admission commits were replayed onto accepted Main
`5f5079cec33e8f788e0ff10cb6cf5fe2779abc79`; its accepted native pointer readiness
changes supersede the earlier duplicated pointer commit. No prerequisite branch
was merged. Graphical Development rebuilt **218 steps**, and full CTest passed
**241/241 in 583.49 s**, zero skips. Real signed admission repeated successfully
in **0.07 s**. Minimal Monolithic Shipping passed **5 steps**, with Editor excluded
as expected. This is the admission feature itself, rather than only its dependent
manager composition. Contracts and bilingual supporting roadmap scope remain unchanged.
Fresh published-head hosted checks and clean current Main are still required.
