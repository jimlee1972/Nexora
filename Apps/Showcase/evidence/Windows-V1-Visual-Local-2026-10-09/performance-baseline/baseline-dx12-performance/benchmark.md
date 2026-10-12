# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `e3eb383b4df7`; backend: dx12; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "dxgi.umd:9007199255732212".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 14.320 | 69.832 | 121.088 | 305.351 | 52.604 | 109092864 |
| basic | 2 | 20.952 | 47.727 | 53.989 | 62.466 | 44.531 | 109047808 |
| basic | 3 | 11.985 | 83.435 | 163.468 | 378.376 | 59.531 | 108773376 |
| standard | 1 | 10.897 | 91.768 | 179.399 | 373.307 | 63.490 | 109629440 |
| standard | 2 | 15.760 | 63.450 | 77.126 | 85.769 | 51.562 | 109506560 |
| standard | 3 | 10.701 | 93.447 | 204.153 | 322.834 | 59.792 | 108945408 |
| high | 1 | 16.645 | 60.079 | 69.714 | 82.361 | 50.990 | 108683264 |
| high | 2 | 16.748 | 59.710 | 69.584 | 72.821 | 50.885 | 108163072 |
| high | 3 | 12.233 | 81.744 | 132.341 | 189.378 | 61.719 | 109346816 |
