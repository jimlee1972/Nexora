# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `e3eb383b4df7`; backend: dx12; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "dxgi.umd:9007199255732212".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 91.548 | 10.923 | 12.432 | 13.394 | 8.698 | 107749376 |
| basic | 2 | 65.940 | 15.165 | 21.838 | 40.743 | 9.844 | 108064768 |
| basic | 3 | 81.749 | 12.233 | 15.467 | 38.132 | 8.906 | 107466752 |
| standard | 1 | 52.878 | 18.912 | 22.710 | 28.174 | 11.198 | 108756992 |
| standard | 2 | 56.932 | 17.565 | 19.606 | 21.156 | 9.427 | 108425216 |
| standard | 3 | 52.522 | 19.039 | 21.797 | 24.064 | 11.927 | 108281856 |
| high | 1 | 54.912 | 18.211 | 21.010 | 25.364 | 11.250 | 108003328 |
| high | 2 | 50.833 | 19.672 | 22.003 | 24.202 | 10.938 | 108466176 |
| high | 3 | 18.559 | 53.882 | 199.014 | 417.800 | 12.240 | 107495424 |
