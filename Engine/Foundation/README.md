# Nexora Foundation public value API

## Math and geometry (API-M1)

`Nexora/Math/Math.h` defines the allocation-free math and geometry surface. Coordinates are
right-handed and Y-up. Matrices use row-major storage, multiply column vectors, and compose as
`parent * child`; angle-bearing functions state `Radians` in their names. `NormalizeSafe` returns
the supplied fallback for zero-length or non-finite inputs. Geometry boundary tests are inclusive.

## Foundation types (API-M2)

`Nexora/Foundation/Types.h` provides UTF-8 validation, byte-oriented string views, stable FNV-1a
names, UUID values, spans, byte buffers, locale-independent numeric parsing, and a small
`Result<T>` error carrier. Views never own their input. Embedded NUL is preserved as a byte;
validation checks encoding only. Containers own their memory and must be created and destroyed in
the same C++ runtime module; they are not a stable C ABI.
