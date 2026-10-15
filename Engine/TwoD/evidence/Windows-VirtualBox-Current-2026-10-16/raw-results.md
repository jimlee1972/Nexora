# Current-package VirtualBox raw observations

Original payloads from this run; the generated package manifest and Zig reports keep their original scope fields. See [acceptance](acceptance.md) for the supplemental VM assessment.

## package/manifest.json

```json
{
  "schema": "nexora.twod.package.v1",
  "files": {
    "NexoraTwoDBenchmark.exe": "87cba2b4d375adbc129ce45b96d32d8c87fc2bb7af0b2e963d3709f1c466bfc4",
    "assets/atlas.nxcook": "c446ad04a2b67a6d7c617beea9fdc3355e1ad365f8f192318eae04ea29510ca9",
    "assets/clip.nxcook": "7cf3bfe2b304001474e3a1e98a160e7c211ed82555cab9f5a07e716d822352cc",
    "NexoraZigTwoD.exe": "99754037d9d2896c8941373b2a0042cc261359cca06ef601eb4be36440679374"
  },
  "runtime": "NexoraTwoDBenchmark.exe",
  "editor_required": false,
  "system_runtime_dependencies": "Use the target OS runtime and native graphics driver.",
  "windows_clean_machine_acceptance": "NOT RUN",
  "source_commit": "350c40eef0044c9e96607fb144a4369927b7b4f5",
  "backends": [
    "DX12"
  ],
  "configuration": "Monolithic Shipping"
}
```

## guest-evidence/inventory.json

```json
{
    "os":  "Microsoft Windows NT 10.0.19045.0",
    "dev_tools":  [

                  ],
    "runtime":  [
                    {
                        "file":  "vcruntime140.dll",
                        "present":  true
                    },
                    {
                        "file":  "vcruntime140_1.dll",
                        "present":  true
                    },
                    {
                        "file":  "msvcp140.dll",
                        "present":  true
                    },
                    {
                        "file":  "vulkan-1.dll",
                        "present":  false
                    }
                ]
}
```

## guest-evidence/hashes.json

```json
[
    {
        "file":  "NexoraTwoDBenchmark.exe",
        "expected":  "87cba2b4d375adbc129ce45b96d32d8c87fc2bb7af0b2e963d3709f1c466bfc4",
        "actual":  "87cba2b4d375adbc129ce45b96d32d8c87fc2bb7af0b2e963d3709f1c466bfc4",
        "match":  true
    },
    {
        "file":  "assets/atlas.nxcook",
        "expected":  "c446ad04a2b67a6d7c617beea9fdc3355e1ad365f8f192318eae04ea29510ca9",
        "actual":  "c446ad04a2b67a6d7c617beea9fdc3355e1ad365f8f192318eae04ea29510ca9",
        "match":  true
    },
    {
        "file":  "assets/clip.nxcook",
        "expected":  "7cf3bfe2b304001474e3a1e98a160e7c211ed82555cab9f5a07e716d822352cc",
        "actual":  "7cf3bfe2b304001474e3a1e98a160e7c211ed82555cab9f5a07e716d822352cc",
        "match":  true
    },
    {
        "file":  "NexoraZigTwoD.exe",
        "expected":  "99754037d9d2896c8941373b2a0042cc261359cca06ef601eb4be36440679374",
        "actual":  "99754037d9d2896c8941373b2a0042cc261359cca06ef601eb4be36440679374",
        "match":  true
    }
]
```

## guest-evidence/results.json

```json
[
    {
        "name":  "cpu",
        "exe":  "NexoraTwoDBenchmark.exe",
        "finished":  true,
        "exit_code":  0,
        "elapsed_seconds":  6.7991706,
        "arguments":  "--atlas assets/atlas.nxcook --clip assets/clip.nxcook --frames 48 --report C:\\NexoraM7Current20261015/cpu.csv"
    },
    {
        "name":  "corrupt",
        "exe":  "NexoraTwoDBenchmark.exe",
        "finished":  true,
        "exit_code":  1,
        "elapsed_seconds":  0.2853041,
        "arguments":  "--atlas C:\\NexoraM7Current20261015/corrupt.nxcook --frames 4 --report C:\\NexoraM7Current20261015/corrupt.csv"
    },
    {
        "name":  "native",
        "exe":  "NexoraTwoDBenchmark.exe",
        "finished":  true,
        "exit_code":  0,
        "elapsed_seconds":  20.6396342,
        "arguments":  "--native --atlas assets/atlas.nxcook --clip assets/clip.nxcook --frames 8 --report C:\\NexoraM7Current20261015/native.csv"
    },
    {
        "name":  "zig-native",
        "exe":  "NexoraZigTwoD.exe",
        "finished":  true,
        "exit_code":  0,
        "elapsed_seconds":  9.0127411,
        "arguments":  "--native --frames 120 --report C:\\NexoraM7Current20261015/zig-native.json"
    }
]
```

