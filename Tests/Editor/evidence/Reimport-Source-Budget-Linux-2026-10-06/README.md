# OBJ source budget: Linux evidence

Known OBJ files larger than the existing 16 MiB limit are rejected after opening and cancellation polling, before payload reads or allocation. Extension normalization, the streaming growth guard, cancellation, parsing and revision-checked publication remain in place. Unavailable size metadata falls back to the bounded streaming path.

The oversized-source test permits immediate cancellation polling but cancels if a read loop starts; it requires an empty payload and the existing budget error. Existing valid, invalid, cancelled, stale and oversized publication tests remain. Timeout diagnostics now identify the publication phase and operation state; the five-second deadline is unchanged.

Source freeze `0c08ca8ca92d819550532fa796f3e6b9b8f51dbf`. Targeted reimport tests and 100 consecutive runs pass (2.79 s total). Full Linux configure/build and 97/97 tests pass with Khronos core/synchronization validation (112.52 s). Logs and source hashes are retained. This Editor-only change does not modify the Shipping Showcase runtime or its visual artifacts.

Repeated Windows reimport timeouts motivated inspection, but their original generic logs do not establish the failing phase. This change does not claim to resolve every Windows timeout; fresh exact-head CI must pass before merge.

After integrating accepted main `1079461167cc49fbf0c55834cc7f5b605e0d77da` (Editor native-image/surface lifetime updates), full configure/build and 97/97 core/synchronization tests pass again (114.20 s). Integration freeze `08c15731cead0d3f19ea91363621a41273a1476e` includes the pending background-shadow Showcase change. The independently published Editor patch preserves the same tested Editor sources and accepted main; exact-head CI validates that independent tree.
