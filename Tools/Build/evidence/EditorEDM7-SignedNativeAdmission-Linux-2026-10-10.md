# Signed manifest/native image admission — Linux acceptance, 2026-10-10

This slice builds on the actual OpenSSL-backed Cryptography/ExtensionTrust change.
The Editor owns canonical manifests, signed packages, current admission policy and sealed image
descriptors; Runtime continues to own native mappings/cooperative lifecycle and service revocation.
No project/package input enrolls trusted publisher keys. Prepared values own bytes and do not grant
later authority without current trust/policy/signature/digest revalidation.

## Actual acceptance

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
