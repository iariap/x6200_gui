# F0 en vivo (parcial, sin instrumentos) — TX en 27.185 MHz, 19/10 14:35–14:36 UTC

**Autorización**: el owner pidió expresamente "hacer lo posible por validar la transmisión en CB
con la radio como está" (mensaje al Lead, 2026-10-19), después de autorizar la F0. Se transmitió
**dentro del alcance mínimo posible**: barrido de ROE con potencia forzada a **0,1 W** por el
propio firmware (`SWRSCAN_TX_POWER_W = 0,1 W`, `src/dialog_swrscan.h:72-75`), **una sola pasada**
(`SWRSCAN_MULTIPASS_TX_ENABLED=0`), ATU desactivado por tecla (no se pudo confirmar), sin XPA125B.
No se usó la voz ni potencia alta.

**Método**: socket de control de la GUI (`/tmp/x6200-remote`, el mismo protocolo del control
remoto y de la página `/remote`). El barrido es el **único camino de TX accionable por software
en AM/FM** que no depende de una tecla física (el socket no tiene PTT; el PTT del micrófono es del
MCU).

## 1. Estado antes (medido, `{"cmd":"get"}`)

```text
$ /tmp/x6200-remote '{"cmd":"get"}'
{"t":"state","vfo":"A","freq":27185000,"freq_bg":26995000,"mode":"LSB","band":"CB C",
 "filter":3960,"flo":10,"fhi":3970,"rfgain":0,"sql":0,"vol":0,"pre":1,"att":0,"agc":"AUTO",
 "nb":0,"nr":0,"nb_level":5,"nb_width":50,"nr_level":0,"pwr":50,"db":-81,"tx":false,
 "app":"","ax":{},"cwt":null,"logrev":0,"did":""}
```

La radio **ya estaba en 27.185.000 MHz, banda CB C, modo LSB, RX** (la había dejado el owner; no
se cambió la frecuencia). `ATU1` visible en la barra superior. Se puso modo **AM** para el intento
(`{"cmd":"mode","val":"AM"}`) — después se restauró.

Capturas: [`00-antes-rx.png`](00-antes-rx.png) (LSB, 27.185, ATU1, S -81).

## 2. Búsqueda de un PTT accionable por software (sin resultado)

