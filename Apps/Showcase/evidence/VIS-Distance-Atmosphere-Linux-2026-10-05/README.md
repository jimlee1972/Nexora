# Distance atmosphere: Linux native evidence

✅ Linux Development configure/build and 97/97 tests pass (88.81 seconds, no skips).
Six analytical native HDR atmosphere cases check disabled/half/full haze, exact restoration,
unlit-radiance preservation and clear near-field range (65 PBR frames total). Native F7 input
changes the paused fixed view and restores exact pixels. Shipping/Full packaging and checksum-
verified isolated native interaction pass. The same Shipping executable records a 100-second
animation tour without overlays.

Production source freeze is `70c84136c354492119b85d17866043f143a96221`. Source/executable/movie/package hashes are retained
in `release-provenance.json`. Distance haze blends linear radiance before transparency, focus,
bloom and ACES, while retaining sky/solar/emitter radiance. Solar and panorama orientation move
together with directional lighting; no new pass, texture or stable C/Zig/NXAB changes occur.

Workspace artifacts: `NexoraShowcase-Distance-Atmosphere-70c8413.mp4` and
`NexoraShowcase-Distance-Atmosphere-70c8413-Linux.zip`. Software Vulkan/Xvfb evidence does not
accept physical target performance or final concept-image parity. VIS remains 5/7.
