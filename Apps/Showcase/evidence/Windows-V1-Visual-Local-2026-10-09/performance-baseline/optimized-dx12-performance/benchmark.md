# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `e3eb383b4df7`; backend: dx12; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "dxgi.umd:9007199255732212".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 39.960 | 25.025 | 32.066 | 37.024 | 20.625 | 108003328 |
| basic | 2 | 48.860 | 20.467 | 23.488 | 25.363 | 17.500 | 107597824 |
| basic | 3 | 52.023 | 19.222 | 22.154 | 23.939 | 16.510 | 107442176 |
| standard | 1 | 36.930 | 27.078 | 29.425 | 31.914 | 17.865 | 107843584 |
| standard | 2 | 36.066 | 27.727 | 30.891 | 34.931 | 18.073 | 107892736 |
| standard | 3 | 34.548 | 28.946 | 32.082 | 35.217 | 19.375 | 108670976 |
| high | 1 | 35.880 | 27.871 | 31.142 | 33.992 | 18.646 | 107749376 |
| high | 2 | 35.709 | 28.004 | 31.054 | 34.680 | 19.010 | 108285952 |
| high | 3 | 37.707 | 26.520 | 28.848 | 31.567 | 18.229 | 108392448 |
