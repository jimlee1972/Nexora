# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `ab7b4f32c14e`; backend: dx12; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "dxgi.umd:9007199255732212".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 105.185 | 9.507 | 10.250 | 10.966 | 7.344 | 108122112 |
| basic | 2 | 96.169 | 10.398 | 12.554 | 14.956 | 7.604 | 107683840 |
| basic | 3 | 85.874 | 11.645 | 13.642 | 19.785 | 8.542 | 107958272 |
| standard | 1 | 36.045 | 27.743 | 70.136 | 149.833 | 11.615 | 107290624 |
| standard | 2 | 62.575 | 15.981 | 16.970 | 17.468 | 8.229 | 108113920 |
| standard | 3 | 62.455 | 16.011 | 16.934 | 17.304 | 7.500 | 108163072 |
| high | 1 | 60.308 | 16.581 | 19.464 | 20.346 | 8.750 | 107474944 |
| high | 2 | 58.959 | 16.961 | 18.841 | 26.669 | 9.010 | 108208128 |
| high | 3 | 59.977 | 16.673 | 17.332 | 17.813 | 8.750 | 108654592 |