## guest-evidence/cpu.csv

```csv
objects,sparse,reload_ms,overlap,native,frames,warmup_frames,loading_ms,cpu_p50_ms,cpu_p95_ms,cpu_p99_ms,cpu_max_ms,frame_p95_ms,gpu_samples,gpu_p95_ms,peak_process_rss_bytes,visible,culled,sprite_batches,covered_aabb_ratio,query_hits,frame_average_ms,frame_p99_ms,frame_max_ms,device_name,vendor_id,device_id,driver_version,driver_version_format
100,0,0.0692,0,0,48,4,3.4761,0.2552,0.5609,3.2951,3.2951,0.5611,0,,0,100,0,1,0.00146484,0,0.367942,3.2953,3.2953,"",0,0,0,0
1000,0,0.7937,0,0,48,4,0.9746,2.6171,7.4605,9.3961,9.3961,7.4639,0,,0,1000,0,1,0.027832,0,3.32161,9.3964,9.3964,"",0,0,0,0
5000,0,11.5716,0,0,48,4,3.3783,17.6826,28.396,32.8495,32.8495,28.399,0,,0,5000,0,1,0.14502,6864,18.7823,32.8499,32.8499,"",0,0,0,0
10000,0,11.123,0,0,48,4,9.5921,41.3898,59.031,97.6487,97.6487,59.0314,0,,0,9700,300,1,0.28125,11076,42.9418,97.6492,97.6492,"",0,0,0,0
1000,0,0.7578,1,0,48,4,0.7458,3.8149,10.4074,12.4404,12.4404,10.4079,0,,0,1000,0,1,5.20833,52000,5.07879,12.4406,12.4406,"",0,0,0,0
10000,1,9.2331,0,0,48,4,15.3185,41.7095,48.1763,50.6525,50.6525,48.7362,0,,0,195,9805,1,0.00509203,260,42.1779,50.6529,50.6529,"",0,0,0,0
```

## guest-evidence/native.csv

```csv
objects,sparse,reload_ms,overlap,native,frames,warmup_frames,loading_ms,cpu_p50_ms,cpu_p95_ms,cpu_p99_ms,cpu_max_ms,frame_p95_ms,gpu_samples,gpu_p95_ms,peak_process_rss_bytes,visible,culled,sprite_batches,covered_aabb_ratio,query_hits,frame_average_ms,frame_p99_ms,frame_max_ms,device_name,vendor_id,device_id,driver_version,driver_version_format
100,0,65.2967,0,1,8,4,3031.36,0.3529,1.3784,1.3784,1.3784,69.236,6,11.7379,0,100,0,1,0.00146484,0,31.0237,69.236,69.236,"Microsoft Basic Render Driver",5140,140,2814751014981172,2
1000,0,47.7527,0,1,8,4,2376.2,2.1344,4.8647,4.8647,4.8647,52.6238,6,9.3872,0,1000,0,1,0.027832,0,32.7819,52.6238,52.6238,"Microsoft Basic Render Driver",5140,140,2814751014981172,2
5000,0,25.879,0,1,8,4,2407.08,21.3757,41.4051,41.4051,41.4051,57.6825,6,31.729,0,5000,0,1,0.14502,1584,37.7027,57.6825,57.6825,"Microsoft Basic Render Driver",5140,140,2814751014981172,2
10000,0,83.2591,0,1,8,4,2289.77,38.6271,66.0122,66.0122,66.0122,144.24,6,58.2015,0,9700,300,1,0.28125,2556,72.3491,144.24,144.24,"Microsoft Basic Render Driver",5140,140,2814751014981172,2
1000,0,114.87,1,1,8,4,2611.01,4.1497,9.977,9.977,9.977,121.543,6,52.6762,0,1000,0,1,5.20833,12000,60.7998,121.543,121.543,"Microsoft Basic Render Driver",5140,140,2814751014981172,2
10000,1,30.2754,0,1,8,4,2511.34,48.8953,65.2671,65.2671,65.2671,91.8106,6,15.5685,0,195,9805,1,0.00509672,60,57.792,91.8106,91.8106,"Microsoft Basic Render Driver",5140,140,2814751014981172,2
```

