# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `5aa60d5cc8a0`; backend: vulkan; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "vulkan.raw:2438201344".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 130.866 | 7.641 | 11.053 | 12.645 | 7.396 | 89247744 |
| basic | 2 | 140.134 | 7.136 | 9.077 | 9.809 | 6.875 | 89055232 |
| basic | 3 | 142.413 | 7.022 | 8.507 | 9.542 | 6.771 | 89092096 |
| standard | 1 | 122.252 | 8.180 | 9.964 | 11.078 | 7.500 | 89051136 |
| standard | 2 | 119.918 | 8.339 | 10.184 | 11.613 | 6.875 | 89350144 |
| standard | 3 | 116.483 | 8.585 | 11.280 | 14.381 | 7.656 | 89600000 |
| high | 1 | 118.525 | 8.437 | 9.882 | 12.714 | 7.552 | 89722880 |
| high | 2 | 119.073 | 8.398 | 10.358 | 12.262 | 7.604 | 89493504 |
| high | 3 | 117.943 | 8.479 | 10.738 | 13.084 | 7.656 | 89034752 |
