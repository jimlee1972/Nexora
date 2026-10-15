# Retained Windows VirtualBox observations

These are unchanged text payloads exported from the guest during the 2026-10-15 run.
The package was built previously; these are not results for current main.

## hashes-runtime-installed.json

```json
[
    {
        "file":  "NexoraTwoDBenchmark.exe",
        "expected":  "7bde35870dda93c7ddfacefb38ecc3827aa60c4b98433e83654bf5ec4da941ba",
        "actual":  "7bde35870dda93c7ddfacefb38ecc3827aa60c4b98433e83654bf5ec4da941ba",
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
    }
]
```

## results-baseline.json

```json
{
    "name":  "cpu",
    "finished":  true,
    "exit_code":  -1073741515,
    "elapsed_seconds":  3.0977527,
    "arguments":  "--atlas assets/atlas.nxcook --clip assets/clip.nxcook --frames 8 --report C:\\NexoraAcceptance20261015/baseline-cpu.csv"
}
```

## results-runtime-installed.json

```json
[
    {
        "name":  "cpu",
        "finished":  true,
        "exit_code":  0,
        "elapsed_seconds":  8.6542085,
        "arguments":  "--atlas assets/atlas.nxcook --clip assets/clip.nxcook --frames 8 --report C:\\NexoraAcceptance20261015/runtime-installed-cpu.csv"
    },
    {
        "name":  "corrupt",
        "finished":  true,
        "exit_code":  1,
        "elapsed_seconds":  1.2717810999999999,
        "arguments":  "--atlas C:\\NexoraAcceptance20261015/corrupt.nxcook --frames 4 --report C:\\NexoraAcceptance20261015/runtime-installed-corrupt.csv"
    },
    {
        "name":  "native",
        "finished":  true,
        "exit_code":  1,
        "elapsed_seconds":  9.1796662,
        "arguments":  "--native --atlas assets/atlas.nxcook --clip assets/clip.nxcook --frames 8 --report C:\\NexoraAcceptance20261015/runtime-installed-native.csv"
    }
]
```

## results-three-d-on.json

```json
[
    {
        "name":  "cpu",
        "finished":  true,
        "exit_code":  0,
        "elapsed_seconds":  5.7055267,
        "arguments":  "--atlas assets/atlas.nxcook --clip assets/clip.nxcook --frames 8 --report C:\\NexoraAcceptance20261015/three-d-on-cpu.csv"
    },
    {
        "name":  "corrupt",
        "finished":  true,
        "exit_code":  1,
        "elapsed_seconds":  0.625417,
        "arguments":  "--atlas C:\\NexoraAcceptance20261015/corrupt.nxcook --frames 4 --report C:\\NexoraAcceptance20261015/three-d-on-corrupt.csv"
    },
    {
        "name":  "native",
        "finished":  true,
        "exit_code":  1,
        "elapsed_seconds":  1.4201591999999998,
        "arguments":  "--native --atlas assets/atlas.nxcook --clip assets/clip.nxcook --frames 8 --report C:\\NexoraAcceptance20261015/three-d-on-native.csv"
    }
]
```

## adapters-3d-on.txt

```text
adapter=0 name=VirtualBox Graphics Adapter (WDDM) vendor=80EE flags=0
adapter=0 D3D12CreateDevice FL11_0 HRESULT=0x887A0004
adapter=1 name=Microsoft Basic Render Driver vendor=1414 flags=2
adapter=1 D3D12CreateDevice FL11_0 HRESULT=0x00000000
```

## runtime-installed-native.stderr.txt

```text
stress draw
```

## three-d-on-native.stderr.txt

```text
stress acquire
```

## runtime-installed-corrupt.stderr.txt

```text
invalid cooked asset
```

## three-d-on-corrupt.stderr.txt

```text
invalid cooked asset
```

## runtime-installed-cpu.csv

```csv
objects,sparse,reload_ms,overlap,native,frames,warmup_frames,loading_ms,cpu_p50_ms,cpu_p95_ms,cpu_p99_ms,cpu_max_ms,frame_p95_ms,gpu_samples,gpu_p95_ms,peak_process_rss_bytes,visible,culled,sprite_batches,covered_aabb_ratio,query_hits,frame_average_ms,frame_p99_ms,frame_max_ms,device_name,vendor_id,device_id,driver_version,driver_version_format
100,0,0.0669,0,0,8,4,3.7035,0.2831,13.0366,13.0366,13.0366,13.0375,0,,0,100,0,1,0.00146484,0,1.91859,13.0375,13.0375,"",0,0,0,0
1000,0,0.7694,0,0,8,4,2.4596,8.5828,23.9897,23.9897,23.9897,23.9923,0,,0,1000,0,1,0.027832,0,11.53,23.9923,23.9923,"",0,0,0,0
5000,0,55.8241,0,0,8,4,7.6951,73.1727,144.454,144.454,144.454,144.46,0,,0,5000,0,1,0.14502,1584,86.9493,144.46,144.46,"",0,0,0,0
10000,0,19.7872,0,0,8,4,316.28,68.9349,179.811,179.811,179.811,179.812,0,,0,9700,300,1,0.28125,2556,92.858,179.812,179.812,"",0,0,0,0
1000,0,0.8446,1,0,8,4,3.7848,8.2869,30.1059,30.1059,30.1059,30.9541,0,,0,1000,0,1,5.20833,12000,11.6358,30.9541,30.9541,"",0,0,0,0
10000,1,59.5678,0,0,8,4,49.0738,185.774,412.128,412.128,412.128,412.132,0,,0,195,9805,1,0.00509672,60,230.108,412.132,412.132,"",0,0,0,0
```

