# Visible foreground foliage banks: Linux evidence

Move the two existing foreground banks to (-3.8, 3.8) and (3.8, 3.8) in X/Z. The wide and close native camera frames now show foliage around the approach edges. Their original alpha mask, 128 sprigs per bank, leaf shape, materials and shared GPU wind remain unchanged. The third arcade bank and hero geometry remain unchanged; no new geometry is added.

Full Linux configure/build and 104/104 tests pass with Khronos core/synchronization validation. Shipping isolated native acceptance, exact wind/reflection restoration and a real 100-second tour pass. Source freeze `236bf29ef11091ed6a405b3ae5fb40252b426ff0`. Native art, movie and package use the same frozen Shipping executable. Hashes and actual device observations are retained.

Reference parity and physical-display acceptance remain open (VIS 5/7).
