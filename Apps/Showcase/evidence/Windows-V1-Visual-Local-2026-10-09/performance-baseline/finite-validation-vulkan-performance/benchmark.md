# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `e3eb383b4df7`; backend: vulkan; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "vulkan.raw:2438201344".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 53.236 | 18.784 | 24.500 | 26.907 | 12.969 | 91111424 |
| basic | 2 | 57.595 | 17.363 | 20.805 | 41.703 | 12.135 | 90935296 |
| basic | 3 | 57.194 | 17.484 | 24.133 | 26.434 | 12.917 | 90836992 |
| standard | 1 | 32.752 | 30.532 | 40.990 | 45.999 | 19.896 | 90230784 |
| standard | 2 | 14.248 | 70.187 | 239.863 | 538.111 | 24.688 | 90927104 |
| standard | 3 | 13.321 | 75.072 | 226.460 | 485.563 | 25.260 | 90890240 |
| high | 1 | 33.800 | 29.585 | 36.135 | 40.174 | 20.208 | 90578944 |
| high | 2 | 32.687 | 30.594 | 39.798 | 43.953 | 20.417 | 91467776 |
| high | 3 | 33.446 | 29.899 | 40.817 | 64.986 | 19.635 | 91385856 |
