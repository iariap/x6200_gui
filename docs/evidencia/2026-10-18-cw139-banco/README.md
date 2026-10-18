# Evidencia #139 — botón `Auto` del decodificador CW en el banco QEMU

**Fecha**: 2026-10-18. **Rama**: `factory/cw-autotune-139` (base `7222abae`, trunk tip).
**HEAD de código**: `e989c040` — *CW (#139): auto-ajuste de los parametros del decodificador (AUTO opt-in)*.
**Binario ARM medido**: `md5 ccaa60d9431590bf06d1349369a9e006` (13.778.528 B, ELF32 ARM, gate #86 OK).
**Fuente**: `md5 e4707f319ac8` (`src/` + `lvgl/src`, el sello del banco).

**Alcance**: el banco **no tiene audio de radio ni tarjeta de sonido**, así que acá se mide la UI y la
persistencia del botón, no la convergencia de los parámetros. La convergencia es **requires-radio**
(procedimiento al final). La lógica del auto-ajuste se prueba entera en la suite nativa
(`tdd-red.log` → `tdd-green.log`).

## Qué se probó y con qué

- Binario ARM real de la imagen, bajo `qemu-arm-static` (`cortex-a7`), framebuffer falso y el socket
  de control (`tools/captura-banco.sh` + `tools/remote/x6200-remote`).
- Chroot privado `/tmp/opencode/vfix3-cw139` (copia del rootfs del banco) para no atribuir la
  evidencia al binario de otra sesión.
- Rebuild desde cero con el procedimiento del banco:
  `CHROOT=… BUILDROOT=… RECONSTRUIR=si tools/captura-banco.sh …`. Log `rebuild-desde-cero.log`;
  el gate #86 y el md5 quedan en `banco-build-gate.txt`.
- Algunos intentos intermedios del rebuild fallaron sin salida (la herramienta suprime la salida de
  `make`, y el volumen de build es compartido con otras sesiones); se declara para que no parezca un
  solo intento. El build final se hizo **dos veces desde cero** (`dirclean` + `rebuild`) y las dos
  dieron el mismo binario: `ccaa60d94315`.

## Secuencia medida

La ruta es la del usuario, sin atajos: grupo **GEN** → tecla **KEY** (grupo de páginas KEY) → botón
de página ×3 hasta `(CW 2:2)` → botón **Auto**. Cada orden y su respuesta cruda están en
`banco-secuencia.log`; las capturas, rotadas 270° como en la radio.

| # | Orden | Respuesta | Qué se ve |
|---|---|---|---|
| 1 | `{"cmd":"page","text":"gen"}` | `{"t":"ok"}` | página GEN (punto de partida conocido) |
| 2 | `{"cmd":"page","text":"key"}` | `{"t":"ok"}` | `(KEY 1:2)` |
| 3 | `{"cmd":"btn","val":0}` | `{"t":"ok"}` | `(KEY 2:2)` |
| 4 | `{"cmd":"btn","val":0}` | `{"t":"ok"}` | `(CW 1:2)` |
| 5 | `{"cmd":"btn","val":0}` | `{"t":"ok"}` | `(CW 2:2)` — captura `06-banco-cw2-auto-off.png`: **Peak beta 0.10 / Noise beta 0.80 / Auto: Off** |
| 6 | `{"cmd":"btn","val":3}` | `{"t":"ok"}` | captura `07-banco-cw2-auto-on.png`: el rótulo pasa a **Auto: On**; Peak/Noise conservan 0.10 / 0.80 |

`06` y `07` son el mismo binario y la misma página: el único cambio de píxeles es el rótulo del
botón, que es el indicador de AUTO.

## Persistencia (medida)

`params.db` del chroot, después de la pulsación:

```
$ sqlite3 /mnt/params.db "select name,val from params where name like 'cw%';"
cw_decoder|1
cw_tune|1
cw_decoder_auto|1
```

Y el arranque siguiente (la GUI se reinició en el banco) lo carga:

```
[User] cfg_params_load_item: Loaded cw_decoder_auto=1 (pk=0)
```

El log de la sesión muestra los saves del toggle (`Saved cw_decoder_auto=1`, `=0`, `=1`), o sea que
el botón recorre el subject, el `cfg_item_t` y la base. Evidencia cruda en `banco-persistencia.txt`.
La captura `08-banco-reinicio-auto-on.png` es la página `(CW 2:2)` **después de reiniciar la GUI**,
con `Auto: On` leído de la base.

## Lo que el banco NO prueba

- **Convergencia con señal**: no hay audio de radio en el banco (`db` del S-meter en `-121`, sin
  muestras). Con AUTO encendido y sin señal el módulo no adapta nada (guarda `signal_ratio > 1` y
  Δ ≥ 6 dB): Peak/Noise seguían en 0.10/0.80. Es exactamente lo que el issue pide ("no perseguir
  ruido"), pero no es evidencia de que los valores converjan.
- **Tono de RX vs TX**: `cfg.key_tone` no se tocó en el banco porque el DDS no converge sin audio;
  la separación está en el código (`src/cw.cpp`: `key_tone + rx_tone_offset`, `cfg.key_tone` nunca se
  escribe) y la suite nativa cubre el signo/acotado del offset.

## requires-radio — procedimiento para el owner

1. Encender el decodificador CW (`Decoder: On`) en una señal de CW real (o un generador de audio en
   la banda, con el tono por debajo de 500 Hz de desviación).
2. Ir a `(CW 2:2)` y pulsar **Auto: On**. En la primera ventana (~5 s, y ya con la ventana previa
   acumulada) los botones de Peak/Noise/Dec SNR deben mostrar valores distintos de los manuales
   según la señal; con una señal fuerte se espera `Dec SNR` cerca de 10 dB y `Noise beta` 0.8–0.9;
   con un piso nervioso, 0.5–0.7.
3. Con una señal débil (< 6 dB sobre el piso) o sin señal, **no debe cambiar nada** (los valores
   quedan como estaban).
4. Mover el dial ±100–200 Hz (o el generador) y verificar que el bloque del widget de tune se
   recentra en pasos de ≤ 30 Hz por ventana, sin saltos, y que el texto decodificado no empeora.
5. Apagar AUTO: los valores quedan como los dejó (visibles/editable a mano) y el DDS vuelve al tono
   manual. Encender de nuevo: recalibra en la primera ventana.
6. Verificar que el **tono de TX** (`cfg.key_tone`, botón `Tone` de `(KEY 1:2)`) no cambió en ningún
   momento.

Pendiente de calibrar en radio: el techo de `snr` (10 dB), la guarda de 6 dB y si `peak_beta` a
0.10 hace chatter con señales muy fuertes (ver `docs/HALLAZGOS.md` 53).

## Índice de la evidencia

| Archivo | Qué es |
|---|---|
| `tdd-red.log` / `tdd-green.log` | RED (stub que devolvía ceros: 70 fallos) → GREEN (18 621 comprobaciones, 0 fallos) |
| `native-host.log` | salida de la suite nativa dentro de `run_all_tests.sh --quick` |
| `run-all-quick.log` | 12 OK, 0 fallos (los codecs WSPR/SSTV/WeFax se omiten por la imagen sin deps) |
| `check-matrix.log` | matriz por paths: nativos, 8 estáticos, determinismo, A/B, bench (exit 0) |
| `ab-cw139.log`, `ab-cw139-report.md`, `ab-cw139-results.json` | A/B `AB-CW139` (A `7222abae`, B `e989c040`): **PASS**, 85 escenarios pixel-idénticos |
| `contracts.log`, `audit.log`, `themes.log` | 893 checks / 0 fallos (87 escenarios), auditoría estructural 0 hallazgos, temas OK |
| `signo-peak-freq.c/.txt` | medición del signo de `peak_freq` con liquid 1.4.0 (HALLAZGOS 52) |
| `rebuild-desde-cero.log`, `banco-build-gate.txt` | cross-compile ARM del banco, gate #86 y md5 del binario |
| `banco-secuencia.log` | órdenes y respuestas crudas de la secuencia |
| `banco-persistencia.txt`, `banco-params-db.txt` | `params.db` y línea de arranque con `cw_decoder_auto` |
| `00-banco-home.png`, `01-banco-cw.png`, `02-banco-cw1.png`, `09-banco-cw2-final.png` | capturas de la ruta (home, modo CW, `(KEY 1:2)`, `(CW 2:2)` con el binario final) |
| `06-banco-cw2-auto-off.png`, `07-banco-cw2-auto-on.png`, `08-banco-reinicio-auto-on.png` | el botón en `(CW 2:2)`: Off → On, y On después de reiniciar la GUI |
| `determinism.log`, `py-gates.log`, `pytest-e2e.log` | determinismo del arnés, ruff/format/ty/pytest (`227 passed`, e2e `13 passed`) |
