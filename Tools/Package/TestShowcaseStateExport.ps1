$ErrorActionPreference = 'Stop'
# Load the actual self-contained packaged script's reader without launching its Windows app.
$tokens = $null; $parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $PSScriptRoot 'AcceptShowcaseWindows.ps1'), [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors | Out-String) }
$reader = @($ast.FindAll({ param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -eq 'Read-ShowcaseStateSnapshot'
}, $true))
if ($reader.Count -ne 1) { throw 'Expected exactly one production snapshot reader.' }
. ([ScriptBlock]::Create($reader[0].Extent.Text))
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
public static class NexoraSnapshotPublisher {
    public static readonly ManualResetEventSlim Complete = new ManualResetEventSlim(false);
    public static Task Publish(string json, string markdown, string ready, string content) {
        return Task.Run(() => {
            using (var stream = new FileStream(json, FileMode.Create, FileAccess.Write, FileShare.None)) {
                var partial = Encoding.UTF8.GetBytes("{\"tour\":");
                stream.Write(partial, 0, partial.Length);
                stream.Flush();
                File.WriteAllText(ready, "locked");
                if (!Complete.Wait(10000)) throw new TimeoutException("Reader did not observe the fixture lock.");
                Thread.Sleep(500);
                stream.SetLength(0);
                stream.Position = 0;
                var complete = Encoding.UTF8.GetBytes(content);
                stream.Write(complete, 0, complete.Length);
                stream.Flush();
                File.WriteAllText(markdown, "# Complete snapshot\n");
            }
        });
    }
}
'@
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('nexora-snapshot-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($temporary) | Out-Null
$writer = $null
try {
    $jsonPath = Join-Path $temporary 'state.json'
    $markdownPath = Join-Path $temporary 'state.md'
    $ready = Join-Path $temporary 'ready'
    $content = '{"selected":"shipping","ticks":123,"tour":{"enabled":true,"paused":true,"step":6,"seconds":210}}'
    $writer = [NexoraSnapshotPublisher]::Publish($jsonPath, $markdownPath, $ready, $content)
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    while (-not (Test-Path -LiteralPath $ready)) {
        if ([DateTime]::UtcNow -ge $deadline) { throw 'Fixture publisher did not start.' }
        Start-Sleep -Milliseconds 10
    }
    # Reproduce the old path-exists/read race against an actual exclusive writer.
    $locked = $false
    try { $null = [IO.File]::ReadAllText($jsonPath) } catch { $locked = $true }
    if (-not $locked) { throw 'Fixture did not expose the original sharing violation.' }
    [NexoraSnapshotPublisher]::Complete.Set()
    $snapshot = Read-ShowcaseStateSnapshot $jsonPath $markdownPath ([DateTime]::UtcNow.AddSeconds(5))
    $writer.GetAwaiter().GetResult()
    if ($snapshot.Json -cne $content -or $snapshot.State.ticks -ne 123 -or
        $snapshot.State.tour.seconds -ne 210 -or $snapshot.Markdown -cne "# Complete snapshot`n") {
        throw 'Reader did not retain the complete published snapshot.'
    }
    [IO.File]::WriteAllText($jsonPath, '{"ticks":999}')
    [IO.File]::WriteAllText($markdownPath, 'replacement')
    if ($snapshot.Json -cne $content -or $snapshot.State.ticks -ne 123 -or
        $snapshot.Markdown -cne "# Complete snapshot`n") { throw 'Snapshot did not own its evidence.' }
    foreach ($invalid in @('', '{', 'null', '[]', '[{}]', '1')) {
        [IO.File]::WriteAllText($jsonPath, $invalid)
        $rejected = $false
        try { $null = Read-ShowcaseStateSnapshot $jsonPath $markdownPath ([DateTime]::UtcNow.AddMilliseconds(100)) }
        catch { $rejected = $_.Exception.Message.StartsWith('Fresh Runtime state export is missing or incomplete:') }
        if (-not $rejected) { throw "Invalid snapshot was accepted: $invalid" }
    }
    [IO.File]::WriteAllText($jsonPath, $content)
    foreach ($missing in @('empty', 'absent')) {
        if ($missing -eq 'empty') { [IO.File]::WriteAllText($markdownPath, '') }
        else { Remove-Item -LiteralPath $markdownPath }
        $rejected = $false
        try { $null = Read-ShowcaseStateSnapshot $jsonPath $markdownPath ([DateTime]::UtcNow.AddMilliseconds(100)) }
        catch { $rejected = $_.Exception.Message.StartsWith('Fresh Runtime state export is missing or incomplete:') }
        if (-not $rejected) { throw 'Missing/empty Markdown was accepted.' }
    }
    Write-Host 'PASS actual snapshot reader: sharing race, complete publication, owning evidence and eight invalid/incomplete states.'
} finally {
    [NexoraSnapshotPublisher]::Complete.Set()
    if ($null -ne $writer) { $writer.GetAwaiter().GetResult() }
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
