# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `7c4534b850c3`; backend: vulkan; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "vulkan.raw:2438201344".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 148.557 | 6.731 | 7.444 | 8.158 | 6.667 | 93560832 |
| basic | 2 | 146.317 | 6.834 | 7.904 | 9.062 | 6.719 | 92766208 |
| basic | 3 | 135.654 | 7.372 | 8.771 | 11.544 | 7.135 | 93118464 |
| standard | 1 | 125.745 | 7.953 | 8.839 | 10.036 | 7.188 | 93265920 |
| standard | 2 | 125.868 | 7.945 | 8.868 | 9.937 | 7.604 | 93192192 |
| standard | 3 | 122.854 | 8.140 | 9.373 | 12.483 | 7.552 | 93728768 |
| high | 1 | 122.385 | 8.171 | 9.499 | 10.229 | 7.865 | 93237248 |
| high | 2 | 108.732 | 9.197 | 11.948 | 29.421 | 7.604 | 93724672 |
| high | 3 | 109.376 | 9.143 | 13.077 | 16.486 | 8.542 | 92405760 |
