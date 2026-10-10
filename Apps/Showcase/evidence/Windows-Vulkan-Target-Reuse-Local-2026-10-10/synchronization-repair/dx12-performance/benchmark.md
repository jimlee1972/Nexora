# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `5aa60d5cc8a0`; backend: dx12; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "dxgi.umd:9007199255732212".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 84.246 | 11.870 | 13.494 | 16.928 | 8.750 | 106225664 |
| basic | 2 | 85.625 | 11.679 | 13.822 | 15.818 | 8.958 | 106840064 |
| basic | 3 | 76.437 | 13.083 | 17.286 | 21.849 | 10 | 107024384 |
| standard | 1 | 55.160 | 18.129 | 20.680 | 22.373 | 9.896 | 106344448 |
| standard | 2 | 54.293 | 18.419 | 21.784 | 23.700 | 11.094 | 107069440 |
| standard | 3 | 51.715 | 19.337 | 24.284 | 27.756 | 11.094 | 106786816 |
| high | 1 | 50.843 | 19.668 | 22.714 | 29.271 | 11.771 | 107216896 |
| high | 2 | 54.552 | 18.331 | 20.430 | 23.322 | 10.781 | 106561536 |
| high | 3 | 46.354 | 21.573 | 24.641 | 53.808 | 12.760 | 107331584 |
