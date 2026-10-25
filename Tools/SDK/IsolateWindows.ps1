# Run only on disposable hosted CI workers after installing consumer tools.
$ErrorActionPreference = 'Stop'
$identity = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
$consumerPaths = @('cmake.exe','python.exe','zig.exe') | ForEach-Object {
    Split-Path -Parent (Get-Command $_ -ErrorAction Stop).Source
}
$consumerPaths += @("$env:SystemRoot\System32", "$env:SystemRoot", $PSHOME, "$env:ProgramFiles\Git\cmd")
$roots = @("${env:ProgramFiles}\Microsoft Visual Studio", "${env:ProgramFiles(x86)}\Microsoft Visual Studio",
           "${env:ProgramFiles}\Windows Kits", "${env:ProgramFiles(x86)}\Windows Kits")
$evidence = @()
foreach ($root in $roots) {
    if (-not (Test-Path -LiteralPath $root)) { continue }
    $probes = @(Get-ChildItem -Path @(
        "$root\*\*\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe",
        "$root\*\*\VC\Tools\MSVC\*\lib\x64\libcmt.lib",
        "$root\*\Include\*\um\Windows.h", "$root\*\Lib\*\um\x64\kernel32.lib",
        "$root\Installer\vswhere.exe") -File -ErrorAction SilentlyContinue)
    if ($probes.Count -eq 0) {
        $probes = @(Get-ChildItem -LiteralPath $root -File -Recurse | Select-Object -First 1)
    }
    if ($probes.Count -eq 0) { throw "No toolchain files found for an isolation probe: $root" }
    $blocked = "$root.sdk-inaccessible"
    Move-Item -LiteralPath $root -Destination $blocked
    & icacls.exe $blocked /deny "${identity}:(OI)(CI)(RX)" | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Could not deny access: $blocked" }
    $readable = $false
    try { Get-ChildItem -LiteralPath $blocked -ErrorAction Stop | Out-Null; $readable = $true } catch {}
    if ($readable -or (Test-Path -LiteralPath $root)) { throw "Toolchain files still accessible: $root" }
    $fileEvidence = @()
    foreach ($probe in $probes) {
        $path = $blocked + $probe.FullName.Substring($root.Length)
        $stream = $null; $fileReadable = $false
        try {
            $stream = [System.IO.File]::OpenRead($path)
            $null = $stream.ReadByte()
            $fileReadable = $true
        } catch {
            if (-not ($_.Exception -is [UnauthorizedAccessException]) -and
                -not ($_.Exception.InnerException -is [UnauthorizedAccessException])) { throw }
        } finally { if ($null -ne $stream) { $stream.Dispose() } }
        if ($fileReadable) { throw "Toolchain file bytes remain readable: $path" }
        $fileEvidence += @{ relative = $probe.FullName.Substring($root.Length); readDenied = $true }
    }
    $evidence += @{ original = $root; relocated = $blocked; readDenied = $true; fileProbes = $fileEvidence }
}
if ($evidence.Count -eq 0) { throw 'No installed toolchains found to isolate' }
# Use only consumer tools and OS paths. Hosted Git/Strawberry ship unrelated
# coreutils link.exe binaries; a regex blacklist cannot establish tool isolation.
$env:PATH = ($consumerPaths | Select-Object -Unique) -join ';'
$evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$env:RUNNER_TEMP/sdk-toolchain-isolation.json" -Encoding utf8
foreach ($name in @('INCLUDE','LIB','LIBPATH','VCINSTALLDIR','VCToolsInstallDir','WindowsSdkDir','VSINSTALLDIR')) {
    Remove-Item "Env:$name" -ErrorAction SilentlyContinue
}
foreach ($tool in @('cl.exe','link.exe','dumpbin.exe','rc.exe','msbuild.exe')) {
    if (Get-Command $tool -ErrorAction SilentlyContinue) { throw "Forbidden tool remains available: $tool ($((Get-Command $tool).Source))" }
}
$evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$env:RUNNER_TEMP/sdk-toolchain-isolation.json" -Encoding utf8