## guest-evidence/zig-native.json

```json
{"status":"PASS","language":"Zig 0.14","api":1,"native_frames":120,"movement_frames":120,"entity_x":2.000000,"sprite_instanced_draws":120,"sprite_gpu_instances":4080,"sprite_instance_upload_bytes":179520,"visible":34,"reloaded":true,"allocations_balanced":true,"physical_windows":"NOT RUN"}
```

## guest-evidence/zig-capture.json

```json
{"status":"PASS","language":"Zig 0.14","api":1,"native_frames":1800,"movement_frames":1800,"entity_x":30.000000,"sprite_instanced_draws":1800,"sprite_gpu_instances":59590,"sprite_instance_upload_bytes":2621960,"visible":31,"reloaded":true,"allocations_balanced":true,"physical_windows":"NOT RUN"}
```

## guest-evidence/zig-desktop.json

```json
{"status":"PASS","language":"Zig 0.14","api":1,"native_frames":1800,"movement_frames":1800,"entity_x":30.000000,"sprite_instanced_draws":1800,"sprite_gpu_instances":59590,"sprite_instance_upload_bytes":2621960,"visible":31,"reloaded":true,"allocations_balanced":true,"physical_windows":"NOT RUN"}
```

## guest-evidence/desktop-task.json

```json
{
    "state":  "Ready",
    "last_result":  0,
    "report_present":  true
}
```

## guest-evidence/corrupt.stderr.txt

```text
invalid cooked asset
```

## dependencies-benchmark.txt

```text
Microsoft (R) COFF/PE Dumper Version 14.44.35228.0
Copyright (C) Microsoft Corporation.  All rights reserved.


Dump of file build\m7-vm-2026-10-15\package\NexoraTwoDBenchmark.exe

File Type: EXECUTABLE IMAGE

  Image has the following dependencies:

    IMM32.dll
    USER32.dll
    d3d12.dll
    D3DCOMPILER_47.dll
    dxgi.dll
    KERNEL32.dll
    MSVCP140.dll
    VCRUNTIME140_1.dll
    VCRUNTIME140.dll
    api-ms-win-crt-stdio-l1-1-0.dll
    api-ms-win-crt-heap-l1-1-0.dll
    api-ms-win-crt-runtime-l1-1-0.dll
    api-ms-win-crt-filesystem-l1-1-0.dll
    api-ms-win-crt-math-l1-1-0.dll
    api-ms-win-crt-locale-l1-1-0.dll

  Summary

        2000 .data
        3000 .pdata
       1E000 .rdata
        1000 .reloc
        1000 .rsrc
       45000 .text
```

## dependencies-zig.txt

```text
Microsoft (R) COFF/PE Dumper Version 14.44.35228.0
Copyright (C) Microsoft Corporation.  All rights reserved.


Dump of file build\m7-vm-2026-10-15\package\NexoraZigTwoD.exe

File Type: EXECUTABLE IMAGE

  Image has the following dependencies:

    ntdll.dll
    IMM32.dll
    USER32.dll
    d3d12.dll
    D3DCOMPILER_47.dll
    dxgi.dll
    KERNEL32.dll
    MSVCP140.dll
    VCRUNTIME140_1.dll
    VCRUNTIME140.dll
    api-ms-win-crt-stdio-l1-1-0.dll
    api-ms-win-crt-heap-l1-1-0.dll
    api-ms-win-crt-runtime-l1-1-0.dll
    api-ms-win-crt-filesystem-l1-1-0.dll
    api-ms-win-crt-math-l1-1-0.dll
    api-ms-win-crt-locale-l1-1-0.dll

  Summary

        3000 .data
        6000 .pdata
       57000 .rdata
        1000 .reloc
        1000 .rsrc
       9A000 .text
```

## guest-current-tests.ps1

