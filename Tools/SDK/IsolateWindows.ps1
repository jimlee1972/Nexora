# Run only on disposable hosted CI workers after installing consumer tools.
$ErrorActionPreference = 'Stop'
$identity = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
$roots = @("${env:ProgramFiles}\Microsoft Visual Studio", "${env:ProgramFiles(x86)}\Microsoft Visual Studio",
           "${env:ProgramFiles}\Windows Kits", "${env:ProgramFiles(x86)}\Windows Kits")
$evidence = @()
foreach ($root in $roots) {
    if (-not (Test-Path -LiteralPath $root)) { continue }
    $blocked = "$root.sdk-inaccessible"
    Move-Item -LiteralPath $root -Destination $blocked
    & icacls.exe $blocked /deny "${identity}:(OI)(CI)(RX)" | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Could not deny access: $blocked" }
    $readable = $false
    try { Get-ChildItem -LiteralPath $blocked -ErrorAction Stop | Out-Null; $readable = $true } catch {}
    if ($readable -or (Test-Path -LiteralPath $root)) { throw "Toolchain files still accessible: $root" }
    $evidence += @{ original = $root; relocated = $blocked; readDenied = $true }
}
if ($evidence.Count -eq 0) { throw 'No installed toolchains found to isolate' }
$env:PATH = (($env:PATH -split ';') | Where-Object {
    $_ -and $_ -notmatch 'Microsoft Visual Studio|Windows Kits|Chocolatey|mingw|LLVM'
}) -join ';'
foreach ($name in @('INCLUDE','LIB','LIBPATH','VCINSTALLDIR','VCToolsInstallDir','WindowsSdkDir','VSINSTALLDIR')) {
    Remove-Item "Env:$name" -ErrorAction SilentlyContinue
}
foreach ($tool in @('cl.exe','link.exe','dumpbin.exe','rc.exe','msbuild.exe')) {
    if (Get-Command $tool -ErrorAction SilentlyContinue) { throw "Forbidden tool remains available: $tool" }
}
$evidence | ConvertTo-Json | Set-Content -LiteralPath "$env:RUNNER_TEMP/sdk-toolchain-isolation.json" -Encoding utf8
