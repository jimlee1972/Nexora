# Native project upgrade input phases — Linux cloud evidence

Accepted on 2026-10-11 Asia/Taipei (2026-10-10 UTC).

The native Project Browser acceptance driver now sends each keyboard chord as one physical
keydown sequence followed by reverse keyup, retaining each phase for 200 ms across rendered
frames. The original short compound Ctrl+O pulse intermittently failed to activate explicit Open
after asynchronous preview completion. This changes only the test driver; product input handling,
upgrade code, all assertions, timeouts and shutdown guards remain unchanged. The gesture is not
retried.

## Accepted scope

Three cold candidate native runs passed all four project cases with the configured Xvfb/xdotool
and actual graphical Editor executable: read-only legacy preview without metadata writes,
writable explicit upgrade/Open with exact CRLF backup and plan report followed by real bootstrap
Scene Save, current-schema read-only preview, and corrupt/recovery conservation. No Vulkan
core/synchronization validation errors are accepted.

The complete affected source-rebase graphical composition passed **252/252 tests in 667.77 s**,
zero skips; `editor.linux_project_upgrade` passed in **18.10 s**. Cold **Minimal Shipping built
74 steps**. This full local proof belongs to the affected graphical composition, including
unmerged prefab work, with input-driver copy 705213ba416ba506834e124a21fe031185e07210.
The isolated input-driver commit c50d4eb43b40f36fe6779c7e0170403acff00503 is based on accepted
Main 5f5079cec33e8f788e0ff10cb6cf5fe2779abc79. This does not claim a separate full local
Main-branch run. Published isolated-head hosted checks must pass before Main merge.

The first affected full run failed only this native explicit-Open step (251/252, 674.30 s) and
is excluded from accepted full proof. Three subsequent unchanged-driver cold runs passed,
showing intermittent input admission. An initial direct invocation guessed an incorrect Xvfb
path; it is excluded. Accepted runs use the configured sysroot native executables.

~~~sh
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
~~~

The affected Development configuration enables the graphical shell, Cryptography, Slang,
Zig gameplay, Showcase and native ProjectPlayer, with unchanged pinned Dear ImGui/Vulkan caches.
Shipping proves minimal profile/link compatibility and does not include Editor. Physical display,
Windows/macOS input acceptance and full graphical milestones remain open (**0/8**).