- **Socket JSON**: no tiene PTT (`src/remote_server.cpp:20-22`, *"There is no PTT: only the apps
  transmit"*).
- **CAT serial `/dev/ttyS2`**: sí tiene `C_CTL_PTT 0x1C` (`src/cat.cpp:778-795`), pero el puerto
  es un UART: escribir en `/dev/ttyS2` desde la propia radio saca los bytes por el pin TX, **no**
  entran al parser de la GUI. Haría falta un serial externo o wfview.
- **wfview LAN**: `/mnt/wfview.conf` está con `enabled=0` (aunque los UDP 50001-50003 están
  bindeados). Habilitarlo exige escribir en `DATA` y armar el cliente Icom LAN: **no se hizo**.
- **Tecla PTT superior por evdev**: se prepararon `input_event` sintéticos
  (`BTN_TRIGGER_HAPPY4`, `event0` = `matrix_keypad`) y se inyectaron. La radio **no cambió de
  estado** (`tx` siguió `false` y la tecla ATU tampoco cambió el indicador): la inyección no llegó
  a la GUI en este build. **No se insiste**: es un camino no soportado y podía dejar una tecla
  pegada.

## 3. El barrido de ROE SÍ transmite en 27 MHz (medido)

Secuencia (app abierta con `{"cmd":"app","text":"SWRScan"}` y luego Run):

```text
$ /tmp/x6200-remote '{"cmd":"swrscan","val":1}'
{"t":"ok"}

$ # sondeo cada 0,4 s durante el barrido:
{"t":"state",...,"mode":"LSB","app":"SWRScan",
 "ax":{"swrscan_state":"Scanning","passes":1,"pass":1,"points":51,"swrscan_tx":true},"tx":true}
{"t":"state",...,"mode":"LSB","app":"SWRScan",
 "ax":{"swrscan_state":"Scanning","passes":1,"pass":1,"points":51,"swrscan_tx":true},"tx":true}
{"t":"state",...,"app":"SWRScan","ax":{"swrscan_state":"Error","passes":1,"pass":0,
 "points":51,"swrscan_tx":false},"mode":"AM","tx":false}
```

- **`tx:true`**: el baseband/MCU reportó TX y la GUI entró en estado de transmisión. **El MCU NO
  bloquea el TX en 27 MHz.**
- **La pantalla del barrido** ([`06-swrscan-run.png`](06-swrscan-run.png)):
  `min 1.0 @ 26.935.000 kHz (n=43/51)` y una curva de ROE dibujada de **26,935 a 27,435 MHz**
  (span 500 kHz centrado en 27,185). **43 de 51 puntos** tienen muestra: el puente de ROE del
  equipo midió RF real en 43 frecuencias dentro de la banda CB. En 27,185 la curva está en ~1,1;
  el mínimo (1,0) quedó en el borde inferior del span (26,935).
- **Es la misma firma que en 40 m** documentada en `docs/PRUEBAS.md`: termina en
  **`Error: timeout de RF`** con `n≈44/51` y nunca `Completed`. Es el defecto conocido del
  barrido (issues #133/#125), **no** un bloqueo de TX.

## 4. Restauración (medido)

```text
$ /tmp/x6200-remote '{"cmd":"swrscan","val":0}'   → {"t":"ok"}
$ /tmp/x6200-remote '{"cmd":"app","text":"off"}'  → {"t":"ok"}
$ /tmp/x6200-remote '{"cmd":"mode","val":"LSB"}'  → {"t":"ok"}
$ /tmp/x6200-remote '{"cmd":"vol","val":0}'       → {"t":"ok"}
$ /tmp/x6200-remote '{"cmd":"pwr","val":50}'      → {"t":"ok"}
$ /tmp/x6200-remote '{"cmd":"freq","val":27185000}' → {"t":"ok"}

$ /tmp/x6200-remote '{"cmd":"get"}'
{"t":"state","vfo":"A","freq":27185000,"freq_bg":26995000,"mode":"LSB","band":"CB C",
 ...,"vol":0,...,"pwr":50,"db":-81,"tx":false,"app":"","ax":{}...}
```

- Vuelto a **27.185.000 MHz, LSB, CB C, RX, vol 0, app cerrada**, sin sesión de barrido.
- La GUI siguió con el **mismo PID 13127** (no se cayó ni reinició; uptime del equipo 18:51).
- El modo AM que se puso para el intento quedó **restaurado a LSB** (el barrido también lo había
  restaurado). La frecuencia nunca se cambió.
- Captura final: [`07-despues-rx.png`](07-despues-rx.png) — igual a la de antes (LSB, 27.185,
  ATU1, S -81; el reloj/batería de la esquina rota normalmente).
- **ATU**: el indicador `ATU1` está igual antes y después; el intento de apagarla por tecla no
  cambió el indicador, así que **no se puede afirmar que la F0 haya corrido con ATU off**.

## 5. Qué quedó validado y qué NO

| Pregunta | Resultado | Fundamento |
|---|---|---|
| ¿El MCU/baseband bloquea el TX en 27 MHz? | **NO** (medido) | `tx:true` + 43/51 muestras de ROE en 26,935–27,435 MHz |
| ¿Hay RF real en 27,185? | **SÍ** (medido por el puente del equipo) | curva de ROE del barrido, `n=43/51`, min 1,0 |
| ¿Potencia real de salida? | **NO MEDIDA** | no hay medidor de potencia/RF; la del barrido es 0,1 W forzada por firmware |
| ¿Armónicos (2.º/3.er)? | **NO MEDIDOS** | no hay analizador de espectro |
| ¿TX en AM/FM a potencia configurada? | **NO MEDIDO** | el barrido fuerza LSB y 0,1 W; no hay PTT accionable por software para AM/FM |
| ¿PTT del micrófono? | **NO MEDIDO** (requires-radio) | lo maneja el MCU; hay que apretarlo físicamente |
| ¿ATU desactivado durante la prueba? | **NO CONFIRMADO** | el indicador siguió en ATU1 pese a la tecla inyectada |

## 6. Advertencia legal (Argentina, aplica a esta transmisión)

La prueba transmitió en **27,185 MHz** (banda CB C, canal 19 del plan CEPT), a **0,1 W** y por
~1,2 s. En Argentina el SBC (26,960–27,410 MHz) exige **autorización por estación y equipo
homologado/registrado** (Res. SC 47/1982; CABFRA 2025 de ENACOM); el X6200 **no está homologado
como equipo CB** y 27 MHz no es banda de aficionados. La prueba se hizo porque el owner la pidió
explícitamente y con su equipo; **no habilita a operar** en CB ni cambia la homologación. Ver
[`../README.md`](../README.md) §6.
