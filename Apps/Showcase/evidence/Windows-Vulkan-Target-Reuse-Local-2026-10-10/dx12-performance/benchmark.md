# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `5f59f93446af`; backend: dx12; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "dxgi.umd:9007199255732212".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 95.877 | 10.430 | 12.069 | 12.658 | 8.490 | 108666880 |
| basic | 2 | 104.114 | 9.605 | 10.222 | 10.744 | 7.917 | 108552192 |
| basic | 3 | 104.615 | 9.559 | 10.126 | 10.792 | 7.500 | 107429888 |
| standard | 1 | 60.564 | 16.511 | 17.291 | 17.959 | 8.229 | 108134400 |
| standard | 2 | 60.068 | 16.648 | 17.300 | 19.151 | 9.115 | 108142592 |
| standard | 3 | 59.872 | 16.702 | 17.694 | 19.963 | 8.906 | 108724224 |
| high | 1 | 59.445 | 16.822 | 18.651 | 19.940 | 9.948 | 107450368 |
| high | 2 | 59.174 | 16.899 | 17.847 | 18.351 | 9.479 | 108613632 |
| high | 3 | 59.712 | 16.747 | 18.516 | 21.149 | 7.865 | 108449792 |
