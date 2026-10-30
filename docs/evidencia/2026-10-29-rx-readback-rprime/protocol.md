# Protocolo R′ — un único sondeo read-only del registro 12

Diseño del parche temporal (diff completo en `logs/probe-patch.diff`). **OFF por defecto**: sólo corre
si existe el flag temporal **no persistente** `/tmp/x6200_reg12_probe.enable` (sin tocar rootfs ni
`x6200_daemon`); sin flag no se abre `/dev/i2c-0`, no se hace ningún ioctl y no se escribe ningún log.
El parche **no se integra** a producción y no se commitea (PR sólo docs). Ejecución real: **una sola vez**
el 2026-10-30 03:17Z (ver §6).

## 1. Framing usado (el del GUI de fábrica, ya medido en #367)

- Bus `I2C-0`, esclavo `0x72`, `ioctl I2C_RDWR` con **2 mensajes**: escritura de **selector de 2 B BE**
  (`[addr&0xFF, addr>>8]`) + lectura de N bytes (`I2C_M_RD`).
- Esto **no** escribe un valor de registro (no hay payload de 6 B como `x6200_control_cmd`); sólo el
  selector de dirección que el framing del vendor (`BaseCommIIC::iicRead`, REA #367) usa para leer.

## 2. Único intento y límites duros (código)

| paso | operación | longitud | observación |
|---|---|---|---|
| a | leer `0x2000` (readiness baseband) | 1 B | si falla o byte==0 ⇒ **abortar**, no se sigue |
| b1 | leer `0x0038` (reg 14 `rxvol`) | 4 B | control conocido: valor plausible 0..100 |
| c1 | leer `0x0030` (reg 12 `sple_atue_trx`) | 4 B | el objetivo |
| pausa | `usleep(50 ms)` **sin retener `radio_lock`** | — | cada transacción es atómica bajo lock |
| b2 | leer `0x0038` de nuevo | 4 B | sólo si a/b1/c1 fueron válidas |
| c2 | leer `0x0030` de nuevo | 4 B | sólo si a/b1/c1 fueron válidas |

- **Máximo 5 lecturas** (`RXPROBE_MAX_READS=5`). **Sin reintentos**: un ioctl fallido no se repite.
- **Sólo esas tres direcciones** (hard-coded); cualquier otra ⇒ error del backend/mock.
- **No hay función de escritura** en el backend (`i2c_read` únicamente): imposible escribir un valor.
- **Segunda ejecución rechazada** (`RXPROBE_ALREADY_DONE`); un flag en memoria lo garantiza.
- El gate `radio_control_ok && state == RADIO_RX` se **rechequea antes de la primera y de la segunda
  transacción**; si cambia ⇒ `BLOCKED_UNSAFE` sin leer/escribir. Writer concurrente ⇒ aborta.
- Punto de ejecución: final de `radio_init()`, con `control_mux` inicializado y `radio_control_ok=true`,
  **antes** de `pthread_create(radio_thread)`: no existe aún el hilo que llama `x6200_control_idle()`;
  la GUI es el único dueño del bus y no hay writers concurrentes de `audio_play_en` en ese instante.
- El log incluye `READBACK_PROBE_DONE verdict=… reads=…`, bytes HEX crudos, RC/errno, tiempos
  monótonos, decodificación LE u32 y la **sombra** (`x6200_control_get`) sólo como diagnóstico.
  La sombra **nunca** se usa como readback.

## 3. Clasificación automática

| condición observada | veredicto |
|---|---|
| control `0x2000` falla / errno | `R_NO_READBACK_ON_TESTED_PROTOCOL` |
| b/c devuelven error o longitudes raras | `R_NO_READBACK_ON_TESTED_PROTOCOL` |
| b/c estables y no triviales, control OK | `R_CANDIDATE_FOUND` (**sólo hipótesis**) |
| b/c uniformes `00000000`/`FFFFFFFF` | `R_INCONCLUSIVE` (uniformidad no prueba error ni éxito) |
| b/c cambian entre las dos lecturas | `R_INCONCLUSIVE` |
| `tx=true`/estado no RX/writer concurrente | `BLOCKED_UNSAFE` |
| segunda llamada | `ALREADY_DONE` |

`R_CANDIDATE_FOUND` **no** es `A_PASS` ni readback real: exige verdad física (cambio dinámico
verificado + ownership/guardián + A4), que corresponde a #363; sin instrumentación presencial el
veredicto máximo sería `R_PRIME_PHYSICAL_NOT_VERIFIED`.

## 4. Tests host (sin hardware, sin tocar la radio)

`tests/native/test_rx_reg12_probe.c` con backend mock inyectable: válido, timeout, bus busy,
`tx=true` (aborta antes del bus), writer concurrente, trivial `00/FF`, respuesta inestable, flag OFF
por defecto, segunda ejecución y conteo de lecturas/direcciones/longitudes (0 payloads de escritura).

Resultado real: **32 comprobaciones, 0 fallos, exit 0** (`logs/host-mocks.txt`).

## 5. (histórico) Primer intento: no ejecutado por BLOCKED_PREFLIGHT

Por `BLOCKED_PREFLIGHT` (radio en uso activo; ver `README.md` y `logs/preflight-monitor.log`) y
`BLOCKED_BUILD` (pipeline #351 vacío, ver `logs/build-pipeline.txt`) **no se desplegó el binario
temporal ni se ejecutó el sondeo**. `readback-values.csv` no tiene filas de datos: no hubo lectura.

## 6. Ejecución real (reintento 2026-10-30 03:17Z)

1. Binario ARM parcheado compilado desde `rom-build/clone-gui` con el árbol completo
   `rom-build/clone-buildroot/build` (`make x6200-gui-dirclean && make -j8 x6200-gui`, exit 0);
   sha256 del binario temporal `25f281b05bfd10990c47d6f38d369af93171c6bdaf751161987b728b121ac19d`.
2. Backup del binario original en la radio (`/mnt/ui-bak-rprime-20261030-031717/x6200_gui`, sha256
   `4ee87aef…66b0`) y prueba de rollback (copia del backup sobre instalado, hash idéntico).
3. Deploy **sólo** de `/usr/sbin/x6200_gui` (temporal), `chmod 755`, hash verificado `25f281b0…`.
4. Flag `/tmp/x6200_reg12_probe.enable` creado; `kill $(pidof x6200_gui)`; el daemon relanzó la GUI;
   el sondeo corrió una vez en `radio_init` (log `READBACK_PROBE_DONE`); flag eliminado de inmediato.
5. Rollback inmediato al binario original; `kill` + relanzamiento; hash original, `tx=false`, pid 15470
   verificados; temporales `/tmp` borrados.

Resultado: `R_INCONCLUSIVE` (ver `README.md` y `readback-values.csv`).
