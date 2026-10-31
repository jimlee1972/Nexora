param([string]$GuestScript)
$ErrorActionPreference = 'Stop'
. ([scriptblock]::Create([IO.File]::ReadAllText($GuestScript, [Text.UTF8Encoding]::new($false, $true)))) -DefinitionsOnly
$root = Join-Path ([IO.Path]::GetTempPath()) ('nexora-vm-guest-tests-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($root)
try {
    $hashVector = Join-Path $root 'sha256-vector.txt'
    Write-DurableText $hashVector 'abc'
    if ((Get-FileSha256 $hashVector) -cne 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad') {
        throw 'file SHA-256 known vector failed'
    }
    if (Test-FixturePanel ([IntPtr]::Zero)) { throw 'invalid window admitted as presented' }
    # Actual Shipping DX12 capture: both panel samples are sRGB (102,170,239).
    # The prior unencoded expectation rejected this presented frame.
    if (![NexoraVmPanel]::ExpectedColor(0x00efaa66) -or
            [NexoraVmPanel]::ExpectedColor(0x00dd6622) -or
            [NexoraVmPanel]::ExpectedColor(0) -or
            [NexoraVmPanel]::ExpectedColor([uint32]::MaxValue)) {
        throw 'sRGB panel acceptance or unrelated/invalid pixel rejection failed'
    }
    $zero = Join-Path $root 'zero.json'
    [IO.File]::WriteAllBytes($zero, [byte[]]::new(806))
    $rejected = $false
    try { Read-StrictJson $zero | Out-Null } catch { $rejected = $_.Exception.Message.Contains('NUL') }
    if (!$rejected) { throw 'zero-filled inventory was admitted' }
    $unicode = Join-Path $root 'unicode.json'
    $chinese = [string][char]0x4e2d + [char]0x6587
    Write-DurableText $unicode ('{"text":"' + $chinese + '"}')
    if ((Read-StrictJson $unicode).text -cne $chinese) { throw 'UTF-8 round trip failed' }
    $before = [IO.File]::ReadAllBytes($unicode)
    $rejected = $false
    try { Write-DurableText $unicode 'replacement' } catch { $rejected = $true }
    if (!$rejected -or [Convert]::ToBase64String($before) -cne
            [Convert]::ToBase64String([IO.File]::ReadAllBytes($unicode))) { throw 'existing evidence replaced' }
    $child = "[Console]::Out.Write(('O'*200000));[Console]::Error.Write(('E'*200000));exit 7"
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($child))
    $result = Invoke-CapturedProcess -Executable (Join-Path $PSHOME 'powershell.exe') `
        -Arguments @('-NoProfile', '-NonInteractive', '-EncodedCommand', $encoded) `
        -LogPrefix (Join-Path $root 'pipes') -TimeoutMilliseconds 30000
    if ($result.exit -ne 7 -or $result.stdout.Length -ne 200000 -or $result.stderr.Length -ne 200000) {
        throw 'real process exit or concurrent pipe drain failed'
    }
    $fixture = Join-Path $root 'worker-fixture.ps1'
    Write-DurableText $fixture 'param($Root,$Cycle,$ExpectedZipSha);[Console]::WriteLine("WORKER_EXECUTED");exit 7'
    $entry = New-InteractiveWorkerEntry -Root $root -Script $fixture -Cycle 1 -ExpectedZipSha ('0' * 64)
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($entry))
    $worker = Invoke-CapturedProcess -Executable (Join-Path $PSHOME 'powershell.exe') `
        -Arguments @('-NoProfile', '-NonInteractive', '-EncodedCommand', $encoded) `
        -LogPrefix (Join-Path $root 'worker') -TimeoutMilliseconds 30000
    if ($worker.exit -ne 7 -or !$worker.stdout.Contains('WORKER_EXECUTED')) { throw 'worker invocation lost real exit' }
    foreach ($phase in @('started', 'hash-verified', 'script-parsed')) {
        if ([IO.File]::ReadAllText((Join-Path $root ('worker-1-' + $phase + '.txt'))) -cne $phase) {
            throw 'worker phase evidence missing'
        }
    }
    $entry = New-InteractiveWorkerEntry -Root $root -Script $fixture -Cycle 2 -ExpectedZipSha ('0' * 64)
    [IO.File]::AppendAllText($fixture, ';# changed after entry admission')
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($entry))
    $worker = Invoke-CapturedProcess -Executable (Join-Path $PSHOME 'powershell.exe') `
        -Arguments @('-NoProfile', '-NonInteractive', '-EncodedCommand', $encoded) `
        -LogPrefix (Join-Path $root 'worker-mutated') -TimeoutMilliseconds 30000
    if ($worker.exit -eq 0 -or $worker.stdout.Contains('WORKER_EXECUTED') -or
            !(Test-Path -LiteralPath (Join-Path $root 'worker-2-started.txt')) -or
            (Test-Path -LiteralPath (Join-Path $root 'worker-2-hash-verified.txt'))) {
        throw 'changed worker script was admitted or its failure phase lost'
    }
    [Console]::WriteLine('Windows guest UTF-8/NUL, immutable evidence, worker hash/phases and full pipe regressions PASS')
} finally {
    # This GUID directory was created by this test, never a user save directory.
    [IO.Directory]::Delete($root, $true)
}
