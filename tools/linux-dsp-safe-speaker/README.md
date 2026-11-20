# QA-LINUX-DSP-ROUTE — modelo de referencia offline

`route_arbiter_model.[ch]`: modelo en C determinista de un **árbitro único de la ruta RX Linux**
(sin I/O real, sin radio). Representa el contrato de seguridad propuesto en
`docs/evidencia/2026-11-04-linux-dsp-safe-speaker/`:

- BASE inicial, sin auto-enable; verdad de árbitro (sombra autoritativa) en lugar de readback del MCU;
- enable sólo con heartbeat/lease fresco, `write_ok`, PCM post-Speex, sin TX/ATU/generado;
- TX/PTT/ATU deshabilitan RX Linux **antes** de tocar bits de TX; bus fail ⇒ FAULT;
- restore independiente de la GUI (lease/exit/sink/PCM) con presupuesto 250 ms;
- PCM 44.1 kHz mono S16, bloque 4410 post-Speex una vez, duplicado rechazado, ring 4 acotado.

Tests en `tests/linux-dsp-safe-speaker/`: 156 comprobaciones, 0 fallos; compila ARM EABI5 con el
toolchain de la ROM. **No es código de producción**: no se integra a `src/`, no abre I2C/ALSA, no se
despliega. La integración real (árbitro/proxy + supervisor) queda para #364.
