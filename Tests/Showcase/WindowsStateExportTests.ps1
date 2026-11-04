param([string]$AcceptanceScript = '')
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($AcceptanceScript)) {
    $AcceptanceScript = Join-Path $PSScriptRoot '../../Tools/Package/AcceptShowcaseWindows.ps1'
}
$tokens = $null
$errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($AcceptanceScript, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw 'Acceptance script has parser errors.' }
foreach ($name in @('Clear-StateOutput', 'Export-State')) {
    $function = $ast.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
    }, $true)
    if ($null -eq $function) { throw "Missing production function: $name" }
    . ([scriptblock]::Create($function.Extent.Text))
}
Add-Type @'
using System;
using System.IO;
using System.Threading;
public static class DelayedShowcaseExport {
    public static Thread Start(string directory, bool complete) {
        var thread = new Thread(() => {
            string json = Path.Combine(directory, "showcase-lab.json");
            string markdown = Path.Combine(directory, "showcase-lab.md");
            File.WriteAllText(json, "");
            File.WriteAllText(markdown, "");
            Thread.Sleep(150);
            File.WriteAllText(json, "{\"schema\":");
            File.WriteAllText(markdown, "# Runtime\n");
            Thread.Sleep(150);
            if (complete) File.WriteAllText(json,
                "{\"schema\":\"nexora.showcase.rooms.v1\",\"tour\":{\"step\":6,\"seconds\":210.028}}");
        });
        thread.Start();
        return thread;
    }
}
'@
function Require($condition, [string]$message) { if (-not $condition) { throw $message } }
function Press-Key($key) {
    if ($key -eq 88) { $script:writer = [DelayedShowcaseExport]::Start($staged, $script:complete) }
}
$root = Join-Path ([IO.Path]::GetTempPath()) ('nexora-export-test-' + [guid]::NewGuid())
$staged = Join-Path $root 'staged'
$evidence = Join-Path $root 'evidence'
New-Item -ItemType Directory -Path $staged, $evidence | Out-Null
try {
    $script:complete = $true
    # Stale exports must be deleted before a new X export is requested.
    [IO.File]::WriteAllText((Join-Path $staged 'showcase-lab.json'), '{"tour":{"step":0}}')
    [IO.File]::WriteAllText((Join-Path $staged 'showcase-lab.md'), 'stale')
    $snapshot = Export-State 'complete'
    $script:writer.Join()
    Require ($snapshot.tour.step -eq 6 -and $snapshot.tour.seconds -eq 210.028) 'Fresh complete tour was not retained.'
    $recorded = Get-Content -Raw (Join-Path $evidence 'complete.json') | ConvertFrom-Json
    Require ($recorded.tour.step -eq 6) 'Evidence differs from the accepted snapshot.'
    Require ((Get-Content -Raw (Join-Path $evidence 'complete.md')) -eq "# Runtime`n") 'Markdown evidence is incomplete.'
    $script:complete = $false
    $rejected = $false
    $clock = [Diagnostics.Stopwatch]::StartNew()
    try { Export-State 'incomplete' | Out-Null } catch {
        if ($_.Exception.Message -notlike 'Fresh*Runtime state export is missing.') { throw }
        $rejected = $true
    }
    $script:writer.Join()
    Require $rejected 'An incomplete export was accepted.'
    Require ($clock.Elapsed.TotalSeconds -lt 7) 'Original five-second export bound was not retained.'
    Require (-not (Test-Path (Join-Path $evidence 'incomplete.json'))) 'Rejected output was published as evidence.'
    Write-Output 'Showcase export race and bounded rejection passed.'
} finally {
    if ($null -ne $script:writer) { $script:writer.Join() }
    Remove-Item -LiteralPath $root -Recurse -Force
}
