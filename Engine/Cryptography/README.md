# Cryptography

The optional `NEXORA_ENABLE_CRYPTOGRAPHY` module depends only on Foundation. Editor declares
this dependency in Config/Modules/modules.json and enables it by default; explicit disabled
Cryptography with enabled Editor rejects configuration. Fresh Shipping/headless profiles
default it off when Editor is off. Standalone hosts can explicitly enable it.

`NEXORA_CRYPTOGRAPHY_BACKEND` selects AUTO, OPENSSL or NONE. OPENSSL requires an installed
OpenSSL >=3.0 Crypto package; AUTO selects the available native package or an unavailable provider.
Cross-compiling AUTO always selects unavailable: an explicit OPENSSL backend and target-platform
package configuration are required to avoid linking a host crypto library into target binaries.
NONE always fails closed even when OpenSSL is installed. No cryptographic source is downloaded.
OpenSSL::Crypto is a private external link dependency, declared in module metadata. Its types
never cross public headers. Public C++ consumers rebuild; no stable C ABI is added.

VerifyEd25519 validates a raw 32-byte public key, raw 64-byte pure Ed25519 signature and at most
64 MiB message before invoking the vetted provider. It is synchronous/stateless, never retains
spans and permits concurrent calls with independently valid inputs. The caller selects trusted
public keys and retains immutable input bytes for the call. Empty messages are supported.
Verified, invalid shape/budget, invalid signature, backend unavailable and backend failure are
separate outcomes; absence/failure never accepts a signature. This is not key generation, key
storage, certificate validation, signed-manifest policy, artifact staging or a native loader.

Sha256 returns an owning 32-byte digest with the same input bound, or no result on unavailable
provider, over-budget input or provider failure. It does not hash files implicitly, persist data,
log input or read network/environment configuration. OpenSSL's platform/library setup remains
host configuration. Installed library availability does not guarantee algorithm availability
under every provider/FIPS policy; such operational failures reject verification.
