# Nexora Foundation public value API

## Math and geometry (API-M1)

`Nexora/Math/Math.h` defines the allocation-free math and geometry surface. Coordinates are
right-handed and Y-up. Matrices use row-major storage, multiply column vectors, and compose as
`parent * child`; angle-bearing functions state `Radians` in their names (a function taking no
angle, such as `LookAt` or `Orthographic`, carries no such suffix). `NormalizeSafe` returns the
supplied fallback for zero-length or non-finite inputs; `Matrix3::InverseSafe` and
`Matrix4::InverseSafe` follow the same epsilon-fallback policy for a singular matrix instead of
propagating NaN/Inf. Geometry boundary tests are inclusive.

`Decompose` is the inverse of `Compose` (translation from the last column, scale from each basis
column's length, rotation recovered via Shepperd's method); it assumes no shear, since `Compose`
cannot produce any. `ExtractFrustum` derives the six frustum planes from a row-major, D3D-style
`[0,1]`-depth view-projection matrix (Gribb-Hartmann); `Intersects(Frustum, Aabb)` tests the box's
positive vertex per plane and so is conservative (may accept a box only touching a far corner) --
it is not full separating-axis culling.

The ABI layout gate (`sizeof`/`alignof`/`offsetof` for every POD type here, plus a byte-level
`memcpy` round trip proving no padding holes or hidden state) lives in
`Tests/API/ApiFoundationTests.cpp`, not in this module: Foundation intentionally carries no test
dependency of its own.

`Vector4`'s `Dot`/`Length`/`NormalizeSafe`/`Lerp` share names with the `Vector3`
operations, but use templates constrained to exactly `Vector4`. This preserves unambiguous legacy
brace-initializer calls for `Vector3`; typed `Vector4` calls bind normally. `Dot` and `Matrix4`
multiplication select SSE2 on x86/x64 and NEON on ARM, with portable scalar references kept in
`detail` for deterministic randomized tolerance tests. AArch64 provides NEON as part of its base
architecture; 32-bit ARM uses the path only when the compiler advertises NEON. Unsupported targets
retain identical scalar semantics.

The coordinate convention is independently gated with constants generated from DirectXMath's
right-handed look-at and perspective functions and transposed from its row-vector convention. The
tests compare Nexora output directly to those constants rather than deriving expected values with
Nexora helpers, preventing a consistent sign, handedness, depth-range, or layout regression from
self-passing. Linux x86-64 exercises SSE2 in the cloud gate; NEON implementation coverage is present
but runtime execution remains a target-host validation responsibility.

`Quaternion::NormalizeSafe` and `Vector3`'s one-argument `NormalizeSafe(Vector3)` are constrained
templates for the same brace-initializer ambiguity reason. The two-argument `Vector3` overload stays
a plain overload because its arity is unambiguous and existing brace-initializer calls remain valid.

`Dot4`/`Length4`/`NormalizeSafe4`/`Lerp4` also still exist, as thin non-template forwarding
wrappers over the templated names above, kept for any caller that adopted that spelling during the
brief window it was the only one available.

## Foundation types (API-M2)

`Nexora/Foundation/Types.h` provides UTF-8 validation, byte-oriented string views, stable FNV-1a
names, UUID values (with `Parse`/`ToString` for the canonical `8-4-4-4-12` hex form), spans, byte
buffers, locale-independent numeric parsing, and a small `Result<T>` error carrier. Views never
own their input. Embedded NUL is preserved as a byte; validation checks encoding only. C++
containers own their memory and must be created and destroyed in the same C++ runtime module.

`Nexora/Foundation/DataAbi.h` is the stable C crossing point for text and bytes. Its pointer-length
views do not imply NUL termination. Callers can either copy into their own buffer after a required-
size query, or receive an opaque Foundation allocation and return it through
`nexora_foundation_buffer_destroy`; the latter's borrowed view expires when its handle is destroyed.
All functions return explicit result codes, reject a null pointer with nonzero length, and the text
constructor validates UTF-8 while preserving embedded NUL. The ABI version macro is
`NEXORA_FOUNDATION_DATA_ABI_VERSION`.

The generational-handle deliverable is `Nexora/Core/Handle.h`'s `Handle<Tag>`/`HandlePool<Tag>`,
already used by `EventBus`, `Timer`, and the RHI backends; it is not duplicated here since Core
depends on Foundation, not the reverse. `Name` stores only a 64-bit FNV-1a hash, not
the source string, as a deliberate ABI-stable-StringId trade-off -- two distinct strings that hash
identically are indistinguishable; see the class's doc comment and the API-M2 hash-collision test.
The API-M2 tests cover malformed UTF-8, embedded NUL, caller-buffer sizing without partial writes,
opaque allocation/destruction across the Foundation module boundary, UUID parsing, locale-neutral
number parsing, and collision diagnostics. The API sample also consumes the C data ABI.
