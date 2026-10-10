# ED-M7 actual extension signature verification — Linux, 2026-10-10

Beads: nexora-0ua.4.2. Base: accepted main 0df2a1fa. This is an actual cryptographic
prerequisite; the whole trust/installation milestone remains open.

## Full and focused validation

Full graphical Linux Development, Slang, Zig gameplay, Showcase and native ProjectPlayer,
with explicit OPENSSL backend: configure/build succeeded (419 remaining build steps);
`ctest --preset linux-development` passed 226/226 in 541.17s, zero skips. Minimal
`linux-shipping` configure/build succeeded (74 steps); Editor and default Cryptography are off.

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON -DNEXORA_CRYPTOGRAPHY_BACKEND=OPENSSL
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
```

The cloud reused independently verified unchanged pinned ImGui/Vulkan-Headers sources using
FetchContent source-directory overrides after public dependency fetch failed. No pins changed.
OpenSSL 3.5.7 was the installed cloud provider; no cryptographic source/private key was downloaded.

Actual crypto/trust focused tests passed 2/2 in 0.33s. Independent RFC8032 section 7.1 empty and
one-byte Ed25519 vectors plus known SHA256 empty/abc vectors test provider behavior; tampered
key/signature/payload, unknown publisher, configuration rejection, rotation/revocation, owning
snapshot copies, 64 publishers and full input bounds reject or preserve state appropriately.
The exact 64 MiB boundary uses the published RFC fixture seed through EVP signing; it is public
fixture material, not a user credential. No hand-written cryptographic implementation is used.

Unavailable-backend configuration/build and tests passed 2/2 in 0.03s:

```sh
cmake --preset linux-development -B build/crypto-none -DNEXORA_CRYPTOGRAPHY_BACKEND=NONE -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF -DNEXORA_ENABLE_NATIVE_BACKENDS=OFF
cmake --build build/crypto-none --target NexoraCryptographyTests NexoraExtensionTrustTests -j4
ctest --test-dir build/crypto-none -R '^(cryptography.ed25519_sha256|editor.extension_signature_trust)$' --output-on-failure
```

Standalone Monolithic Shipping explicitly enabled Cryptography and tests with Editor off:

```sh
cmake --preset linux-shipping -B build/crypto-shipping -DBUILD_TESTING=ON -DNEXORA_ENABLE_CRYPTOGRAPHY=ON -DNEXORA_CRYPTOGRAPHY_BACKEND=OPENSSL
cmake --build build/crypto-shipping --target NexoraCryptographyTests -j4
ctest --test-dir build/crypto-shipping -R '^cryptography.ed25519_sha256$' --output-on-failure
```

The real crypto test passed 1/1 in 0.31s. This verifies static linkage, not Editor UI in Shipping.
The Linux display CI explicitly installs libssl-dev, requests OPENSSL and requires backend plus
both tests, preventing unavailable mode from substituting for actual verification.

## Ownership and limits

Crypto calls borrow valid spans only synchronously (maximum message 64 MiB), retain nothing and
return owning results. The trust registry owns at most 64 caller-configured public keys with
bounded ASCII publisher IDs; all registry calls serialize. Metadata changes advance a nonwrapping
revision, and successful verification includes that revision plus a SHA256 digest. Results are
observations rather than load authority. Source/artifact inputs must stay immutable for the call.

AUTO native discovery may select unavailable; NONE always rejects verification. Cross-compiling
AUTO never links a discovered host OpenSSL. Explicit OPENSSL requires the target package; provider
or algorithm failure rejects. No Windows/macOS/mobile cryptographic-provider execution is claimed
from this Linux evidence. Final published-head hosted checks are independently required for merge.

ExtensionPolicy's legacy boolean predicate remains a portable primitive. PluginHost is not wired
to this new verifier: signed manifest/ABI/permission/dependency validation, immutable native staging,
revision recheck and actual pre-load enforcement still require implementation. No private key store,
network telemetry, installation UI or native crash isolation is added. Full milestones stay 0/8.

## Integration with accepted inspector and build console

On main `638e982e64a937d97d94c804a1becb4b2ed55301`, the full graphical Linux
Development build completed (286 steps), and **237/237** tests passed with zero
skips in **591.45 s**. Minimal Monolithic Shipping built successfully (14 steps).
The real reflected Inspector Xvfb/xdotool interaction additionally passed eight
consecutive runs (**103.66 s** total). Pointer movement now receives a hovered
frame before the separate click event, preserving all original source-byte,
Undo/Redo, read-only, restart and validation assertions. This corrects the native
test input timing exposed by hosted Linux CI, without changing production code.
Final published-head hosted checks remain required for merge.