## three-d-on-cpu.csv

```csv
objects,sparse,reload_ms,overlap,native,frames,warmup_frames,loading_ms,cpu_p50_ms,cpu_p95_ms,cpu_p99_ms,cpu_max_ms,frame_p95_ms,gpu_samples,gpu_p95_ms,peak_process_rss_bytes,visible,culled,sprite_batches,covered_aabb_ratio,query_hits,frame_average_ms,frame_p99_ms,frame_max_ms,device_name,vendor_id,device_id,driver_version,driver_version_format
100,0,0.0725,0,0,8,4,0.1009,0.2337,0.3474,0.3474,0.3474,0.3475,0,,0,100,0,1,0.00146484,0,0.252263,0.3475,0.3475,"",0,0,0,0
1000,0,10.912,0,0,8,4,4.5948,6.6027,16.9724,16.9724,16.9724,16.9746,0,,0,1000,0,1,0.027832,0,10.1998,16.9746,16.9746,"",0,0,0,0
5000,0,14.064,0,0,8,4,34.792,47.9858,80.7424,80.7424,80.7424,80.7465,0,,0,5000,0,1,0.14502,1584,52.0153,80.7465,80.7465,"",0,0,0,0
10000,0,51.6092,0,0,8,4,10.4808,96.8032,159.442,159.442,159.442,189.416,0,,0,9700,300,1,0.28125,2556,108.356,189.416,189.416,"",0,0,0,0
1000,0,0.6443,1,0,8,4,0.7344,14.0512,42.1472,42.1472,42.1472,42.1489,0,,0,1000,0,1,5.20833,12000,18.3016,42.1489,42.1489,"",0,0,0,0
10000,1,19.6947,0,0,8,4,18.0976,92.9277,383.423,383.423,383.423,383.426,0,,0,195,9805,1,0.00509672,60,139.535,383.426,383.426,"",0,0,0,0
```

## Initial 3D-off inventory.json

```json
{
    "os":  [
               {
                   "Caption":  "Microsoft Windows 10 家用版",
                   "Version":  "10.0.19045",
                   "OSArchitecture":  "64 位元"
               }
           ],
    "gpu":  [
                {
                    "Name":  "VirtualBox Graphics Adapter (WDDM)",
                    "DriverVersion":  "7.2.20.25154",
                    "AdapterCompatibility":  "Oracle Corporation"
                }
            ],
    "dev_tools":  [

                  ],
    "runtime_dlls":  [
                         {
                             "name":  "vcruntime140.dll",
                             "present":  false
                         },
                         {
                             "name":  "vcruntime140_1.dll",
                             "present":  false
                         },
                         {
                             "name":  "msvcp140.dll",
                             "present":  false
                         },
                         {
                             "name":  "d3d12.dll",
                             "present":  true
                         },
                         {
                             "name":  "vulkan-1.dll",
                             "present":  false
                         }
                     ],
    "vulkan_icds":  [

                    ]
}
```

## Initial 3D-off adapters.txt

```text
adapter=0 name=Microsoft Basic Render Driver vendor=1414 flags=0
adapter=0 D3D12CreateDevice FL11_0 HRESULT=0x00000000
adapter=1 name=Microsoft Basic Render Driver vendor=1414 flags=2
adapter=1 D3D12CreateDevice FL11_0 HRESULT=0x00000000
```

## Initial 3D-off graphics-probe.txt

```text
Default adapter D3D12CreateDevice FL11_0 HRESULT=0x00000000
System.DllNotFoundException: Unable to load DLL 'vulkan-1.dll': 找不到指定的模組。 (Exception from HRESULT: 0x8007007E)
   於 GP.vkCreateInstance(Info& i, IntPtr a, IntPtr& v)
   於 GP.Vk()
```

## vcredist-result.json

```json
{
    "exit_code":  0,
    "version":  "14.51.36247.0"
}
```
