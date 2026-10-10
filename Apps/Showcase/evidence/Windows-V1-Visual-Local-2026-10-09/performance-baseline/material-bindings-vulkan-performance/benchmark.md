# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `ab7b4f32c14e`; backend: vulkan; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "vulkan.raw:2438201344".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 67.269 | 14.866 | 15.844 | 18.657 | 11.719 | 90042368 |
| basic | 2 | 65.501 | 15.267 | 18.697 | 21.352 | 11.458 | 90460160 |
| basic | 3 | 65.279 | 15.319 | 17.563 | 20.454 | 11.458 | 91230208 |
| standard | 1 | 39.522 | 25.303 | 28.127 | 35.148 | 16.354 | 90628096 |
| standard | 2 | 35.290 | 28.337 | 36.129 | 38.965 | 18.542 | 90910720 |
| standard | 3 | 15.670 | 63.814 | 209.871 | 419.935 | 21.406 | 90406912 |
| high | 1 | 39.775 | 25.141 | 33.483 | 37.372 | 16.250 | 90607616 |
| high | 2 | 39.881 | 25.074 | 32.823 | 39.043 | 15.990 | 91262976 |
| high | 3 | 38.690 | 25.846 | 33.135 | 40.023 | 17.083 | 90845184 |
