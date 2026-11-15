$ErrorActionPreference = 'Stop'
# Load the actual packaged reader and its consumer without launching the Windows app.
$tokens = $null; $parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $PSScriptRoot 'AcceptShowcaseWindows.ps1'), [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors | Out-String) }
foreach ($name in @('Read-ShowcaseStateSnapshot', 'Clear-StateOutput', 'Export-State', 'Require')) {
    $definition = @($ast.FindAll({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
    }, $true))
    if ($definition.Count -ne 1) { throw "Expected exactly one production function: $name" }
    . ([ScriptBlock]::Create($definition[0].Extent.Text))
}
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
    # Exercise the production Export-State consumer, including its physical-input request boundary.
    # The fake key sink publishes fixture files; the production reader and consumer stay intact.
    $staged = Join-Path $temporary 'staged'
    $evidence = Join-Path $temporary 'evidence'
    [IO.Directory]::CreateDirectory($staged) | Out-Null
    [IO.Directory]::CreateDirectory($evidence) | Out-Null
    $exportJson = Join-Path $staged 'showcase-lab.json'
    $exportMarkdown = Join-Path $staged 'showcase-lab.md'
    $exportContent = '{"schema":"nexora.showcase.rooms.v1","selected":"scene","ticks":321,"reloads":1,"lines":["Last action: Scene snapshot reload","Inspector Transform.x = 0.50 / Undo depth 0"]}'
    $keys = [Collections.Generic.List[int]]::new()
    function Press-Key([int]$key) {
        $keys.Add($key)
        if ($key -eq 88) {
            if ((Test-Path -LiteralPath $exportJson) -or (Test-Path -LiteralPath $exportMarkdown)) {
                throw 'Export-State did not discard both prior exports before requesting a fresh one.'
            }
            [IO.File]::WriteAllText($exportJson, $exportContent)
            [IO.File]::WriteAllText($exportMarkdown, "# Exported scene`n")
        }
    }
    [IO.File]::WriteAllText($exportJson, '{"ticks":999}')
    [IO.File]::WriteAllText($exportMarkdown, 'stale')
    $exported = Export-State 'scene-reload-state'
    if ($null -eq $exported -or $exported.selected -ne 'scene' -or $exported.ticks -ne 321 -or
        $exported.reloads -ne 1 -or
        $exported.lines -notcontains 'Last action: Scene snapshot reload' -or
        $exported.lines -notcontains 'Inspector Transform.x = 0.50 / Undo depth 0') {
        throw 'Export-State did not return the fresh scene state from the owning reader wrapper.'
    }
    if (($keys -join ',') -ne '114,88,114' -or
        [IO.File]::ReadAllText((Join-Path $evidence 'scene-reload-state.json')) -cne $exportContent -or
        [IO.File]::ReadAllText((Join-Path $evidence 'scene-reload-state.md')) -cne "# Exported scene`n") {
        throw 'Export-State changed the input sequence or failed to retain exact owning evidence.'
    }
    Write-Host 'PASS actual snapshot reader and Export-State consumer: sharing race, fresh request, exact owning evidence and eight invalid/incomplete states.'
} finally {
    [NexoraSnapshotPublisher]::Complete.Set()
    if ($null -ne $writer) { $writer.GetAwaiter().GetResult() }
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
