# Windows Editor project location selection

## Behavior

Windows Open project chooses the existing `project.nexora` file and passes its parent directory
to the existing workspace-open operation. Create project chooses the actual destination folder,
then confirms a display name. The selected folder remains the project directory; the name is
never appended. The raw Project root and startup Project name fields and standalone upgrade
preview command are absent when the native adapter is installed.

The adapter owns a synchronous STA COM file/folder dialog in the application. It preserves the
process working directory and releases shell items and path storage before its COM scope ends.
No native handle enters the Editor core/widgets. Cancellation returns no request; errors remain
visible. Workspace validation, schema-upgrade backup, writer leases and activation remain owned
by the existing application/Core operations. Non-Windows hosts retain the no-adapter fallback.

## Local validation

Source base: `7eb0072fe97d5c164d79d118912c02422a14e99f`, with this change applied.
The packet `build/project-picker-20261024/` retains source hashes and configure/build/test output.

```powershell
cmake --preset windows-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_VULKAN_BACKEND=OFF -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON -DBUILD_TESTING=ON -DFETCHCONTENT_SOURCE_DIR_NEXORA_IMGUI=<pinned source cache>
cmake --build --preset windows-development --target NexoraEditor NexoraProjectPickerUiTests NexoraProjectUpgradeUiTests NexoraEditorImGuiTests --parallel 3
ctest --preset windows-development --output-on-failure -R '^(editor.imgui_contract|editor.project_upgrade_ui|editor.project_picker_ui)$'
```

MSVC configure/build passed. Three selected CTests passed with no failures or skips (36.13 s).
The new test uses actual ImGui controls at 1x/2x with an injected chooser: it verifies cancellation,
chooser errors, exact selected-directory Open, absence of the standalone preview shortcut,
read-only and busy rejection, delayed name confirmation, empty-name rejection and cancellation.
It consumes the resulting Create request through a real ProjectWorkspace and checks that
`project.nexora` is written directly into the selected folder, without a display-name subdirectory.
This injected chooser does not establish physical operation of the Windows shell dialog.

## Remaining acceptance

The final Windows build also preserves the working directory with `FOS_NOCHANGEDIR`. The updated
Editor-only ZIP passes a 120-frame native launch after extraction into a Unicode/space path,
with 120 acquisitions/presents, zero rejected UI textures and exit 0. The archive is 4,069,826
bytes and contains only the Editor launch entry/host, required DLLs and license notices; no tools,
examples or source. SHA-256: `96521a5aa8c61be00dd6e0192b006563bfdaf7a9500a35a9757daebdbf1d3316`.

Hosted Linux Development/Shipping gates are recorded after their results become available.
Native file/folder dialog manual selection,
other-platform native chooser adapters, independent clean-host validation and full Editor milestone
acceptance are not established by the portable tests. Editor milestone progress remains 0/8.
