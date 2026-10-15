# Evidencia — audio del banco (2026-10-15, corrida `factory/banco-audio`)

Todo lo de acá se genera con `tools/captura-banco.sh` en modo `CAP_AUDIO` (ver
[`docs/banco-audio.md`](../../banco-audio.md)). Base `02c1311`, binario de la GUI
`md5 67b41837e0f4`, QEMU `cortex-a7`, chroot `../.vfix3`.

| Archivo | Qué es |
|---|---|
| `captura-voz.txt` | salida real de `CAP_AUDIO=captura-voz.wav` con la prueba de voz: sink 0.00 dB, 205.778 muestras/canal, **RMS −25,23 dBFS, pico −3,70 dBFS**, 67.974 muestras ≠ 0, y las dos líneas del mixer del GUI |
| `captura-voz.wav` | la captura de esa corrida (monitor del sink, estéreo 44100; los dos canales idénticos) |
| `control-sin-voz.txt` | la misma corrida con `CAP_VOZ=`: **440.094 muestras, todas 0** (silencio digital) |
| `parte-b-pendiente.txt` | `CAP_LOOPBACK=si` sin `/dev/snd`: lo que imprime el guard (runbook) y su código 2 |
| `alsa-evidence.txt` | comandos y salida de ALSA (libasound del host): el plugin `file` es transparente, el mixer `default` no existe en el banco, `softvol` necesita una tarjeta, y el plugin `file` sin reloj escribe a ~500 MB/s |

Repeticiones (mismo comando, sólo para ver la dispersión): RMS −26,33 dBFS / −25,22 / −25,23 con
pico −3,70 dBFS las tres, y 53.503 / 67.974 / 67.974 muestras ≠ 0. El detalle y lo que falta explicar
está en `docs/banco-audio.md` § "Evidencia medida".

Para repetirlo (≈10 s):

```bash
CHROOT=../.vfix3 RECONSTRUIR=no CAP_AUDIO=/tmp/audio.wav tools/captura-banco.sh
```
