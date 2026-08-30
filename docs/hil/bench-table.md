# HIL benchmark

_Debian GNU/Linux 13 (trixie) · kernel 6.18.34+rpt-rpi-v8 · aarch64 · BlueZ 5.82 · Python 3.13.5, load 0.2_

| Board | Profile | Report B | Descr B | Conn ms | MTU | btn e2e p50/p99 ms | axis p50 ms | clean Hz | dropped |
|---|---|---|---|---|---|---|---|---|---|
| esp32dev | minimal | 3 | 50 | 48.75 | 255 | 18.6/67.825 | 18.574 | 128.4 | 0 |
| esp32dev | reports | 6 | 82 | 48.75 | 255 | 18.615/67.496 | 18.591 | 81.2 | 0 |
| esp32dev | maxbtn | 16 | 25 | 48.75 | 255 | 18.59/67.443 | - | - | 0 |
| esp32dev | specials | 20 | 132 | 48.75 | 255 | 18.601/67.518 | 18.586 | 121.9 | 0 |
| esp32dev | default | 28 | 102 | 48.75 | 255 | 18.611/67.986 | 18.591 | 82.7 | 0 |
| esp32dev | signed-axes | 28 | 102 | 48.75 | 255 | 18.607/67.813 | 18.591 | 109.6 | 0 |
