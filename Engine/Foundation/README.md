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

## Foundation types (API-M2)

`Nexora/Foundation/Types.h` provides UTF-8 validation, byte-oriented string views, stable FNV-1a
names, UUID values (with `Parse`/`ToString` for the canonical `8-4-4-4-12` hex form), spans, byte
buffers, locale-independent numeric parsing, and a small `Result<T>` error carrier. Views never
own their input. Embedded NUL is preserved as a byte; validation checks encoding only. Containers
own their memory and must be created and destroyed in the same C++ runtime module; they are not a
stable C ABI -- that is a deliberate scoping of this file to the C++ convenience layer, not a gap:
the roadmap's opaque-handle/POD/versioned-function-table C ABI rules apply to the actual crossing
points (see `Nexora/Core/Handle.h`'s `Handle<Tag>`/`HandlePool<Tag>`, already used by `EventBus`,
`Timer`, and the RHI backends, for the generational-handle deliverable; it is not duplicated here
since Core depends on Foundation, not the reverse). `Name` stores only a 64-bit FNV-1a hash, not
the source string, as a deliberate ABI-stable-StringId trade-off -- two distinct strings that hash
identically are indistinguishable; see the class's doc comment and the API-M2 hash-collision test.

**Not yet implemented**: a caller-buffer or engine-owned-opaque-buffer C ABI variant of
`String`/`ByteBuffer`/`Span` for cases that need those specific containers (not just a handle or
POD) to cross the ABI boundary. Nothing in the engine currently needs that, so it has not been
built ahead of a real use.
