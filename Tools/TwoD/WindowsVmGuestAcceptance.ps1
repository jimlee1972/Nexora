param(
    [string]$Root,
    [int]$Cycle,
    [string]$ExpectedZipSha,
    [switch]$DefinitionsOnly,
    [switch]$ScheduleInteractive
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

function Write-DurableBytes([string]$Path, [byte[]]$Bytes) {
    $temporary = $Path + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    $stream = [IO.FileStream]::new($temporary, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::None, 4096, [IO.FileOptions]::WriteThrough)
    try { $stream.Write($Bytes, 0, $Bytes.Length); $stream.Flush($true) }
    finally { $stream.Dispose() }
    # Move refuses replacement. Neither evidence nor an older run is overwritten.
    [IO.File]::Move($temporary, $Path)
}

function Write-DurableText([string]$Path, [string]$Text) {
    Write-DurableBytes $Path ([Text.UTF8Encoding]::new($false, $true).GetBytes($Text))
}

function Read-StrictJson([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if (!$bytes.Length -or $bytes.Length -gt 1048576 -or $bytes -contains [byte]0) {
        throw 'JSON is empty, oversized or contains NUL bytes; preserve the damaged copy'
    }
    $text = [Text.UTF8Encoding]::new($false, $true).GetString($bytes)
    if ($text[0] -eq [char]0xfeff) { $text = $text.Substring(1) }
    return ($text | ConvertFrom-Json)
}

function Assert-Hash([string]$Path, [string]$Expected) {
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $Expected) {
        throw ('SHA-256 mismatch: ' + $Path)
    }
}

function Assert-Inventory([string]$Package) {
    $inventory = Read-StrictJson (Join-Path $Package 'inventory.json')
    if ($inventory.schema -ne 1 -or $inventory.development_test_copy -ne $false) {
        throw 'expected Shipping inventory'
    }
    foreach ($file in $inventory.files.PSObject.Properties) {
        Assert-Hash (Join-Path $Package $file.Name) $file.Value
    }
    return $inventory
}

function Sync-Package([string]$Package) {
    # Close/unzip alone does not promise durable data after a VM abort. Flush each staged
    # file before accepting its hashes and before running the immutable package.
    foreach ($file in [IO.Directory]::GetFiles($Package, '*', [IO.SearchOption]::AllDirectories)) {
        $stream = [IO.File]::Open($file, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::Read)
        try { $stream.Flush($true) } finally { $stream.Dispose() }
    }
}

function Copy-Fixture([string]$Source, [string]$Destination) {
    [void][IO.Directory]::CreateDirectory($Destination)
    foreach ($file in [IO.Directory]::GetFiles($Source, '*', [IO.SearchOption]::AllDirectories)) {
        $relative = $file.Substring($Source.Length).TrimStart([char]92)
        $target = Join-Path $Destination $relative
        [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))
        [IO.File]::Copy($file, $target, $false)
    }
}

function Test-FixturePanel([IntPtr]$Window) {
    if (!('NexoraVmPanel' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class NexoraVmPanel {
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X, Y; }
    [DllImport("user32.dll")] static extern bool IsWindow(IntPtr window);
    [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr window, ref Point point);
    [DllImport("user32.dll")] static extern IntPtr GetDC(IntPtr window);
    [DllImport("user32.dll")] static extern int ReleaseDC(IntPtr window, IntPtr dc);
    [DllImport("gdi32.dll")] static extern uint GetPixel(IntPtr dc, int x, int y);
    public static bool Visible(IntPtr window) {
        if (window == IntPtr.Zero || !IsWindow(window)) return false;
        IntPtr dc = GetDC(IntPtr.Zero);
        if (dc == IntPtr.Zero) return false;
        try {
            // The independent ProjectModule fixture submits this blue panel at (16,16,128,48).
            foreach (int x in new int[] {32, 100}) {
                Point p = new Point { X = x, Y = 32 };
                if (!ClientToScreen(window, ref p)) return false;
                uint pixel = GetPixel(dc, p.X, p.Y);
                if (pixel == 0xffffffff || Math.Abs((int)(pixel & 255) - 34) > 12 ||
                    Math.Abs((int)((pixel >> 8) & 255) - 102) > 12 ||
                    Math.Abs((int)((pixel >> 16) & 255) - 221) > 12) return false;
            }
            return true;
        } finally { ReleaseDC(IntPtr.Zero, dc); }
    }
}
'@
    }
    return [NexoraVmPanel]::Visible($Window)
}

