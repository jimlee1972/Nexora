# Crystal mote scale: Linux evidence

Reduce each animated crystal mote card from 0.025 to 0.015 world-unit half-width and from 0.05 to 0.03 height. The smaller luminous dots retain their count, orbital/vertical trajectories, HDR radiance, texture, materials, shared animation clock and original light/bloom settings. Standard remains 62,557 vertices, 142,488 indices, 395 batches, 48 materials and 4,041 source foliage quads. Fixed paused native before/after shots isolate the smaller motes and their existing reflection/bloom response.

Full Linux configure/build and 111/111 tests pass with core/sync validation. Frozen Shipping native acceptance, exact wind/reflection/occlusion restoration and a real 100-second tour pass. Source freeze `d01541bed0d4b4ec3c06135b121ab44bb627b8d9`. Native art, movie and package use the frozen Shipping executable.

Reference parity and physical-display acceptance remain open (VIS 5/7).

This integration preserves all 15 accepted Animation/Editor/instruction paths from main `e05af4b9a102`. The general Monolithic Shipping build passes; three fixed native art images are byte-identical to the preceding mote freeze. Original mote proof remains in the parent evidence directory.
