# MSVC fixture literal follow-up

The point-light fill and, when present, two-sided normal fixtures now use explicit floating
constants to avoid MSVC /WX C4244 warnings. Values and renderer behavior are unchanged.
✅ Follow-up Linux configure/build and all 97 tests pass (102.62 seconds), including 85
native PBR frames with Khronos core/synchronization validation. Exact fixture hashes and
logs are retained here. This is a test-only compiler correction; the existing Shipping
package/movie remain tied to their recorded production freeze. Windows CI recheck is pending.
