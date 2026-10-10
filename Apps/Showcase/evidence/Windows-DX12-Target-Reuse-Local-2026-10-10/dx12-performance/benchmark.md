# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `7c4534b850c3`; backend: dx12; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "dxgi.umd:9007199255732212".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 157.788 | 6.338 | 7.636 | 8.356 | 6.250 | 110972928 |
| basic | 2 | 158.258 | 6.319 | 6.868 | 7.440 | 6.250 | 109719552 |
| basic | 3 | 137.565 | 7.269 | 8.092 | 9.321 | 7.240 | 110333952 |
| standard | 1 | 134.911 | 7.412 | 8.369 | 8.782 | 6.510 | 110039040 |
| standard | 2 | 131.521 | 7.603 | 8.729 | 9.412 | 7.344 | 110276608 |
| standard | 3 | 133.587 | 7.486 | 8.454 | 8.821 | 6.875 | 110313472 |
| high | 1 | 131.884 | 7.582 | 8.472 | 9.308 | 6.771 | 110379008 |
| high | 2 | 131.898 | 7.582 | 8.621 | 9.319 | 6.719 | 109838336 |
| high | 3 | 133.073 | 7.515 | 8.576 | 8.934 | 7.031 | 110006272 |
