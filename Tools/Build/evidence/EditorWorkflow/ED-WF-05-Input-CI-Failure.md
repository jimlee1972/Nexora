# ED-WF-05 native input CI repair

Source `201d73e14c4a3363fdf644fceae83de1f0af94b6` compiles the complete native Editor after
the GCC15 fixture repairs, but hosted Xvfb acceptance exits8: 231/247 pass,16 fail,zero skips,
1307.53s. It is unaccepted. [Receipt](ED-WF-05-Input-CI-Failure.json) retains the full log/hash.

The failures share Play entry: F5 reaches processed GUI input together with Alt (`keys=010000000`,
`play_keys=100`), admission is enabled but no Play request is issued. The inferred cause is the
keysym injection modifier recipe on the updated runner, not evidence that Alt+F5 should become
a Play shortcut. [Upstream xdotool](https://github.com/jordansissel/xdotool/blob/master/xdo.c#L1415-L1437)
supports explicit decimal keycodes with a zero modifier mask.

The harness queries X11 for each F5/F6/F10 keycode and verifies its unmodified XKB symbol before
sending that physical key through the same native XTEST path. It neither remaps the keyboard nor
invokes Editor APIs. Requested and physical input are recorded, and the copied GUI observations
additionally require no Control/Alt/Shift during the F-key press. All original request-count,
Play/runtime, pixels, source conservation,32-cycle/RSS and deadline checks remain.

Only Python parsing was checked on Windows; no Linux native execution is claimed locally.
The repaired immutable head must pass full Codex Cloud Development/Shipping and CI, including
these native display cases. Keep PR762 draft, ED-WF-05 unaccepted and overall progress4/9.
