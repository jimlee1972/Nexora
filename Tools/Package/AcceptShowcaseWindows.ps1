[CmdletBinding()]
param(
    [string]$PackageRoot = '',
    [string]$EvidenceDirectory = (Join-Path (Get-Location) 'showcase-windows-v1-evidence'),
    [ValidateSet('dx12', 'vulkan')][string]$Backend = 'dx12',
    [string]$ExpectedBuildId = '',
    [string]$ExpectedVulkanDriverLibrary = '',
    [switch]$CompleteGuidedTour,
    [switch]$PhysicalDisplay,
    [switch]$CleanHost,
    [switch]$AllowUnavailableDisplay
)
$ErrorActionPreference = 'Stop'
# Windows PowerShell 5 can evaluate parameter defaults before PSScriptRoot is set.
if ([string]::IsNullOrWhiteSpace($PackageRoot)) { $PackageRoot = $PSScriptRoot }
$evidence = [IO.Path]::GetFullPath($EvidenceDirectory)
[IO.Directory]::CreateDirectory($evidence) | Out-Null
$acceptance = [ordered]@{
    schema = 'nexora.showcase.windows.acceptance.v1'; status = 'FAIL'
    scope = 'Windows isolated-copy native visual acceptance'
    physical_display_verified = $false; clean_host_verified = $false
    physical_display_operator_attestation = [bool]$PhysicalDisplay
    clean_host_operator_attestation = [bool]$CleanHost
    recorded_utc = [DateTime]::UtcNow.ToString('o'); checksums_verified = 0
    screenshots = @(); engine_module_locations = @{}; issues = @()
    interaction_checks = @(); complete_guided_tour_requested = [bool]$CompleteGuidedTour
}
$process = $null
$staged = $null
$exitCode = 1
function Write-Acceptance {
    [IO.File]::WriteAllText((Join-Path $evidence 'acceptance.json'),
        ($acceptance | ConvertTo-Json -Depth 20), [Text.UTF8Encoding]::new($false))
}
function Require([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
}
try {
    Require ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) 'Windows is required.'
    Add-Type -AssemblyName System.Drawing
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class NexoraAcceptanceWindow {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X, Y; }
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr w, out Rect r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr w, ref Point p);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr w);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr w, IntPtr after, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w, uint message, IntPtr value, IntPtr flags);
    [DllImport("user32.dll")] public static extern IntPtr OpenInputDesktop(uint flags, bool inherit, uint access);
    [DllImport("user32.dll")] public static extern bool CloseDesktop(IntPtr desktop);
    public static void Press(IntPtr w, uint key) {
        PostMessage(w, 0x100, new IntPtr(key), new IntPtr(1));
        PostMessage(w, 0x101, new IntPtr(key), new IntPtr(unchecked((int)0xc0000001)));
    }
}
'@
    $desktop = [NexoraAcceptanceWindow]::OpenInputDesktop(0, $false, 0x0001)
    if ($desktop -eq [IntPtr]::Zero) {
        $acceptance.status = 'UNSUPPORTED'
        $acceptance.issues = @('No accessible interactive Windows desktop; unlock a local desktop and rerun.')
        Write-Acceptance
        if ($AllowUnavailableDisplay) { exit 77 }
        throw $acceptance.issues[0]
    }
    [NexoraAcceptanceWindow]::CloseDesktop($desktop) | Out-Null
    $root = [IO.Path]::GetFullPath($PackageRoot)
    $staged = Join-Path ([IO.Path]::GetTempPath()) ('nexora-v1-' + [Guid]::NewGuid().ToString('N'))
    Copy-Item -LiteralPath $root -Destination $staged -Recurse
    $prefix = $staged.TrimEnd('\') + '\'
    foreach ($line in [IO.File]::ReadAllLines((Join-Path $staged 'manifests/SHA256SUMS'))) {
        Require ($line -match '^([0-9a-f]{64})  (.+)$') 'Malformed package checksum record.'
        $expected = $Matches[1]
        $path = [IO.Path]::GetFullPath((Join-Path $staged $Matches[2]))
        Require ($path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) 'Checksum path leaves the package.'
        Require ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -eq $expected) "Checksum mismatch: $path"
        $acceptance.checksums_verified++
    }
    $build = Get-Content -LiteralPath (Join-Path $staged 'manifests/build.json') -Raw | ConvertFrom-Json
    Require ($build.application -eq 'NexoraShowcase') 'Unexpected packaged application.'
    $binary = Join-Path $staged 'bin/NexoraShowcase.exe'
    Require (Test-Path -LiteralPath $binary) 'Windows Showcase executable is missing.'
    $arguments = @('--mode=interactive', '--scene=hub', "--backend=$Backend", '--no-reload',
        '--report=acceptance-launch.json', '--markdown=acceptance-launch.md')
    if ($build.gameplay_linkage -eq 'static') { $arguments += '--gameplay-module=static' }
    else {
        $module = @(Get-ChildItem -LiteralPath (Join-Path $staged 'bin') -Filter 'NexoraZigGameplay.dll')
        Require ($module.Count -eq 1) 'Development Zig module is missing.'
        $arguments += @('--gameplay-module=dynamic', '--gameplay-library=bin/NexoraZigGameplay.dll')
    }
    $acceptance.profile = $build.profile
    $acceptance.shipping_profile = $build.shipping_profile
    $process = Start-Process -FilePath $binary -ArgumentList $arguments -WorkingDirectory $staged -PassThru -NoNewWindow `
        -RedirectStandardOutput (Join-Path $evidence 'stdout.log') -RedirectStandardError (Join-Path $evidence 'stderr.log')
    # Retain the native process handle before exit: Windows PowerShell 5 otherwise
    # can return a null ExitCode from a Start-Process -PassThru object after WaitForExit.
    $null = $process.Handle
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        $process.Refresh()
        Require (-not $process.HasExited) 'Showcase exited before publishing its window; see stderr.log.'
        $window = $process.MainWindowHandle
        if ($window -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    Require ($window -ne [IntPtr]::Zero) 'Showcase did not publish its native window.'
    # Fit the outer window on the primary desktop and keep the client unobscured by
    # the launcher's console. Screen captures must show the native scene, not other windows.
    $outerWidth = [Math]::Min(1300, [NexoraAcceptanceWindow]::GetSystemMetrics(0))
    $outerHeight = [Math]::Min(780, [NexoraAcceptanceWindow]::GetSystemMetrics(1) - 40)
    Require ([NexoraAcceptanceWindow]::SetWindowPos($window, [IntPtr]::new(-1), 0, 0,
        $outerWidth, $outerHeight, 0x0040)) 'Could not expose the Showcase on the desktop.'
    [NexoraAcceptanceWindow]::SetForegroundWindow($window) | Out-Null
    Start-Sleep -Milliseconds 400
    function Capture([string]$name) {
        Require ([NexoraAcceptanceWindow]::SetWindowPos($window, [IntPtr]::new(-1), 0, 0,
            0, 0, 3)) 'Could not bring the Showcase above other windows.'
        $rect = [NexoraAcceptanceWindow+Rect]::new()
        $point = [NexoraAcceptanceWindow+Point]::new()
        Require ([NexoraAcceptanceWindow]::GetClientRect($window, [ref]$rect)) 'Client rectangle unavailable.'
        Require ([NexoraAcceptanceWindow]::ClientToScreen($window, [ref]$point)) 'Window screen position unavailable.'
        $width = $rect.Right - $rect.Left; $height = $rect.Bottom - $rect.Top
        Require ($width -gt 0 -and $height -gt 0) 'Window client is empty.'
        $bitmap = [Drawing.Bitmap]::new($width, $height)
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CopyFromScreen($point.X, $point.Y, 0, 0, $bitmap.Size)
            $colors = [Collections.Generic.HashSet[int]]::new()
            for ($y = 0; $y -lt $height; $y += 13) {
                for ($x = 0; $x -lt $width; $x += 13) { $colors.Add($bitmap.GetPixel($x, $y).ToArgb()) | Out-Null }
            }
            Require ($colors.Count -ge 8) 'Display capture is blank; keep the Showcase visible and rerun.'
            $bitmap.Save((Join-Path $evidence $name), [Drawing.Imaging.ImageFormat]::Png)
            if ($script:acceptance.screenshots -notcontains $name) { $script:acceptance.screenshots += $name }
        } finally { $graphics.Dispose(); $bitmap.Dispose() }
    }
    $rooms = @('hub', 'rendering', 'scene', 'input', 'gameplay', 'presentation', 'streaming', 'shipping')
    for ($i = 0; $i -lt $rooms.Count; $i++) {
        [NexoraAcceptanceWindow]::Press($window, [uint32](49 + $i))
        Start-Sleep -Milliseconds 250
        Capture ($rooms[$i] + '.png')
        if ($i -eq 1) {
            [NexoraAcceptanceWindow]::Press($window, 80); Start-Sleep -Milliseconds 250; Capture 'quad.png'
            [NexoraAcceptanceWindow]::Press($window, 80); Start-Sleep -Milliseconds 250; Capture 'triangle.png'
            [NexoraAcceptanceWindow]::Press($window, 80)
        }
    }
    function Press-Key([uint32]$key) {
        [NexoraAcceptanceWindow]::Press($window, $key)
        Start-Sleep -Milliseconds 150
    }
    function Capture-Compared([string]$name, [string]$reference, [bool]$equal) {
        $deadline = [DateTime]::UtcNow.AddSeconds(5)
        do {
            Capture $name
            $same = (Get-FileHash (Join-Path $evidence $reference)).Hash -eq
                (Get-FileHash (Join-Path $evidence $name)).Hash
            if ($same -eq $equal) { return }
            Start-Sleep -Milliseconds 100
        } while ([DateTime]::UtcNow -lt $deadline)
        throw "Native comparison did not settle within five seconds: $name"
    }
    # Exercise the same shared PBR scene and comparison path on each requested native backend.
    $rooms += 'courtyard'
    Press-Key 57
    Press-Key 32 # Explicit animation pause for exact fixed-shot comparisons.
    Press-Key 82 # Replay to time zero.
    Capture 'courtyard-ui.png'
    Press-Key 115 # F4: remove the overlay from fixed visual evidence.
    Capture-Compared 'courtyard-wide.png' 'courtyard-ui.png' $false
    Press-Key 75 # K: restrained GPU bloom comparison.
    Capture-Compared 'courtyard-bloom-off.png' 'courtyard-wide.png' $false
    Require ((Get-FileHash (Join-Path $evidence 'courtyard-wide.png')).Hash -ne
        (Get-FileHash (Join-Path $evidence 'courtyard-bloom-off.png')).Hash) 'Bloom did not change pixels.'
    Press-Key 75
    Capture-Compared 'courtyard-bloom-restored.png' 'courtyard-wide.png' $true
    Require ((Get-FileHash (Join-Path $evidence 'courtyard-wide.png')).Hash -eq
        (Get-FileHash (Join-Path $evidence 'courtyard-bloom-restored.png')).Hash) 'Bloom restoration differs.'
    $acceptance.courtyard_bloom_comparison = $true

    Press-Key 117 # F6: directional shadow comparison.
    Capture-Compared 'courtyard-shadow-off.png' 'courtyard-wide.png' $false
    Require ((Get-FileHash (Join-Path $evidence 'courtyard-wide.png')).Hash -ne
        (Get-FileHash (Join-Path $evidence 'courtyard-shadow-off.png')).Hash) 'Directional shadow comparison did not change pixels.'
    Press-Key 117
    Capture-Compared 'courtyard-shadow-restored.png' 'courtyard-wide.png' $true
    Require ((Get-FileHash (Join-Path $evidence 'courtyard-wide.png')).Hash -eq
        (Get-FileHash (Join-Path $evidence 'courtyard-shadow-restored.png')).Hash) 'Shadow restoration differs.'
    Press-Key 71 # G: stylized tonal separation.
    Capture-Compared 'courtyard-neutral.png' 'courtyard-wide.png' $false
    Require ((Get-FileHash (Join-Path $evidence 'courtyard-wide.png')).Hash -ne
        (Get-FileHash (Join-Path $evidence 'courtyard-neutral.png')).Hash) 'Stylized tone did not change pixels.'
    Press-Key 71
    Capture-Compared 'courtyard-styled-restored.png' 'courtyard-wide.png' $true
    Require ((Get-FileHash (Join-Path $evidence 'courtyard-wide.png')).Hash -eq
        (Get-FileHash (Join-Path $evidence 'courtyard-styled-restored.png')).Hash) 'Tone restoration differs.'
    $acceptance.courtyard_shadow_comparison = $true
    $acceptance.courtyard_tone_comparison = $true
    Press-Key 69 # E: expose retained linear HDR highlights.
    Capture-Compared 'courtyard-exposure.png' 'courtyard-wide.png' $false
    Require ((Get-FileHash (Join-Path $evidence 'courtyard-wide.png')).Hash -ne
        (Get-FileHash (Join-Path $evidence 'courtyard-exposure.png')).Hash) 'Courtyard HDR exposure pixels did not change.'
    Press-Key 69
    Capture-Compared 'courtyard-exposure-restored.png' 'courtyard-wide.png' $true
    Require ((Get-FileHash (Join-Path $evidence 'courtyard-wide.png')).Hash -eq
        (Get-FileHash (Join-Path $evidence 'courtyard-exposure-restored.png')).Hash) 'Courtyard exposure restoration differs.'
    $acceptance.courtyard_hdr_exposure = $true
    Press-Key 79 # O: compare actual environment lighting with direct light.
    Capture-Compared 'courtyard-direct.png' 'courtyard-wide.png' $false
    Require ((Get-FileHash (Join-Path $evidence 'courtyard-wide.png')).Hash -ne
        (Get-FileHash (Join-Path $evidence 'courtyard-direct.png')).Hash) 'Courtyard IBL comparison pixels did not change.'
    Press-Key 79
    Capture-Compared 'courtyard-ibl-restored.png' 'courtyard-wide.png' $true
    Require ((Get-FileHash (Join-Path $evidence 'courtyard-wide.png')).Hash -eq
        (Get-FileHash (Join-Path $evidence 'courtyard-ibl-restored.png')).Hash) 'Courtyard IBL restoration pixels differ.'
    $acceptance.courtyard_ibl_comparison = $true
    Press-Key 80
    Capture-Compared 'courtyard-lambert.png' 'courtyard-wide.png' $false
    Require ((Get-FileHash (Join-Path $evidence 'courtyard-wide.png')).Hash -ne
        (Get-FileHash (Join-Path $evidence 'courtyard-lambert.png')).Hash) 'Courtyard material comparison pixels did not change.'
    Press-Key 80
    Capture-Compared 'courtyard-pbr-restored.png' 'courtyard-wide.png' $true
    Require ((Get-FileHash (Join-Path $evidence 'courtyard-wide.png')).Hash -eq
        (Get-FileHash (Join-Path $evidence 'courtyard-pbr-restored.png')).Hash) 'Courtyard PBR restoration pixels differ.'
    foreach ($effect in @(@(78, 'wind'), @(77, 'transmission'))) {
        Press-Key $effect[0]
        Capture-Compared "courtyard-$($effect[1])-off.png" 'courtyard-wide.png' $false
        Press-Key $effect[0]
        Capture-Compared "courtyard-$($effect[1])-restored.png" 'courtyard-wide.png' $true
    }
    $acceptance.courtyard_wind_comparison = $true
    $acceptance.courtyard_transmission_comparison = $true
    Press-Key 66; Capture 'courtyard-material.png'
    Press-Key 66; Capture 'courtyard-motion.png'
    Press-Key 66
    Capture-Compared 'courtyard-wide-replay.png' 'courtyard-wide.png' $true
    Require ((Get-FileHash (Join-Path $evidence 'courtyard-wide.png')).Hash -eq
        (Get-FileHash (Join-Path $evidence 'courtyard-wide-replay.png')).Hash) 'Courtyard fixed camera replay pixels differ.'
    Press-Key 81 # Standard -> High -> Basic -> Standard, with effects paused.
    Capture-Compared 'courtyard-quality-high.png' 'courtyard-wide.png' $false
    Press-Key 81
    Capture-Compared 'courtyard-quality-basic.png' 'courtyard-wide.png' $false
    Press-Key 81
    Capture-Compared 'courtyard-quality-standard.png' 'courtyard-wide.png' $true
    $acceptance.courtyard_quality_cycle_restores_pixels = $true
    Press-Key 67 # Enter actual free camera.
    Capture-Compared 'courtyard-free-camera.png' 'courtyard-wide.png' $false
    [NexoraAcceptanceWindow]::PostMessage($window, 0x100, [IntPtr]::new(87), [IntPtr]::new(1)) | Out-Null
    Start-Sleep -Milliseconds 300
    [NexoraAcceptanceWindow]::PostMessage($window, 0x101, [IntPtr]::new(87), [IntPtr]::new(-1073741823)) | Out-Null
    Capture-Compared 'courtyard-free-moved.png' 'courtyard-free-camera.png' $false
    Press-Key 82
    Capture-Compared 'courtyard-free-restored.png' 'courtyard-wide.png' $true
    $acceptance.courtyard_free_camera = $true
    Press-Key 13 # Activate the device with animation paused at time zero.
    Capture-Compared 'courtyard-activated.png' 'courtyard-wide.png' $false
    Start-Sleep -Milliseconds 300
    Capture-Compared 'courtyard-paused.png' 'courtyard-activated.png' $true
    Press-Key 32
    Start-Sleep -Milliseconds 600
    Press-Key 32
    Capture-Compared 'courtyard-animated.png' 'courtyard-activated.png' $false
    Press-Key 82
    Capture-Compared 'courtyard-animation-replay.png' 'courtyard-activated.png' $true
    Press-Key 13
    Capture-Compared 'courtyard-inactive.png' 'courtyard-wide.png' $true
    $acceptance.courtyard_living_replay = $true
    Press-Key 115
    $acceptance.courtyard_material_comparison = $true
    $acceptance.courtyard_fixed_shots = $true
    function Clear-StateOutput {
        foreach ($name in @('showcase-lab.json', 'showcase-lab.md')) {
            $path = Join-Path $staged $name
            if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
        }
    }
    function Export-State([string]$name) {
        # Each X must publish a fresh owning snapshot; an earlier export is not evidence.
        Clear-StateOutput
        Press-Key 114; Press-Key 88
        $jsonPath = Join-Path $staged 'showcase-lab.json'
        $markdownPath = Join-Path $staged 'showcase-lab.md'
        $deadline = [DateTime]::UtcNow.AddSeconds(5)
        while (-not ((Test-Path -LiteralPath $jsonPath) -and (Test-Path -LiteralPath $markdownPath))) {
            Require ([DateTime]::UtcNow -lt $deadline) 'Fresh Runtime state export is missing.'
            Start-Sleep -Milliseconds 50
        }
        $snapshot = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json
        Copy-Item -LiteralPath $jsonPath -Destination (Join-Path $evidence ($name + '.json'))
        Copy-Item -LiteralPath $markdownPath -Destination (Join-Path $evidence ($name + '.md'))
        Press-Key 114
        return $snapshot
    }
    function Require-Metric($probe, [string]$name, [string]$expected) {
        $metric = @($probe.metrics | Where-Object { $_.name -eq $name })
        Require ($metric.Count -eq 1 -and $metric[0].value -eq $expected) "Unexpected sampled metric: $name"
    }
    Press-Key 50 # Rendering: exercise the actual Win32 pointer/wheel route
    [NexoraAcceptanceWindow]::PostMessage($window, 0x200, [IntPtr]::Zero, [IntPtr]::new(400 -bor (350 -shl 16))) | Out-Null
    [NexoraAcceptanceWindow]::PostMessage($window, 0x201, [IntPtr]::new(1), [IntPtr]::Zero) | Out-Null
    [NexoraAcceptanceWindow]::PostMessage($window, 0x200, [IntPtr]::new(1), [IntPtr]::new(460 -bor (380 -shl 16))) | Out-Null
    [NexoraAcceptanceWindow]::PostMessage($window, 0x202, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    [NexoraAcceptanceWindow]::PostMessage($window, 0x20A, [IntPtr]::new(120 -shl 16), [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 250
    Capture 'rendering-orbit-zoom.png'
    Press-Key 112; Press-Key 113; Capture 'overlays-hidden.png'
    Press-Key 112; Press-Key 113
    $acceptance.interaction_checks += @('pointer_orbit_wheel_zoom', 'overview_profiler_toggles')
    Press-Key 51 # Scene
    Press-Key 69; Capture 'scene-modified.png'
    Press-Key 85; Capture 'scene-undo.png'
    Press-Key 69 # F5 must round-trip the changed Editor World and clear its Undo history
    Press-Key 80; Start-Sleep -Milliseconds 300; Capture 'scene-play.png'
    Press-Key 116; Capture 'scene-reloaded.png' # F5 stops Play and reloads the snapshot
    $reloaded = Export-State 'scene-reload-state'
    Require ($reloaded.selected -eq 'scene' -and $reloaded.reloads -ge 1 -and
        $reloaded.lines -contains 'Last action: Scene snapshot reload' -and
        $reloaded.lines -contains 'Inspector Transform.x = 0.50 / Undo depth 0') 'Scene snapshot restore or editor state failed.'
    Press-Key 80; Press-Key 80 # Verify the reloaded editor can start/stop Play
    Press-Key 52; Press-Key 76; Capture 'input-localized.png'
    Press-Key 53
    [NexoraAcceptanceWindow]::PostMessage($window, 0x100, [IntPtr]::new(68), [IntPtr]::new(1)) | Out-Null
    Start-Sleep -Milliseconds 500
    [NexoraAcceptanceWindow]::PostMessage($window, 0x101, [IntPtr]::new(68), [IntPtr]::new(-1073741823)) | Out-Null
    Press-Key 67; Capture 'gameplay-crouched.png'
    Press-Key 71; Capture 'gameplay-teleported.png'
    Press-Key 54; Press-Key 74; Start-Sleep -Milliseconds 600; Capture 'presentation-blend.png'
    Press-Key 56; Press-Key 66; Press-Key 66; Press-Key 72; Capture 'shipping-pressure.png'
    Press-Key 84; Start-Sleep -Milliseconds 1800; Press-Key 32; Capture 'tour-paused.png'
    $beforeReplay = Export-State 'tour-before-replay'
    Require ($beforeReplay.tour.enabled -and $beforeReplay.tour.paused -and
        $beforeReplay.tour.seconds -ge 1.5) 'Tour did not advance before pause/replay.'
    Press-Key 82; Press-Key 32 # Replay resets elapsed time and step, then pause the fresh tour
    $replayed = Export-State 'tour-replayed'
    Require ($replayed.tour.enabled -and $replayed.tour.paused -and
        $replayed.tour.step -eq 0 -and $replayed.tour.seconds -lt 1 -and
        $replayed.tour.seconds -lt $beforeReplay.tour.seconds) 'Tour replay did not reset progress.'
    $acceptance.interaction_checks += 'guided_tour_replay_pause'
    if ($CompleteGuidedTour) {
        Press-Key 32
        # Keep rendering all seven steps; do not fabricate elapsed simulation time.
        $tourDeadline = [DateTime]::UtcNow.AddSeconds(215)
        while ([DateTime]::UtcNow -lt $tourDeadline) {
            Require (-not $process.HasExited) 'Showcase exited during the complete guided tour.'
            Start-Sleep -Milliseconds 500
        }
        Capture 'tour-completed.png'
    }
    $tourState = Export-State 'tour-state'
    if ($CompleteGuidedTour) {
        Require ($tourState.tour.seconds -ge 210 -and $tourState.tour.step -eq 6 -and
            $tourState.tour.enabled -and $tourState.tour.paused) 'Seven-step guided tour did not complete.'
        $acceptance.interaction_checks += 'guided_tour_210_seconds'
    }
    Press-Key 56 # Leave tour before Lab reruns
    Clear-StateOutput
    $acceptance.interaction_checks += @('scene_modify_undo_play_reload', 'locale_switch', 'held_character_input_crouch_teleport', 'animation_blend', 'lifecycle_pressure')
    [NexoraAcceptanceWindow]::Press($window, 114) # F3
    for ($i = 0; $i -lt 5; $i++) { [NexoraAcceptanceWindow]::Press($window, 9) }
    [NexoraAcceptanceWindow]::Press($window, 73); [NexoraAcceptanceWindow]::Press($window, 82)
    [NexoraAcceptanceWindow]::Press($window, 9)
    [NexoraAcceptanceWindow]::Press($window, 73); [NexoraAcceptanceWindow]::Press($window, 73)
    [NexoraAcceptanceWindow]::Press($window, 82); Press-Key 88
    $labDeadline = [DateTime]::UtcNow.AddSeconds(5)
    while (-not (Test-Path -LiteralPath (Join-Path $staged 'showcase-lab.json'))) {
        Require ([DateTime]::UtcNow -lt $labDeadline) 'Fresh sampled Lab results are missing.'
        Start-Sleep -Milliseconds 50
    }
    Start-Sleep -Milliseconds 300
    Capture 'validation-lab.png'
    $labPath = Join-Path $staged 'showcase-lab.json'
    Require (Test-Path -LiteralPath $labPath) 'Validation Lab did not export its sampled results.'
    $lab = Get-Content -LiteralPath $labPath -Raw | ConvertFrom-Json
    if ($build.shipping_profile -eq 'Full') {
        Require ($lab.integration_probes.probes[5].status -eq 'PASS') 'M5 sampled asset-error case failed.'
        Require ($lab.integration_probes.probes[6].status -eq 'PASS') 'M6 sampled plugin ABI rejection failed.'
        Require-Metric $lab.integration_probes.probes[5] 'input.milestone' '5'
        Require-Metric $lab.integration_probes.probes[5] 'input.error_case' '1'
        Require-Metric $lab.integration_probes.probes[5] 'output.accepted' 'true'
        Require-Metric $lab.integration_probes.probes[6] 'input.milestone' '6'
        Require-Metric $lab.integration_probes.probes[6] 'input.error_case' '3'
        Require-Metric $lab.integration_probes.probes[6] 'output.accepted' 'true'
        Require-Metric $lab.integration_probes.probes[6] 'output.loaded' 'false'
        Require-Metric $lab.integration_probes.probes[6] 'output.registered' 'false'
    }
    Copy-Item -LiteralPath $labPath -Destination (Join-Path $evidence 'showcase-lab.json')
    Copy-Item -LiteralPath (Join-Path $staged 'showcase-lab.md') -Destination (Join-Path $evidence 'showcase-lab.md')
    [NexoraAcceptanceWindow]::Press($window, 114)
    [NexoraAcceptanceWindow]::SetWindowPos($window, [IntPtr]::Zero, 0, 0, 960, 540, 6) | Out-Null
    Start-Sleep -Milliseconds 300
    Capture 'resized.png'
    foreach ($module in $process.Modules) {
        if ($module.ModuleName -like 'Nexora*') {
            Require ($module.FileName.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) 'Engine module resolved outside the isolated copy.'
            $acceptance.engine_module_locations[$module.ModuleName] = $module.FileName.Substring($prefix.Length)
        }
    }
    if ($ExpectedVulkanDriverLibrary) {
        Require ($Backend -eq 'vulkan') 'An expected Vulkan driver requires the Vulkan backend.'
        $expectedDriver = [IO.Path]::GetFullPath($ExpectedVulkanDriverLibrary)
        $loadedDriver = @($process.Modules | Where-Object {
            [string]::Equals($_.FileName, $expectedDriver, [StringComparison]::OrdinalIgnoreCase)
        })
        Require ($loadedDriver.Count -eq 1) 'The requested Vulkan driver DLL was not loaded.'
        $acceptance.vulkan_driver_library = [ordered]@{
            name = [IO.Path]::GetFileName($expectedDriver)
            sha256 = (Get-FileHash -LiteralPath $expectedDriver -Algorithm SHA256).Hash.ToLowerInvariant()
            loaded_from_expected_path = $true
        }
    }
    [NexoraAcceptanceWindow]::PostMessage($window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    Require ($process.WaitForExit(15000)) 'Showcase did not close cleanly.'
    $acceptance.process_exit_code = $process.ExitCode
    Require ($process.ExitCode -eq 0) "Showcase exit failed (code $($process.ExitCode)); see retained launch report and logs."
    $report = Get-Content -LiteralPath (Join-Path $staged 'acceptance-launch.json') -Raw | ConvertFrom-Json
    Copy-Item -LiteralPath (Join-Path $staged 'acceptance-launch.json') -Destination (Join-Path $evidence 'launch-report.json')
    Copy-Item -LiteralPath (Join-Path $staged 'acceptance-launch.md') -Destination (Join-Path $evidence 'launch-report.md')
    Require ($report.status -eq 'PASS' -and -not $report.headless_evidence.executed) 'Native launch scope failed.'
    $native = $report.windowed_evidence
    Require ($native.executed -and $native.backend -eq $Backend -and -not $native.backend_fallback) 'Requested native backend was not used.'
    Require ($native.native_graph_frames -gt 0 -and $native.native_graph_frames -eq $native.scene_draws `
        -and $native.native_offscreen_draws -eq $native.scene_draws -and $native.native_scene_composites -eq $native.scene_draws `
        -and $native.native_graph_passes -eq 4 -and $native.native_graph_resource_transitions -eq 3 `
        -and ($native.native_graph_order -join ',') -eq 'Offscreen,Main,UI,Present' `
        -and $native.composed_frames -eq 0 -and $native.native_scene_texture_uploads -gt 0 `
        -and $native.surface_acquires -eq ($native.surface_presents + $native.surface_recoverable_presents) -and $native.resize_generations -gt 0) 'Native graph/lifecycle counters failed.'
    Require ($report.runtime_rooms.reloads -ge 1 -and $report.runtime_rooms.healthy) 'Scene reload or Runtime room health failed.'
    foreach ($room in $rooms) { Require ($report.runtime_rooms.visited -contains $room) "Room not visited: $room" }
    if ($ExpectedBuildId) { Require ($report.build.build_id -eq $ExpectedBuildId) 'Build ID does not match the requested version.' }
    if ($PhysicalDisplay) { Require (-not $native.software_rasterizer) 'Physical GPU acceptance cannot use a software rasterizer.' }
    $acceptance.build = $report.build
    $acceptance.native = $native
    $acceptance.display_adapters = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion)
    $acceptance.os_version = [Environment]::OSVersion.VersionString
    $acceptance.physical_display_verified = [bool]$PhysicalDisplay
    $acceptance.clean_host_verified = [bool]$CleanHost
    $acceptance.status = 'PASS'
    $exitCode = 0
} catch { $acceptance.issues = @($_.Exception.Message) }
finally {
    if ($null -ne $process) {
        if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
    }
    if ($null -ne $staged -and (Test-Path -LiteralPath $staged)) {
        foreach ($entry in @(@('acceptance-launch.json', 'launch-report.json'), @('acceptance-launch.md', 'launch-report.md'))) {
            $source = Join-Path $staged $entry[0]
            if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination (Join-Path $evidence $entry[1]) }
        }
        Remove-Item -LiteralPath $staged -Recurse -Force
    }
    Write-Acceptance
}
foreach ($issue in $acceptance.issues) { Write-Output $issue }
Write-Output (Join-Path $evidence 'acceptance.json')
exit $exitCode
