# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `e3eb383b4df7`; backend: vulkan; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "vulkan.raw:2438201344".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 35.796 | 27.936 | 32.572 | 33.608 | 22.188 | 90558464 |
| basic | 2 | 33.674 | 29.697 | 35.980 | 39.461 | 21.719 | 90423296 |
| basic | 3 | 32.662 | 30.616 | 36.069 | 37.955 | 21.458 | 90460160 |
| standard | 1 | 24.950 | 40.080 | 51.332 | 59.649 | 28.229 | 91090944 |
| standard | 2 | 26.607 | 37.585 | 45.748 | 49.559 | 28.906 | 91123712 |
| standard | 3 | 26.567 | 37.640 | 47.112 | 49.112 | 28.385 | 90734592 |
| high | 1 | 23.042 | 43.398 | 57.275 | 63.312 | 31.875 | 90566656 |
| high | 2 | 25.687 | 38.930 | 46.516 | 50.373 | 29.896 | 90742784 |
| high | 3 | 26.629 | 37.553 | 44.657 | 49.323 | 29.688 | 90693632 |
