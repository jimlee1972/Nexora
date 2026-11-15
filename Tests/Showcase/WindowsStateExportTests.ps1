param([string]$AcceptanceScript = '')
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($AcceptanceScript)) {
    $AcceptanceScript = Join-Path $PSScriptRoot '../../Tools/Package/AcceptShowcaseWindows.ps1'
}
$tokens = $null
$errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($AcceptanceScript, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw 'Acceptance script has parser errors.' }
foreach ($name in @('Read-ShowcaseStateSnapshot', 'Clear-StateOutput', 'Export-State')) {
    $function = $ast.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
    }, $true)
    if ($null -eq $function) { throw "Missing production function: $name" }
    . ([scriptblock]::Create($function.Extent.Text))
}
Add-Type @'
using System;
using System.IO;
using System.Text;
using System.Threading;
public static class DelayedShowcaseExport {
    public static readonly ManualResetEventSlim Ready = new ManualResetEventSlim(false);
    public static readonly ManualResetEventSlim Release = new ManualResetEventSlim(false);
    public static Exception Failure;
    static void Write(FileStream stream, string text) {
        byte[] bytes = Encoding.UTF8.GetBytes(text);
        stream.Position = 0;
        stream.SetLength(0);
        stream.Write(bytes, 0, bytes.Length);
        stream.Flush();
    }
    public static Thread Start(string directory, bool complete) {
        return Start(directory, complete, false);
    }
    public static Thread Start(string directory, bool complete, bool holdOpen) {
        Ready.Reset();
        Release.Reset();
        Failure = null;
        var thread = new Thread(() => {
            try {
                // Like ShowcaseRooms' two ofstreams, retain both handles through the export.
                // Reopening WriteAllText between phases lets a polling reader deny the next write.
                using (var json = new FileStream(Path.Combine(directory, "showcase-lab.json"),
                                                FileMode.Create, FileAccess.Write, FileShare.ReadWrite))
                using (var markdown = new FileStream(Path.Combine(directory, "showcase-lab.md"),
                                                    FileMode.Create, FileAccess.Write, FileShare.ReadWrite)) {
                    Thread.Sleep(150); // Both files exist but are still empty.
                    Write(json, "{\"schema\":");
                    Write(markdown, "# Runtime\n");
                    Thread.Sleep(150);
                    if (complete) Write(json,
                        "{\"schema\":\"nexora.showcase.rooms.v1\",\"tour\":{\"step\":6,\"seconds\":210.028}}");
                    Ready.Set();
                    if (holdOpen && !Release.Wait(5000))
                        throw new TimeoutException("Writer close was not released.");
                }
            } catch (Exception error) {
                // Surface producer failures on the PowerShell owner instead of crashing its process.
                Failure = error;
            } finally {
                Ready.Set();
            }
        });
        thread.Start();
        return thread;
    }
}
'@
function Require($condition, [string]$message) { if (-not $condition) { throw $message } }
function Join-ExportWriter {
    Require ($script:writer.Join(3000)) 'Export writer did not drain.'
    if ($null -ne [DelayedShowcaseExport]::Failure) { throw [DelayedShowcaseExport]::Failure }
}
function Press-Key($key) {
    if ($key -eq 88) { $script:writer = [DelayedShowcaseExport]::Start($staged, $script:complete) }
}
$root = Join-Path ([IO.Path]::GetTempPath()) ('nexora-export-test-' + [guid]::NewGuid())
$staged = Join-Path $root 'staged'
$evidence = Join-Path $root 'evidence'
New-Item -ItemType Directory -Path $staged, $evidence | Out-Null
try {
    # An actual producer I/O failure must reach the owner, rather than terminate PowerShell.
    $script:writer = [DelayedShowcaseExport]::Start((Join-Path $root 'missing'), $true)
    $producerRejected = $false
    try { Join-ExportWriter } catch [IO.DirectoryNotFoundException] { $producerRejected = $true }
    Require $producerRejected 'Background writer failure was not reported to the owner.'
    $script:writer = $null
    if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
        # Fully flushed valid bytes are not publication while the real writer still owns handles.
        $script:writer = [DelayedShowcaseExport]::Start($staged, $true, $true)
        Require ([DelayedShowcaseExport]::Ready.Wait(3000)) 'Export writer was not ready.'
        if ($null -ne [DelayedShowcaseExport]::Failure) { throw [DelayedShowcaseExport]::Failure }
        $openRejected = $false
        try {
            try {
                Read-ShowcaseStateSnapshot (Join-Path $staged 'showcase-lab.json') `
                    (Join-Path $staged 'showcase-lab.md') ([DateTime]::UtcNow.AddMilliseconds(200)) | Out-Null
            } catch {
                if (-not $_.Exception.Message.StartsWith('Fresh Runtime state export is missing or incomplete:')) { throw }
                $openRejected = $true
            }
        } finally {
            [DelayedShowcaseExport]::Release.Set()
            Join-ExportWriter
        }
        Require $openRejected 'Valid bytes were accepted before the writer closed.'
        $closed = Read-ShowcaseStateSnapshot (Join-Path $staged 'showcase-lab.json') `
            (Join-Path $staged 'showcase-lab.md') ([DateTime]::UtcNow.AddSeconds(5))
        Require ($closed.State.tour.step -eq 6 -and $closed.State.tour.seconds -eq 210.028) `
            'Closed writer snapshot was not accepted.'
    } else {
        Write-Output 'Windows handle-sharing assertion requires Windows; portable export checks follow.'
    }
    $script:complete = $true
    # Stale exports must be deleted before a new X export is requested.
    [IO.File]::WriteAllText((Join-Path $staged 'showcase-lab.json'), '{"tour":{"step":0}}')
    [IO.File]::WriteAllText((Join-Path $staged 'showcase-lab.md'), 'stale')
    $snapshot = Export-State 'complete'
    Join-ExportWriter
    Require ($snapshot.tour.step -eq 6 -and $snapshot.tour.seconds -eq 210.028) 'Fresh complete tour was not retained.'
    $recorded = Get-Content -Raw (Join-Path $evidence 'complete.json') | ConvertFrom-Json
    Require ($recorded.tour.step -eq 6) 'Evidence differs from the accepted snapshot.'
    Require ((Get-Content -Raw (Join-Path $evidence 'complete.md')) -eq "# Runtime`n") 'Markdown evidence is incomplete.'
    $script:complete = $false
    $rejected = $false
    $clock = [Diagnostics.Stopwatch]::StartNew()
    try { Export-State 'incomplete' | Out-Null } catch {
        if (-not $_.Exception.Message.StartsWith('Fresh Runtime state export is missing or incomplete:')) { throw }
        $rejected = $true
    }
    Join-ExportWriter
    Require $rejected 'An incomplete export was accepted.'
    Require ($clock.Elapsed.TotalSeconds -lt 7) 'Original five-second export bound was not retained.'
    Require (-not (Test-Path (Join-Path $evidence 'incomplete.json'))) 'Rejected output was published as evidence.'
    Write-Output 'Showcase export race and bounded rejection passed.'
} finally {
    [DelayedShowcaseExport]::Release.Set()
    try {
        if ($null -ne $script:writer) { Join-ExportWriter }
    } finally {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}
