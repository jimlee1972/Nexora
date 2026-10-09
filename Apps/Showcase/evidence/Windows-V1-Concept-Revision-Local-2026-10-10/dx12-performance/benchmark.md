# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `bb0c96f0c539`; backend: dx12; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "dxgi.umd:9007199255732212".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 102.549 | 9.751 | 10.604 | 11.418 | 7.760 | 107868160 |
| basic | 2 | 102.186 | 9.786 | 10.593 | 12.791 | 7.500 | 107495424 |
| basic | 3 | 100.935 | 9.907 | 11.180 | 12.803 | 7.969 | 107515904 |
| standard | 1 | 60.651 | 16.488 | 17.539 | 18.894 | 8.125 | 107798528 |
| standard | 2 | 60.727 | 16.467 | 17.820 | 19.038 | 8.906 | 108257280 |
| standard | 3 | 60.202 | 16.611 | 17.850 | 20.475 | 6.979 | 107315200 |
| high | 1 | 60.425 | 16.549 | 17.796 | 19.141 | 10.104 | 108363776 |
| high | 2 | 60.014 | 16.663 | 19.015 | 21.363 | 7.865 | 108167168 |
| high | 3 | 60.490 | 16.532 | 18.081 | 19.314 | 8.958 | 107872256 |
