# Native Scene save observation — Linux cloud evidence

The Linux native center-gesture helper observes committed scene bytes within its existing
five-second deadline and repeats only Ctrl+S while waiting. Gestures and Undo are still issued
once, and exact saved-byte, transform and one-step Undo assertions remain unchanged. This aligns
the Save observer with the existing Undo-and-save observer. Production editor input is unchanged.

Earlier full gates reported unchanged authored-mesh center bytes; isolated unchanged acceptance
could pass. Separate preview/center Undo failures were also observed and are not claimed fixed
by this narrow helper change. No common timing cause has been established.

On accepted main ceb409da50a4900c9fedfc476db33f18ed03600a, the graphical/native build passed.
Focused capability, native preview, center and authored-mesh acceptance passed **4/4 in 74.52s**.
The full Linux graphical/native CTest preset passed **213/213 in 470.61s**, zero skips; minimal
Shipping passed its five-step incremental build. Validation uses GCC 14.2, Slang 2026.18, Zig,
Xvfb/xdotool and Mesa software Vulkan, as configured in the companion capability evidence.

This result covers the actual helper paths; physical input/display acceptance and general
synthetic-input stability remain open. Fresh final-head hosted CI is required before merge.
