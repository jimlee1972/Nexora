# Typed zero for the occlusion native fixture

Use `0.0F` when filling a float projection matrix. MSVC instantiates the integer version of `std::fill` for `0` and reports C4244 as an error. This changes only the test initializer type; the matrix values and AO runtime are unchanged. Full Linux configure/build and 104/104 tests pass (134.11 seconds, core/sync validation) in the authoring tree, including the separately verified landmark-cypress art. The corrected PR must pass Windows CI before merge.
