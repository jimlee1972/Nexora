# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `e3eb383b4df7`; backend: vulkan; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "vulkan.raw:2438201344".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 7.479 | 133.713 | 279.855 | 482.802 | 74.688 | 91336704 |
| basic | 2 | 8.177 | 122.290 | 286.840 | 468.073 | 70.312 | 90755072 |
| basic | 3 | 6.541 | 152.892 | 364.642 | 539.064 | 77.604 | 91353088 |
| standard | 1 | 8.255 | 121.146 | 240.882 | 719.672 | 77.240 | 91160576 |
| standard | 2 | 14.184 | 70.502 | 81.185 | 89.451 | 61.562 | 91987968 |
| standard | 3 | 8.679 | 115.223 | 286.566 | 486.287 | 70.781 | 91058176 |
| high | 1 | 9.295 | 107.582 | 246.357 | 482.859 | 74.531 | 91066368 |
| high | 2 | 13.330 | 75.021 | 90.991 | 102.780 | 62.708 | 92168192 |
| high | 3 | 13.287 | 75.261 | 91.067 | 97.522 | 61.250 | 91967488 |
