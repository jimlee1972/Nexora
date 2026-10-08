[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$EditorExe,
    [Parameter(Mandatory = $true)][string]$ProjectPath,
    [string]$EvidenceDirectory = (Join-Path (Get-Location) 'editor-windows-ed-m0-evidence'),
    [string]$Commit = '',
    [string]$Backend = 'dx12',
    [uint32]$SmokeFrames = 240
)
# Records ED-M0 Windows target-host evidence (Editor_ImGui_Integration_Plan.md, WP6/WP8).
# The automated part launches the real graphical Editor for a bounded frame count and parses its
# "graphical evidence:" line. DPI and IME rows can only be judged by a person at the machine, so
# they are recorded as operator answers and default to "blocked" - never to "pass".
$ErrorActionPreference = 'Stop'
$evidence = [IO.Path]::GetFullPath($EvidenceDirectory)
[IO.Directory]::CreateDirectory($evidence) | Out-Null
if ([string]::IsNullOrWhiteSpace($Commit)) {
    try { $Commit = (& git rev-parse HEAD 2>$null) } catch { $Commit = '' }
    if ([string]::IsNullOrWhiteSpace($Commit)) { $Commit = 'unknown' }
}
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { throw 'Windows is required.' }

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class NexoraMonitors {
    delegate bool MonitorProc(IntPtr m, IntPtr dc, IntPtr rect, IntPtr data);
    [DllImport("user32.dll")] static extern bool EnumDisplayMonitors(IntPtr dc, IntPtr clip, MonitorProc cb, IntPtr data);
    [DllImport("shcore.dll")] static extern int GetDpiForMonitor(IntPtr m, int type, out uint x, out uint y);
    [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
    public static string[] List() {
        SetProcessDPIAware();
        var rows = new List<string>();
        EnumDisplayMonitors(IntPtr.Zero, IntPtr.Zero, (m, dc, r, d) => {
            uint x = 0, y = 0;
            GetDpiForMonitor(m, 0, out x, out y);
            rows.Add(x + " dpi (" + Math.Round(x * 100.0 / 96.0) + "%)");
            return true;
        }, IntPtr.Zero);
        return rows.ToArray();
    }
}
'@

$gpu = @(Get-CimInstance Win32_VideoController | ForEach-Object { "$($_.Name) / $($_.DriverVersion)" })
$os = (Get-CimInstance Win32_OperatingSystem).Version
$monitors = @([NexoraMonitors]::List())
$rows = New-Object System.Collections.Generic.List[object]
function Add-Row([string]$scenario, [string]$result, [string]$notes, [string]$artifacts = '') {
    $rows.Add([ordered]@{
        commit = $Commit; host_os = $os; backend = $Backend; gpu_driver = ($gpu -join '; ')
        monitors = ($monitors -join '; '); scenario = $scenario; result = $result
        artifacts = $artifacts; notes = $notes })
}

# Automated row: bounded real-window smoke. This is developer-host evidence, not DPI/IME proof.
$stdout = Join-Path $evidence 'smoke-stdout.log'
$stderr = Join-Path $evidence 'smoke-stderr.log'
$project = [IO.Path]::GetFullPath($ProjectPath)
$proc = Start-Process -FilePath $EditorExe -ArgumentList @(
    '--graphical', "--project=$project", "--frames=$SmokeFrames") `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru -Wait -NoNewWindow
$err = Get-Content -Raw -ErrorAction SilentlyContinue $stderr
$line = ($err -split "`n" | Where-Object { $_ -like 'graphical evidence:*' } | Select-Object -Last 1)
$ok = ($proc.ExitCode -eq 0) -and $line -and ($line -match "acquired=$SmokeFrames ") -and
      ($line -match "presented=$SmokeFrames ") -and ($line -match 'ui_rejected=0 ') -and
      ($err -notmatch 'Validation Error|VUID-|SYNC-HAZARD-|D3D12 ERROR')
Add-Row 'smoke.bounded_frames' ($(if ($ok) { 'pass' } else { 'fail' })) `
    "exit=$($proc.ExitCode); $line" "$stdout; $stderr"

# Operator rows. Answer p (pass), f (fail) or b (blocked / not performed).
$checks = @(
    @('dpi.single_monitor_scale', 'Launch the Editor at your current scale. Is text crisp and are panels, docking and clicks aligned?'),
    @('dpi.cross_monitor_move', 'Drag the Editor between two monitors with DIFFERENT scale. After the move is text re-rasterised (not blurry) and hit-testing still correct?'),
    @('ime.composition_commit', 'With Microsoft Pinyin (or another installed IME), type in the Hierarchy create-name field. Does composition show, and does committed text appear exactly once?'),
    @('ime.composition_cancel', 'Start a composition and cancel with Esc. Is nothing committed?'),
    @('ime.candidate_position_dpi_a', 'Is the candidate window next to the text cursor at the first scale?'),
    @('ime.candidate_position_dpi_b', 'Change Windows scale (or move to the other monitor). Is the candidate window still next to the cursor?'),
    @('recovery.keyboard_only', 'With a seeded journal, can Recover and Discard each be chosen using the keyboard only, with visible focus?')
)
Write-Host "`nMonitors detected: $($monitors -join ' | ')"
foreach ($check in $checks) {
    Write-Host "`n[$($check[0])] $($check[1])"
    $answer = ''
    while ($answer -notin @('p', 'f', 'b')) { $answer = (Read-Host 'p=pass f=fail b=blocked/not performed').Trim().ToLower() }
    $note = Read-Host 'notes (screenshot/video path, what you saw)'
    Add-Row $check[0] ($(switch ($answer) { 'p' { 'pass' } 'f' { 'fail' } default { 'blocked' } })) "operator-attested: $note"
}

$required = @($rows | Where-Object { $_.scenario -ne 'smoke.bounded_frames' })
$overall = if ($rows | Where-Object { $_.result -eq 'fail' }) { 'FAIL' }
           elseif ($required | Where-Object { $_.result -ne 'pass' }) { 'INCOMPLETE' } else { 'PASS' }
if (@($monitors | Select-Object -Unique).Count -lt 2) {
    Write-Host 'Fewer than two distinct monitor scales detected: cross-monitor DPI evidence cannot be complete.'
    Add-Row 'dpi.distinct_scales_available' 'blocked' "detected: $($monitors -join '; ')"
    if ($overall -eq 'PASS') { $overall = 'INCOMPLETE' }
}
$record = [ordered]@{
    schema = 'nexora.editor.windows.ed_m0.evidence.v1'; status = $overall
    recorded_utc = [DateTime]::UtcNow.ToString('o'); operator_attested_rows = $true; rows = $rows }
[IO.File]::WriteAllText((Join-Path $evidence 'evidence.json'),
    ($record | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
$md = @("# ED-M0 Windows evidence - $overall", '', "commit: $Commit", "host/os: $os",
        "backend: $Backend", "gpu/driver: $($gpu -join '; ')", "monitors: $($monitors -join '; ')", '',
        '| scenario | result | notes |', '|---|---|---|')
foreach ($row in $rows) { $md += "| $($row.scenario) | $($row.result) | $(($row.notes -replace '\|','/')) |" }
[IO.File]::WriteAllText((Join-Path $evidence 'evidence.md'), ($md -join "`n"), [Text.UTF8Encoding]::new($false))
Write-Host "`nStatus: $overall. Evidence written to $evidence"
if ($overall -eq 'PASS') { exit 0 } else { exit 1 }
