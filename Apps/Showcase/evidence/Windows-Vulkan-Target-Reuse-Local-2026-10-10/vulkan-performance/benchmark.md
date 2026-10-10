# Native courtyard performance

Sequential native fixed-scene measurements; hardware/visual acceptance remains open.

Build: `5f59f93446af`; backend: vulkan; software rasterizer: False.

Device: "NVIDIA GeForce GTX 960"; driver: "vulkan.raw:2438201344".

Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.
Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.
These observations do not accept the hardware budget or physical display.

| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| basic | 1 | 173.386 | 5.767 | 6.265 | 6.933 | 5.729 | 90935296 |
| basic | 2 | 169.783 | 5.890 | 6.560 | 6.973 | 5.885 | 91004928 |
| basic | 3 | 173.998 | 5.747 | 6.211 | 6.533 | 5.781 | 90300416 |
| standard | 1 | 126.192 | 7.924 | 8.618 | 9.504 | 6.302 | 98799616 |
| standard | 2 | 126.149 | 7.927 | 8.632 | 8.955 | 5.625 | 99471360 |
| standard | 3 | 125.988 | 7.937 | 8.775 | 10.175 | 5.990 | 99000320 |
| high | 1 | 123.907 | 8.071 | 9.039 | 9.637 | 6.771 | 112480256 |
| high | 2 | 123.301 | 8.110 | 9.153 | 9.679 | 7.031 | 90972160 |
| high | 3 | 123.921 | 8.070 | 8.970 | 9.832 | 6.094 | 112558080 |
