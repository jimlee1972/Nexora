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

`Vector4`'s `Dot`/`Length`/`NormalizeSafe`/`Lerp` share their names with `Vector3`'s overloads of the
same operation, but are declared as function templates constrained to exactly `Vector4`
(`template <typename T> requires std::is_same_v<T, Vector4>`) rather than plain `Vector4` overloads.
A plain overload would be ambiguous for a bare brace-init-list call such as `Dot({1, 2, 3},
{4, 5, 6})`, since `Vector3` and `Vector4` are both aggregates and a 3-of-4-members initializer list
is an equally good conversion to either (the missing `Vector4::w` is simply value-initialized) --
which would silently break the brace-init calling convention this file already relies on elsewhere.
Template argument deduction is never attempted from a bare braced-init-list against a plain
type-template parameter, so the `Vector4` templates simply are not viable candidates for such a
call and the ambiguity never arises; called with an already-typed `Vector4` (the common case for
this kind of API), they bind normally. `Dot` dispatches to an SSE2 implementation when
`NEXORA_MATH_HAS_SSE2` is set (any x86/x64 target with SSE2, which is baseline on x86-64), falling
back to the plain scalar reduction otherwise; both paths are exposed as
`detail::DotSimd`/`detail::DotScalar` and cross-checked by a 10,000-iteration fuzz test in
`Tests/API/ApiFoundationTests.cpp` (tolerance scaled to the sum of `|component product|` magnitudes,
not to the final dot value, since catastrophic cancellation can drive that value near zero while
each term still carries real floating-point error). **Not done**: the SIMD path covers only
`Dot(Vector4, Vector4)` -- `Vector3`'s dot/cross, `Matrix3`/`Matrix4` multiplication, and every
other operation in this file remain scalar-only, and the SSE2 path itself is unverified on ARM/NEON
(it simply falls back to scalar there via the `NEXORA_MATH_HAS_SSE2` guard, which is correct but
untested since this sandbox has no ARM target). `Quaternion::NormalizeSafe` and `Vector3`'s
one-argument `NormalizeSafe(Vector3)` (default fallback = a zero vector) are constrained templates
for the same reason as `Vector4`'s: a bare `NormalizeSafe({1, 2, 3})` was ambiguous between them
(`Quaternion::w` defaults to `1.0F`, so a 3-element list aggregate-inits either type), and neither
template is deducible from a bare braced-init-list, so that call now fails to compile outright (a
clear "no matching function", not a silent pick or an ambiguity) instead of being accepted by
whichever type happened to win. `Vector3`'s **two**-argument `NormalizeSafe(Vector3, Vector3)`
stays a plain, non-template overload: that arity was never ambiguous with `Quaternion`'s
one-argument overload in the first place (arity alone rules it out), so templating it too would
only have broken existing bare-brace two-argument calls like
`NormalizeSafe({3, 4, 0}, {0, 1, 0})` for no benefit -- exactly the regression a review caught when
the whole two-argument overload was made a template.

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
