# QA backlog 2026-10-18 — #133 / #114 / #121: **bloqueado, la radio no responde**

**Fecha:** 2026-10-18 ~12:40 (hora radio) / 01:36 -03 · **Rama:** `lu2dsx/x6200-rom`
**Commit del trunk:** `94b9989d1b32` (punta de QA; el último commit de código es `d3e84ed`, merge del #133)
**Binario:** build **limpio** → `ffef63df58cb854a5e63bcf88f645594` (13.757.724 B)
**Gate #86:** `OK ... ELF32 ARM, 13757724 B, 3/3 marcas` (exit 0)
**Deploy:** **NO EJECUTADO** — la radio no está en la red.

## Qué se hizo (medido)

```
  git fetch fork                                  → fork/lu2dsx/x6200-rom = 94b9989 (sin cambios)
  tools/compila-gui.sh                            → exit 0, md5 ffef63df58cb, 13.757.724 B
  python3 tools/verifica-firmware-port.py <bin>   → OK, 3/3 marcas (gate #86, exit 0)
  tools/prueba-gui-en-radio.sh                    → exit 1: radio_ip() no encontró la radio
  tools/verifica-radio.sh 192.168.0.236           → "la radio (192.168.0.236) no responde" (exit 1)
```

El binario quedó listo en el volumen `x6200-build-output` (y una copia en `.tmp-backlog/` del clon),
pero **no se subió a la radio**.

## Por qué no se pudieron correr las tres pruebas

La radio está inalcanzable en **las dos redes** que usa:

| Red | Host | Radio conocida | ping | SSH | hosts con :22 en /24 |
|---|---|---|---|---|---|
| DharmaNet (5 GHz) | `192.168.0.195/24` | `192.168.0.236` | **0/2** | `No route to host` | **0** |
| DharmaNet 2.4 (2,4 GHz) | `192.168.18.68/24` | `192.168.18.67` | **0/2** | `No route to host` | **0** |

- Evidencia cruda: [`radio-down-5ghz.txt`](radio-down-5ghz.txt), [`radio-down-2ghz.txt`](radio-down-2ghz.txt).
- Además se reintentó SSH a `192.168.0.236` cada ~20 s desde 01:31:
  [`poll-radio.log`](poll-radio.log) — **todas las intentonas sin respuesta** (hasta el cierre de esta
  corrida).
- La red del host está sana (gateway `192.168.0.1` responde; otros equipos de la casa también).
- Es la **4.ª caída del #121**: la última medición viva fue a las 00:13 local (uptime 51.644 s,
  pid 7317); a las 00:33 ya no respondía. Sigue sin volver sola (el watchdog de 16 s no la levanta),
  igual que la 1.ª caída. **Necesita ciclo de energía** (y revisar la batería: en la medición de ROE
  del 17/10 estaba en **8,6 V**, dato ya anotado en el #121).

## Veredicto por issue

| Issue | Veredicto | Qué falta para cerrarlo |
|---|---|---|
| **#133** `swrscan` remoto | **NO MEDIDO** (la issue ya está cerrada por el merge; faltaba la re-validación en esta punta) | Con la radio arriba: abrir `SWRScan`, `{"cmd":"swrscan","val":1}` → `{"t":"ok"}`, poll de `get` hasta `Completed/Stopped/Error` con `swrscan_tx:false`, captura + `tests/swrscan_curve_evidence.py <png>` (1 arg, #132) → U/exit 0; Stop con `val:0`; doble Run; app cerrada → `app not open`. Todo a 0,1 W (lo fuerza el firmware) y con la antena/carga ficticia. |
| **#114** NB reinicia la radio | **NO MEDIDO** | 16 ciclos de `nb`/`nb_level`/`nb_width` (0 y medios), `uptime`/`pid`/`CRASHLOG` entre ciclos, barrido de apps y medición final. El fix del #111 (`b5822d2`, cola de eventos acotada) ya está en este build: es la condición que faltaba para poder cerrarlo como "no reproducido". |
| **#121** cuelgue en barrido de SWR | **NO MEDIDO** | 5 barridos consecutivos (spans 50/200/500 kHz, 1 pasada) con `uptime`/`pid`/`CRASHLOG` antes y después de cada uno y estado final. **Ojo:** el equipo se cae también **en reposo** (4.ª caída), así que un resultado limpio de los 5 barridos no cierra el riesgo de fondo; el watchdog del #125 sólo cubre el cuelgue *del barrido*. |

**No se cierra ningún issue con esta corrida y no se creó ningún issue nuevo**: la radio caída ya está
reportada en el #121; no hay un defecto nuevo que fundar.

## Estado en que queda la radio

No se tocó nada: no hubo deploy, no se abrieron apps, no se cambiaron params/temas. La radio queda
**inalcanzable**, como estaba al empezar. El build `ffef63df58cb` queda listo para desplegar cuando
vuelva.

## Archivos

| Archivo | Qué es |
|---|---|
| [`md5.txt`](md5.txt) | commit, md5, tamaño y hash de fuentes del build |
| [`build.log`](build.log) | salida de `tools/compila-gui.sh` (build limpio) |
| [`gate.txt`](gate.txt) | gate #86 sobre el binario compilado |
| [`deploy-intento.log`](deploy-intento.log) | intento de `tools/prueba-gui-en-radio.sh` (abortó sin radio) |
| [`verifica-radio-intento.log`](verifica-radio-intento.log) | `tools/verifica-radio.sh` (la radio no responde) |
| [`radio-down-5ghz.txt`](radio-down-5ghz.txt) | ping/SSH/scan en `192.168.0.0/24` |
| [`radio-down-2ghz.txt`](radio-down-2ghz.txt) | ping/SSH/scan en `192.168.18.0/24` |
| [`poll-radio.log`](poll-radio.log) | reintentos cada ~20 s (01:31→01:46) |
