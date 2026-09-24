# Nexora Editor

`NexoraEditor` is the standalone process boundary for the Editor Preview. The currently portable
executable opens a versioned project, indexes its `Content` tree, and emits deterministic shell
evidence. `NexoraEditorCore` supplies stable panel IDs and authoring models without introducing a
private presentation path.

```bash
NexoraEditor --project=/path/to/project --report=editor-report.json
```

This command is a headless workflow/evidence entry point, not the graphical acceptance gate. The
native docking host and visual Scene/Game views must use the public Window and Presentation
contracts when implemented.

Configure with `NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON` to build the optional
`NexoraEditorImGui` host and its headless draw-data contract test. The option fetches a pinned
Dear ImGui docking release; it remains off by default so the deterministic CLI workflow does not
acquire a graphical dependency.

On a host with a display and native presentation support, launch the shell with:

```bash
NexoraEditor --project=/path/to/project --graphical
```

`--frames=N` supplies a bounded native smoke run for target-host automation. A missing display or
presentation backend is reported as an error rather than silently falling back to the CLI.
The shell tracks the surface's live client extent and DPI scale, rebuilds its bucketed font atlas,
and submits backend-neutral textured/indexed UI draws directly into the acquired native GPU
backbuffer. It also round-trips a versioned project layout and supplies a live `SceneDocument` to
the Hierarchy panel. It does not create a validation-device offscreen target or a full-frame CPU
RGBA image.
