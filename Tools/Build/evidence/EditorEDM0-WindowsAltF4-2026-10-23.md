# Windows Editor Alt+F4 and local interaction evidence (2026-10-23)

## Scope and identity

Windows 10 Home 19045, physical GTX 960, one configured monitor at 100% (96 DPI).
This is agent-observed native input on the physical desktop, not a human operator attestation,
clean-host acceptance, or Linux display evidence. ED-M0 and the 0/8 milestone count remain open.

Base: `715114a95a8de2977d44a61c8765de39623a9f7a`, plus the Win32 system-key forwarding patch
and its native regression test. The isolated acceptance branch integrated latest remote main;
the primary checkout and unrelated work were preserved.

Frozen runtime SHA-256:

- `NexoraEditor.exe`: `a6557daecab9aa808a820e3ed11eedd1a096775ea846c6b62c85e5bc6f7bec13`
- `NexoraWindow.dll`: `15fb2b6a3829c986a509bc86f2e96e3f40af70de352bb90413ba1112ac8518f3`

Generated local packet: `build/my2-alt-f4-20261023/`. It retains source patch identity,
runtime hashes, raw command output, timestamps/exits, disposable project hashes, and individual
PNG/JSON pairs binding each observed frame to its HWND, PID, bounds, capture method, and SHA-256.
No generated binaries or screenshots are committed. Prior packets remain separate and unchanged.

## Alt+F4 cause and behavior

`Win32WindowSystem::WndProc` queued `WM_SYSKEYDOWN/UP` as key events and returned zero, swallowing
default Windows system-key processing. It now queues the same events and delegates system keys
to `DefWindowProcW`. Alt+F4 reaches the existing `WM_CLOSE` handler, which emits `CloseRequested`
and leaves confirmation/destruction to the application. Ordinary F4 remains input only.

`window_presentation.contracts` now checks ordinary F4 press/release, exactly one Alt+F4
close request after its key event, key release forwarding, and native window survival until
application destruction. The fixture must be visible for default system-close handling;
an initial hidden-window attempt failed and is retained in the local packet.
The final, identical visible-window test executable was also run against both DLLs while all other
runtime files stayed fixed: original `NexoraWindow.dll` (`2011ec14…`) fails precisely at
`altF4.size() == 2`; patched DLL exits 0. `regression-control.json` records both binary identities,
commands, exits and timestamps, with raw assertion output retained.

Physical desktop observations through supported native `sky` input and GDI captures:

- Frames 001–004: Alt+F4 opens Unsaved work; Escape cancels and retains the original scene.
- Frames 005–007: Save and Exit reports an independent Save All destination error for the
  default unmanaged Main.scene. Escape cancels safely; single-scene Save Scene succeeds.
  This observed save workflow issue is tracked separately as `nexora-zey` and is not claimed fixed.
- The saved scene then closes with Alt+F4, exit 0 (`dirty-exit.json`).
- A normal reopen and later clean Alt+F4 also exit 0 (`interaction-exit.json`).
- One premature duplicate-writer launch was rejected while the original Save All error dialog
  was still open; its stderr/session are retained. It is not a successful clean-close run.

## Continued .4.2 / .4.3 acceptance

Frames 014–030 use the installed zh-TW IME in Rename at 100% DPI. Home places the insertion point
at the start of `Scene Root`; End places it at the end. Each starts the same `s`, `u`, `3`, Down
composition/candidate sequence. Candidate left edges in window-relative pixels are approximately
475 and 545 respectively (frames 020 and 027), at the same vertical position beside their carets.
Both compositions are cancelled before cancelling Rename; saved scene SHA-256 stays unchanged.
This adds concrete caret-follow evidence at 100%, supplementing the earlier commit/cancel record.
Other DPI/cross-monitor cases and a complete operator record remain unaccepted.

Frames 008–013 retry Content undocking/movement against the latest main input changes. Movement
works; successful redocking is not established. Frames 032–040 add a one-node, six-component
read-only Inspector fixture with real horizontal/vertical scrollbars. Vertical down/up correctly
shows rows 4/5 and returns to rows 0/1. Horizontal input is reversed: documented right `scrollX=600`
does not advance from the left edge, documented left `-600` advances right, and `+600` restores
the original frame. Frames 035/036/038/040 are byte-identical; 037 shows horizontal displacement,
and 039 shows vertical displacement. Horizontal acceptance therefore fails and remains a separate
follow-up `nexora-3mj`, rather than being marked passed. The LF fixture exits 0 with scene SHA-256 unchanged;
the initial CRLF fixture was rejected and retained as a harness error, not a product regression.
Held-input focus loss remains unverified. The supported input API has a complete press/chord operation, not a
key-down hold spanning subsequent focus changes; a completed press is not substituted for that test.
Earlier keyboard Recover/Discard, corrupt-journal retry/safe-exit, vertical-wheel and floating-layout
restart evidence remain scoped to their earlier frozen runtime, not silently transferred to this one.

The GDI path uses `PrintWindow(PW_RENDERFULLCONTENT)` for Editor content and foreground-guarded
`CopyFromScreen` for native IME overlays. The original WGC timeout remains separately open as
`nexora-nqn.4.6`; alternative capture does not claim to repair it.

## Validation

- `cmake --preset windows-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_VULKAN_BACKEND=OFF`: exit 0.
- `cmake --build --preset windows-development --parallel 2`: exit 0.
- `ctest --preset windows-development --output-on-failure -R '^(window_presentation.contracts|editor.imgui_contract|editor.windows_dpi_ime_contract)$'`: 3/3 passed, exit 0.
- `ctest --preset windows-development --output-on-failure -E '^(editor.deep_hierarchy|editor.hierarchy_scale_lifecycle|editor.overview_pose_snapshot)$'`:
  user-approved local Windows baseline, 310 selected, 304 passed / 6 native artifact-device skips /
  0 failed, exit 0, 1560.78 seconds. The full inventory has 313 tests; exactly the three unchanged
  100k stress tests remain outside this named local tier. No timeout, assertion, fixture-size,
  or CI exclusion change is made by this patch.
- Fresh frozen-runtime `--graphical --frames=240` smoke: exit 0, acquired=presented=240,
  ui_draws=2631, ui_uploads=1, ui_rejected=0; GPU timing source=2, software=0.
- Pinned isolated Markdown tooling: documentation tests 16/16 passed; changed documents and
  bilingual counterpart/link checks passed; `git diff --check` passed. Root README unchanged.
- Linux full gate is not locally available on this Windows host. Required Linux configure/build/
  CTest must be verified in the exact-head CI before completion/merge; Windows results do not
  substitute for it. No linkage boundary changed, so a separate Linux Shipping gate is not required.