```powershell
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$root='C:\NexoraM7Current20261015'
$pkg=Join-Path $root 'package'
Set-Location $pkg
$env:PATH="$env:windir\System32;$env:windir;$env:windir\System32\Wbem"
Add-Type 'using System;using System.Runtime.InteropServices;public static class ErrorMode{[DllImport("kernel32.dll")]public static extern uint SetErrorMode(uint m);}'
[void][ErrorMode]::SetErrorMode(0x8003)
$manifest=Get-Content manifest.json -Raw | ConvertFrom-Json
$hashes=@(foreach($file in $manifest.files.PSObject.Properties){$actual=(Get-FileHash $file.Name -Algorithm SHA256).Hash.ToLowerInvariant();[pscustomobject]@{file=$file.Name;expected=$file.Value;actual=$actual;match=($actual -eq $file.Value)}})
$hashes | ConvertTo-Json | Set-Content "$root\hashes.json" -Encoding UTF8
if($hashes.match -contains $false){throw 'Package hash mismatch'}
function RunCase($name,$exe,$arguments,$timeout){
$out="$root\$name.stdout.txt";$err="$root\$name.stderr.txt"
$watch=[Diagnostics.Stopwatch]::StartNew()
$p=Start-Process (Join-Path $pkg $exe) -ArgumentList $arguments -PassThru -WindowStyle Hidden -RedirectStandardOutput $out -RedirectStandardError $err
$handle=$p.Handle
$finished=$p.WaitForExit($timeout)
if(!$finished){$p.Kill();$p.WaitForExit()}
$p.Refresh()
[pscustomobject]@{name=$name;exe=$exe;finished=$finished;exit_code=$p.ExitCode;elapsed_seconds=$watch.Elapsed.TotalSeconds;arguments=$arguments}
}
$results=@(RunCase 'cpu' 'NexoraTwoDBenchmark.exe' "--atlas assets/atlas.nxcook --clip assets/clip.nxcook --frames 48 --report $root/cpu.csv" 300000)
$bad=[IO.File]::ReadAllBytes("$pkg\assets\atlas.nxcook");$bad[$bad.Length-1]=$bad[$bad.Length-1] -bxor 1;[IO.File]::WriteAllBytes("$root\corrupt.nxcook",$bad)
$results+=RunCase 'corrupt' 'NexoraTwoDBenchmark.exe' "--atlas $root/corrupt.nxcook --frames 4 --report $root/corrupt.csv" 60000
$results+=RunCase 'native' 'NexoraTwoDBenchmark.exe' "--native --atlas assets/atlas.nxcook --clip assets/clip.nxcook --frames 8 --report $root/native.csv" 180000
$results+=RunCase 'zig-native' 'NexoraZigTwoD.exe' "--native --frames 120 --report $root/zig-native.json" 180000
$results | ConvertTo-Json -Depth 4 | Set-Content "$root\results.json" -Encoding UTF8
$results | ConvertTo-Json -Depth 4
```

## guest-evidence/keyboard-task.json

```json
{
    "state":  "Ready",
    "last_result":  0,
    "report_present":  true,
    "human_result":  "User confirmed D/W movement then Escape"
}
```

## guest-evidence/zig-keyboard.json

```json
{"status":"PASS","language":"Zig 0.14","api":1,"native_frames":353,"movement_frames":48,"entity_x":0.716667,"sprite_instanced_draws":353,"sprite_gpu_instances":12002,"sprite_instance_upload_bytes":528088,"visible":34,"reloaded":false,"allocations_balanced":true,"physical_windows":"NOT RUN"}
```

## studio/manifest.json

```json
{
  "source_commit": "350c40eef0044c9e96607fb144a4369927b7b4f5",
  "configuration": "Monolithic Development /O2, assertions enabled",
  "files": {
    "NexoraTwoDStudio.exe": "1cab59e2a62c290d65386e809916750947b516c78dc159d768c3a3df3ef58739",
    "workspace/atlas.nx2atlas": "8a91683ff993d36adf3b158c8870d3b0be4d88ff7ab4eb2a819c56c0eb943a26",
    "workspace/clip.nx2clip": "00f73da7e88fd6986bf36be01d9af612627583a3de0a263059a976826a49a135"
  }
}
```

## guest-evidence/studio-hashes.json

