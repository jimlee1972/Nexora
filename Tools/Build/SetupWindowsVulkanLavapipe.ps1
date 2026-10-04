[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$Directory)
$ErrorActionPreference = 'Stop'
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw 'Windows is required for the lavapipe CI driver.'
}
# CI host dependency only: never install into the system or the Showcase package.
$version = '26.2.4'
$archiveName = "mesa3d-$version-release-msvc.7z"
$expectedHash = '351fc8c8b695878ffb3eaa044b3ead08672a48b1a045e3c3e3975811df0f6695'
$url = "https://github.com/pal1000/mesa-dist-win/releases/download/$version/$archiveName"
$root = [IO.Path]::GetFullPath($Directory)
[IO.Directory]::CreateDirectory($root) | Out-Null
$archive = Join-Path $root $archiveName
Invoke-WebRequest -Uri $url -OutFile $archive
$actualHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualHash -ne $expectedHash) { throw 'Mesa archive SHA-256 mismatch.' }
$sevenZip = (Get-Command 7z.exe -ErrorAction Stop).Source
& $sevenZip x $archive "-o$root/driver" -y | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Mesa extraction failed: $LASTEXITCODE" }
$manifests = @(Get-ChildItem -LiteralPath (Join-Path $root 'driver') -Recurse -File -Filter 'lvp_icd.x86_64.json')
if ($manifests.Count -ne 1) { throw 'Expected one x64 lavapipe ICD manifest.' }
$original = $manifests[0]
$icd = Get-Content -LiteralPath $original.FullName -Raw | ConvertFrom-Json
$library = [IO.Path]::GetFullPath((Join-Path $original.DirectoryName $icd.ICD.library_path))
if (-not (Test-Path -LiteralPath $library -PathType Leaf)) { throw 'Lavapipe DLL is missing.' }
# Use an absolute library path so isolated-copy launches resolve this exact CI driver.
$icd.ICD.library_path = $library
$manifest = Join-Path $root 'nexora-lvp-icd.json'
[IO.File]::WriteAllText($manifest, ($icd | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
$env:VK_DRIVER_FILES = $manifest
$env:NEXORA_CI_VULKAN_DRIVER_LIBRARY = $library
if ($env:GITHUB_ENV) {
    Add-Content -LiteralPath $env:GITHUB_ENV -Value "VK_DRIVER_FILES=$manifest"
    Add-Content -LiteralPath $env:GITHUB_ENV -Value "NEXORA_CI_VULKAN_DRIVER_LIBRARY=$library"
}
$provenance = [ordered]@{
    schema = 'nexora.showcase.ci.vulkan_driver.v1'
    driver = 'Mesa lavapipe'; version = $version; software_rasterizer = $true
    source_url = $url; archive_sha256 = $actualHash
    library = [IO.Path]::GetFileName($library)
    library_sha256 = (Get-FileHash -LiteralPath $library -Algorithm SHA256).Hash.ToLowerInvariant()
    api_version = $icd.ICD.api_version
    scope = 'Hosted Windows software-driver coverage; no physical GPU or clean-host attestation'
}
[IO.File]::WriteAllText((Join-Path $root 'driver-provenance.json'),
    ($provenance | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
Write-Host "Selected checksum-verified Mesa $version x64 lavapipe for native Vulkan CI."
