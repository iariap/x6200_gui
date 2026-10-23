| Estado | Reps | CPU hilo GUI | CPU otros hilos | ctxt/s | RSS (kB) | PSS (kB) | fds | threads | RTT p50/p95/p99 (µs) | log (B/min) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|
| CW decoder sin señal | 3 | 9.033 % (7.29–9.17) | 99.479 % | 207.41 | 58020.0 | 57692.0 | 26 | 12 | 333/441/549 (n=60) | 0.0 |
| DFLEditor abierto | 3 | 1.675 % (1.29–1.78) | 99.501 % | 212.72 | 58020.0 | 57692.0 | 26 | 12 | 335/470/570 (n=60) | 0.0 |
| home RX (sin cliente) | 3 | 1.675 % (1.25–1.83) | 99.498 % | 220.26 | 58020.0 | 57693.0 | 26 | 12 | — | 0.0 |
| home RX + get 1 Hz | 3 | 1.691 % (1.22–1.78) | 99.474 % | 203.17 | 58020.0 | 57692.0 | 26 | 12 | 320/493/631 (n=60) | 0.0 |
| home RX + get 5 Hz | 3 | 1.303 % (1.22–1.66) | 99.797 % | 205.14 | 58020.0 | 57696.0 | 25 | 11 | 304/413/472 (n=300) | 0.0 |
| Propagation cancelado | 3 | 1.37 % (1.30–1.73) | 99.646 % | 209.17 | 58020.0 | 57696.0 | 26 | 11 | 333/475/605 (n=60) | 0.0 |
| Propagation abierto | 3 | 1.641 % (1.18–1.69) | 99.55 % | 201.29 | 58020.0 | 57692.0 | 25 | 11 | 328/449/551 (n=60) | 0.0 |
| Propagation barrido | 3 | 2.859 % (2.20–2.93) | 99.559 % | 189.53 | 58020.0 | 57696.0 | 26 | 12 | 340/456/679 (n=60) | 220.2 |
| Recorder inactivo | 3 | 1.759 % (1.29–1.84) | 99.574 % | 222.32 | 58020.0 | 57692.0 | 26 | 12 | 322/466/530 (n=60) | 0.0 |
| Settings abierto | 3 | 1.708 % (1.22–1.73) | 99.489 % | 200.45 | 58020.0 | 57692.0 | 26 | 12 | 330/486/552 (n=60) | 0.0 |

Reps = ventanas de 60 s; CPU% = porcentaje de UN core del host (QEMU_USER), no Cortex-A7. 
`otros hilos` incluye el lazo degradado del radio_thread sin BASE (~100 %); no es reposo del equipo.
RTT = socket `get` observable; p99 sólo si n≥60.