```json
[
    {
        "file":  "NexoraTwoDStudio.exe",
        "expected":  "1cab59e2a62c290d65386e809916750947b516c78dc159d768c3a3df3ef58739",
        "actual":  "1cab59e2a62c290d65386e809916750947b516c78dc159d768c3a3df3ef58739",
        "match":  true
    },
    {
        "file":  "workspace/atlas.nx2atlas",
        "expected":  "8a91683ff993d36adf3b158c8870d3b0be4d88ff7ab4eb2a819c56c0eb943a26",
        "actual":  "8a91683ff993d36adf3b158c8870d3b0be4d88ff7ab4eb2a819c56c0eb943a26",
        "match":  true
    },
    {
        "file":  "workspace/clip.nx2clip",
        "expected":  "00f73da7e88fd6986bf36be01d9af612627583a3de0a263059a976826a49a135",
        "actual":  "00f73da7e88fd6986bf36be01d9af612627583a3de0a263059a976826a49a135",
        "match":  true
    }
]
```

## guest-evidence/studio-results.json

```json
[
    {
        "name":  "studio-smoke",
        "arguments":  "--workspace C:/NexoraM7Current20261015/studio/workspace --atlas atlas.nx2atlas --clip clip.nx2clip --document scene.nx2doc --smoke --frames 120",
        "finished":  true,
        "exit_code":  0,
        "elapsed_seconds":  19.1852569,
        "before_hash":  null,
        "after_hash":  "7C4FE0F1F47B7378978E4E781B8636B1573717899E024650399E5FA2F62EC860"
    },
    {
        "name":  "studio-reopen",
        "arguments":  "--workspace C:/NexoraM7Current20261015/studio/workspace --atlas atlas.nx2atlas --clip clip.nx2clip --document scene.nx2doc --read-only --frames 12",
        "finished":  true,
        "exit_code":  0,
        "elapsed_seconds":  7.2824854,
        "before_hash":  "7C4FE0F1F47B7378978E4E781B8636B1573717899E024650399E5FA2F62EC860",
        "after_hash":  "7C4FE0F1F47B7378978E4E781B8636B1573717899E024650399E5FA2F62EC860"
    }
]
```

## guest-evidence/studio-desktop-unresponsive.json

```json
{
    "Id":  3564,
    "SessionId":  1,
    "CPU":  12.4375,
    "WorkingSet64":  66060288,
    "Responding":  true,
    "MainWindowTitle":  "Nexora 2D Studio"
}
```

## guest-evidence/studio-desktop-retry.json

```json
{
    "state":  "Ready",
    "last_result":  0,
    "document_present":  true
}
```

## dependencies-studio.txt

```text
Microsoft (R) COFF/PE Dumper Version 14.44.35228.0
Copyright (C) Microsoft Corporation.  All rights reserved.


Dump of file build\m7-vm-2026-10-15\studio\NexoraTwoDStudio.exe

File Type: EXECUTABLE IMAGE

  Image has the following dependencies:

    d3d12.dll
    D3DCOMPILER_47.dll
    dxgi.dll
    IMM32.dll
    USER32.dll
    KERNEL32.dll
    SHELL32.dll
    MSVCP140.dll
    MSVCP140_2.dll
    VCRUNTIME140.dll
    VCRUNTIME140_1.dll
    api-ms-win-crt-runtime-l1-1-0.dll
    api-ms-win-crt-stdio-l1-1-0.dll
    api-ms-win-crt-filesystem-l1-1-0.dll
    api-ms-win-crt-heap-l1-1-0.dll
    api-ms-win-crt-string-l1-1-0.dll
    api-ms-win-crt-math-l1-1-0.dll
    api-ms-win-crt-environment-l1-1-0.dll
    api-ms-win-crt-utility-l1-1-0.dll
    api-ms-win-crt-convert-l1-1-0.dll
    api-ms-win-crt-locale-l1-1-0.dll

  Summary

        1000 .00cfg
       15000 .data
        7000 .idata
       23000 .pdata
       C3000 .rdata
        4000 .reloc
        1000 .rsrc
      36B000 .text
        1000 .tls
```

## guest-evidence/studio-final-result.json