function Invoke-CapturedProcess {
    param([string]$Executable, [string[]]$Arguments, [string]$LogPrefix,
          [int]$TimeoutMilliseconds = 90000, [switch]$CloseWindow)
    $process = [Diagnostics.Process]::new()
    $process.StartInfo.FileName = $Executable
    # These are controlled fixture paths/tokens, never arbitrary shell text. Quote each
    # token; reject forms that would need additional Windows command-line escaping.
    foreach ($argument in $Arguments) {
        if ($argument.Contains('"') -or $argument.EndsWith('\')) { throw 'unsupported fixture argument' }
    }
    $process.StartInfo.Arguments = ($Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.StandardOutputEncoding = [Text.Encoding]::UTF8
    $process.StartInfo.StandardErrorEncoding = [Text.Encoding]::UTF8
    $started = $false
    try {
        if (!$process.Start()) { throw 'process start failed' }
        $started = $true
        $heldHandle = $process.Handle
        # Drain both pipes while the process runs. Waiting first deadlocks on a full pipe.
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if ($CloseWindow) {
            $clock = [Diagnostics.Stopwatch]::StartNew()
            do {
                $process.Refresh()
                if ($process.HasExited) { throw 'Player exited before window close' }
                if (Test-FixturePanel $process.MainWindowHandle) { break }
                Start-Sleep -Milliseconds 100
            } while ($clock.ElapsedMilliseconds -lt 90000)
            if (!(Test-FixturePanel $process.MainWindowHandle)) { throw 'presented fixture panel unavailable' }
            if (!$process.CloseMainWindow()) { throw 'native close request failed' }
        }
        if (!$process.WaitForExit($TimeoutMilliseconds)) {
            throw 'fixture process deadline exceeded'
        }
        $exitCode = $process.ExitCode
        if ($null -eq $exitCode) { throw 'missing real process exit code' }
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
    } finally {
        if ($started) {
            if (!$process.HasExited) { $process.Kill(); [void]$process.WaitForExit(5000) }
            Write-DurableText ($LogPrefix + '.stdout.log') ($stdoutTask.GetAwaiter().GetResult())
            Write-DurableText ($LogPrefix + '.stderr.log') ($stderrTask.GetAwaiter().GetResult())
        }
        $process.Dispose()
    }
    return [pscustomobject]@{exit = [int]$exitCode; stdout = $stdout; stderr = $stderr; panel_observed = [bool]$CloseWindow}
}

function Invoke-PlayerCase {
    param([string]$Name, [string]$Package, [string[]]$Extra,
          [int]$ExpectedExit = 0, [switch]$CloseWindow)
    $arguments = @('--run-2d-project', (Join-Path $Package 'game.nx2game')) + $Extra
    $result = Invoke-CapturedProcess -Executable (Join-Path $Package 'NexoraTwoDPlayer.exe') `
        -Arguments $arguments -LogPrefix (Join-Path $Root ('cycle-' + $Cycle + '-' + $Name)) -CloseWindow:$CloseWindow
    if ($result.exit -ne $ExpectedExit) { throw ('unexpected exit: ' + $Name + ': ' + $result.stderr) }
    $case = [ordered]@{name = $Name; exit = $result.exit; stderr = $result.stderr}
    if ($CloseWindow) { $case.panel_observed = $result.panel_observed }
    if ($ExpectedExit -eq 0) {
        $report = $result.stdout | ConvertFrom-Json
        if ($report.status -cne 'PASS' -or !$report.project_mode -or !$report.allocations_balanced) {
            throw ('incomplete Player report: ' + $Name)
        }
        $minimum = 1
        if ($Name -eq 'zig-native') { $minimum = 120 }
        if ($report.native_frames -lt $minimum) { throw ('no native completion: ' + $Name) }
        $case.report = $report
    } elseif ($result.stdout.Trim()) {
        throw ('failed fixture emitted a success report: ' + $Name)
    }
    return [pscustomobject]$case
}

if ($DefinitionsOnly) { return }

try {
    if ($Root -notmatch '^C:\\NexoraVmAcceptance\\run-[0-9a-f]{32}$' -or $Cycle -lt 1 -or
            $ExpectedZipSha -cnotmatch '^[0-9a-f]{64}$') { throw 'invalid acceptance request' }
    if ($ScheduleInteractive) {
        # Guest Control runs in Session 0. DXGI/window messages require the logged-in user's desktop.
        # An InteractiveToken task uses the existing login; no password is stored in Task Scheduler.
        $taskName = 'NexoraAcceptance-' + (Split-Path -Leaf $Root) + '-' + $Cycle
        $script = Join-Path $Root 'guest.ps1'
        $hash = (Get-FileHash -LiteralPath $script).Hash.ToLowerInvariant()
        $entry = "`$p='$script';if((Get-FileHash -LiteralPath `$p).Hash.ToLowerInvariant() -ne '$hash'){throw 'script hash'};" +
            "& ([scriptblock]::Create([IO.File]::ReadAllText(`$p,[Text.UTF8Encoding]::new(`$false,`$true))))" +
            " -Root '$Root' -Cycle $Cycle -ExpectedZipSha '$ExpectedZipSha'"
        $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($entry))
        $action = New-ScheduledTaskAction -Execute (Join-Path $PSHOME 'powershell.exe') `
            -Argument ('-NoProfile -NonInteractive -WindowStyle Hidden -EncodedCommand ' + $encoded)
        $principal = New-ScheduledTaskPrincipal -UserId ([Security.Principal.WindowsIdentity]::GetCurrent().Name) `
            -LogonType Interactive -RunLevel Limited
        $settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 4) `
            -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
        # Refuse an existing task, even though a GUID collision is unlikely.
        if (Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue) { throw 'task already exists' }
        Register-ScheduledTask -TaskName $taskName -Action $action -Principal $principal -Settings $settings | Out-Null
        try {
            Start-ScheduledTask -TaskName $taskName
            $clock = [Diagnostics.Stopwatch]::StartNew()
            do {
                Start-Sleep -Milliseconds 500
                $task = Get-ScheduledTask -TaskName $taskName
                $info = Get-ScheduledTaskInfo -TaskName $taskName
                if ($info.LastRunTime.Year -gt 2000 -and $task.State -ne 'Running') { break }
            } while ($clock.Elapsed.TotalSeconds -lt 250)
            if ($task.State -eq 'Running' -or $info.LastRunTime.Year -le 2000) { throw 'interactive task deadline exceeded' }
            if ($info.LastTaskResult -ne 0) {
                $failure = Get-ChildItem -LiteralPath $Root -Filter 'failure-*.json' | Select-Object -Last 1
                if ($failure) { [Console]::Error.WriteLine([IO.File]::ReadAllText($failure.FullName)) }
                throw ('interactive task exit: ' + $info.LastTaskResult)
            }
            $completed = Read-StrictJson (Join-Path $Root ('cycle-' + $Cycle + '.json'))
            if ($completed.status -cne 'PASS' -or $completed.desktop_session -le 0) { throw 'missing interactive completion' }
            [Console]::WriteLine('INTERACTIVE_ACCEPTANCE_PASS cycle=' + $Cycle)
        } finally {
            if ((Get-ScheduledTask -TaskName $taskName).State -ne 'Running') {
                Unregister-ScheduledTask -TaskName $taskName -Confirm:$false
            }
        }
        exit 0
    }
    $desktopSession = [Diagnostics.Process]::GetCurrentProcess().SessionId
    if ($desktopSession -le 0 -or ![Environment]::UserInteractive) { throw 'native acceptance requires an interactive desktop' }
    $env:Path = 'C:\Windows\System32;C:\Windows;C:\Windows\System32\WindowsPowerShell\v1.0'
    Assert-Hash (Join-Path $Root 'package.zip') $ExpectedZipSha
    $stage = Join-Path $Root 'Unicode 封裝'
    if ($Cycle -eq 1) {
        if (Test-Path -LiteralPath $stage) { throw 'staging directory already exists' }
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [IO.Compression.ZipFile]::ExtractToDirectory((Join-Path $Root 'package.zip'), $stage)
        Sync-Package $stage
    }
    $ui = Join-Path $stage 'UI-Shipping'
    $zig = Join-Path $stage 'ZigGame-Shipping-Fixture'
    [void](Assert-Inventory $ui)
    [void](Assert-Inventory $zig)
    $primarySaves = Join-Path $Root 'durable-saves'
    $cases = @()
    $expectedVisits = 2 * ($Cycle - 1)
    $cases += Invoke-PlayerCase 'ui-first' $ui @('--frames', '3', '--native', '--save-directory', $primarySaves)
    $cases += Invoke-PlayerCase 'ui-restart' $ui @('--frames', '3', '--native', '--save-directory', $primarySaves)
    for ($index = 0; $index -lt 2; $index++) {
        $lines = $cases[$index].stderr.Replace([string][char]13, '').Split([char]10)
        if ($lines -notcontains ('project visits loaded=' + ($expectedVisits + $index))) {
            throw 'durable process/cold-boot visit state mismatch'
        }
    }
    $corruptSaves = Join-Path $Root ('negative-save-' + $Cycle)
    [void][IO.Directory]::CreateDirectory($corruptSaves)
    $slot = Join-Path $corruptSaves 'visits.nxsav'
    # Mutate only a fresh copy. The durable primary slot and all older runs stay intact.
    $bytes = [IO.File]::ReadAllBytes((Join-Path $primarySaves 'visits.nxsav'))
    $bytes[$bytes.Length - 1] = $bytes[$bytes.Length - 1] -bxor 1
    Write-DurableBytes $slot $bytes
    $corruptHash = (Get-FileHash -LiteralPath $slot).Hash.ToLowerInvariant()
    $cases += Invoke-PlayerCase 'ui-corrupt-save' $ui @('--frames', '3', '--save-directory', $corruptSaves) 1
    Assert-Hash $slot $corruptHash
    $cases += Invoke-PlayerCase 'ui-unicode-save' $ui @('--frames', '3', '--native', '--save-directory',
        (Join-Path $Root ('中文進度-' + $Cycle)))
    $cases += Invoke-PlayerCase 'zig-native' $zig @('--frames', '120', '--native', '--save-directory',
        (Join-Path $Root ('zig-saves-' + $Cycle)))
    if ($cases[-1].report.visible -lt 32) { throw 'Zig fixture sprites missing' }
    $badPackage = Join-Path $Root ('negative-cook-' + $Cycle)
    Copy-Fixture $zig $badPackage
    $cook = Get-ChildItem -LiteralPath (Join-Path $badPackage 'assets') -Filter '*.nxcook' | Select-Object -First 1
    $bytes = [IO.File]::ReadAllBytes($cook.FullName)
    $bytes[$bytes.Length - 1] = $bytes[$bytes.Length - 1] -bxor 1
    [IO.File]::WriteAllBytes($cook.FullName, $bytes)
    $cases += Invoke-PlayerCase 'zig-corrupt-cook' $badPackage @('--frames', '3') 1
    if (!$cases[-1].stderr.Contains('project cooked hash/type/identity')) { throw 'wrong Cooked rejection' }
    $cases += Invoke-PlayerCase 'ui-window-close' $ui @('--native', '--save-directory',
        (Join-Path $Root ('close-saves-' + $Cycle))) -CloseWindow
    [void](Assert-Inventory $ui)
    [void](Assert-Inventory $zig)
    Sync-Package $primarySaves
    $processes = [Diagnostics.Process]::GetProcessesByName('NexoraTwoDPlayer')
    $processCount = $processes.Length
    foreach ($process in $processes) { $process.Dispose() }
    if ($processCount -ne 0) { throw 'residual Player process' }
    $report = [ordered]@{schema = 1; status = 'PASS'; run_id = (Split-Path -Leaf $Root); cycle = $Cycle;
        hashes_verified = $true; corrupt_save_preserved = $true; player_processes = $processCount;
        desktop_session = $desktopSession;
        initial_visits = $expectedVisits; final_visits = $expectedVisits + 2; cases = $cases;
        audible_acceptance = 'NOT VERIFIED'; physical_devices = 'NOT VERIFIED'}
    Write-DurableText (Join-Path $Root ('cycle-' + $Cycle + '.json')) ($report | ConvertTo-Json -Depth 10)
    [Console]::WriteLine('GUEST_ACCEPTANCE_PASS cycle=' + $Cycle)
    exit 0
} catch {
    if ($Root -and [IO.Directory]::Exists($Root)) {
        $failure = [ordered]@{status = 'FAILED'; cycle = $Cycle; error = ($_ | Out-String)}
        Write-DurableText (Join-Path $Root ('failure-' + [Guid]::NewGuid().ToString('N') + '.json')) `
            ($failure | ConvertTo-Json -Depth 4)
    }
    [Console]::Error.WriteLine(($_ | Out-String))
    exit 1
}
