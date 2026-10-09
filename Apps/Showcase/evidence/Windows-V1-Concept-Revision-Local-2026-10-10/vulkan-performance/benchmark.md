# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `bb0c96f0c539`; backend: vulkan; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "vulkan.raw:2438201344".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 61.531 | 16.252 | 20.643 | 21.947 | 11.042 | 90939392 |
| basic | 2 | 61.780 | 16.186 | 21.554 | 24.628 | 11.406 | 90898432 |
| basic | 3 | 54.164 | 18.462 | 24.032 | 35.317 | 11.406 | 90607616 |
| standard | 1 | 40.115 | 24.929 | 28.016 | 35.146 | 16.615 | 90984448 |
| standard | 2 | 37.754 | 26.487 | 31.806 | 37.181 | 17.135 | 90984448 |
| standard | 3 | 37.513 | 26.657 | 33.857 | 37.660 | 17.344 | 90886144 |
| high | 1 | 38.410 | 26.035 | 29.468 | 34.663 | 17.604 | 91303936 |
| high | 2 | 39.201 | 25.509 | 28.448 | 35.007 | 16.875 | 91131904 |
| high | 3 | 37.174 | 26.901 | 30.930 | 35.761 | 18.073 | 90427392 |