```json
{
    "human_result":  "User confirmed all four tabs text/preview and Add sprite Save Open",
    "desktop_task_state":  "Running",
    "desktop_task_result":  267009,
    "baseline_hash":  "7C4FE0F1F47B7378978E4E781B8636B1573717899E024650399E5FA2F62EC860",
    "after_user_save_hash":  "45CD40AAD07C210D5F153E423511920453E6ECFA527FA0D9AD6EC58EEA42C13F",
    "readonly_arguments":  "--workspace C:/NexoraM7Current20261015/studio/workspace --atlas atlas.nx2atlas --clip clip.nx2clip --document scene.nx2doc --read-only --frames 12",
    "readonly_finished":  true,
    "readonly_exit":  0,
    "after_readonly_hash":  "45CD40AAD07C210D5F153E423511920453E6ECFA527FA0D9AD6EC58EEA42C13F",
    "readonly_unchanged":  true
}
```

## guest-evidence/studio-exit.json

```json
{
    "state":  "Ready",
    "last_result":  1,
    "studio_processes":  [

                         ],
    "human_confirmed_close":  true
}
```

## studio-fixed/manifest.json

```json
{
  "base_commit": "350c40eef0044c9e96607fb144a4369927b7b4f5",
  "configuration": "Monolithic Development /O2, assertions enabled",
  "change": "Check CloseRequested immediately after BeginFrame, before validating acquisition status",
  "studio_source_sha256": "0b1af2f054ba83ee1550c93859a1a451478e23b94eef2e2574bbb22290b3fe9e",
  "files": {
    "NexoraTwoDStudio.exe": "48fe056cd0967f3ccdea6f4e87a00461cfd6f49567afc0d0bd660a1cf9c82bfa",
    "workspace/atlas.nx2atlas": "8a91683ff993d36adf3b158c8870d3b0be4d88ff7ab4eb2a819c56c0eb943a26",
    "workspace/clip.nx2clip": "00f73da7e88fd6986bf36be01d9af612627583a3de0a263059a976826a49a135",
    "workspace/scene.nx2doc": "45cd40aad07c210d5f153e423511920453e6ecfa527fa0d9ad6ec58eea42c13f"
  }
}
```

## guest-evidence/fixed-studio-hashes.json

```json
[
    {
        "file":  "NexoraTwoDStudio.exe",
        "expected":  "48fe056cd0967f3ccdea6f4e87a00461cfd6f49567afc0d0bd660a1cf9c82bfa",
        "actual":  "48fe056cd0967f3ccdea6f4e87a00461cfd6f49567afc0d0bd660a1cf9c82bfa",
        "match":  true
    },
    {
        "file":  "workspace/atlas.nx2atlas",
        "expected":  "8a91683ff993d36adf3b158c8870d3b0be4d88ff7ab4eb2a819c56c0eb943a26",
        "actual":  "8a91683ff993d36adf3b158c8870d3b0be4d88ff7ab4eb2a819c56c0eb943a26",
        "match":  true
    },
    {
        "file":  "workspace/clip.nx2clip",
        "expected":  "00f73da7e88fd6986bf36be01d9af612627583a3de0a263059a976826a49a135",
        "actual":  "00f73da7e88fd6986bf36be01d9af612627583a3de0a263059a976826a49a135",
        "match":  true
    },
    {
        "file":  "workspace/scene.nx2doc",
        "expected":  "45cd40aad07c210d5f153e423511920453e6ecfa527fa0d9ad6ec58eea42c13f",
        "actual":  "45cd40aad07c210d5f153e423511920453e6ecfa527fa0d9ad6ec58eea42c13f",
        "match":  true
    }
]
```

## guest-evidence/fixed-studio-smoke.json

```json
{
    "finished":  true,
    "exit_code":  0,
    "arguments":  "--workspace C:/NexoraM7Current20261015/studio-fixed/workspace --atlas atlas.nx2atlas --clip clip.nx2clip --document scene.nx2doc --read-only --frames 12",
    "document_hash":  "45CD40AAD07C210D5F153E423511920453E6ECFA527FA0D9AD6EC58EEA42C13F"
}
```

## guest-evidence/fixed-studio-exit.json

```json
{
    "state":  "Ready",
    "last_result":  0,
    "studio_processes":  [

                         ],
    "human_confirmed_close":  true,
    "document_hash":  "45CD40AAD07C210D5F153E423511920453E6ECFA527FA0D9AD6EC58EEA42C13F"
}
```

## guest-evidence/remaining-acceptance-tasks.json

```json
[]
```
