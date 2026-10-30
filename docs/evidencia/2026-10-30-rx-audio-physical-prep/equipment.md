# Equipo mínimo y disponibilidad REAL — #371

**Fecha de la verificación: 2026-10-30.** Fuentes usadas, en este orden: (1) evidencia ya versionada del
repo; (2) inspección **estática** de este host de trabajo (sin conectar nada a la radio); (3) manual
público Xiegu X6200 Rev.05; (4) documentación externa Mestre. **No hubo contacto con la radio, ni
confirmación del owner, ni compras.** El owner estará físicamente con el equipo **desde 2026-11-12**.

## Leyenda de disponibilidad

| Estado | Significado |
|---|---|
| **PRESENTE-DOCUMENTADO** | Existe evidencia versionada de que el elemento está disponible en el proyecto; igual hay que re-verificarlo en la ventana. |
| **PRESENTE-HOST** | Existe en esta máquina de trabajo, pero **no** es un instrumento de medida con límites declarados. |
| **NO DECLARADO** | No hay ninguna evidencia de que exista. Se trata como **NO DISPONIBLE** hasta que el owner lo confirme y se registre en la ventana. |
| **NO DISPONIBLE** | Comprobado que en esta corrida no está (p. ej. sin interfaz USB de audio adjunta). |
| **NO IDENTIFICADO** | No existe método/fuente conocido en el proyecto; no es un tema de compra. |

> **Regla:** nada se asume por “debería haber”. Lo no confirmado es **no disponible** y bloquea el paso que
> lo necesite. El agente **no compra** hardware ni pide compras sin justificación eléctrica.

## Inventario

| # | Ítem | Mínimo exigido | Disponibilidad real (2026-10-30) | Fabricante/modelo/límites conocidos | Fuente | Impacto |
|---|---|---|---|---|---|---|
| 1 | Radio Xiegu X6200 con antena en recepción y alimentación estable | Equipo operativo en RX, sin ATU/TX/PTT/CW | **PRESENTE-DOCUMENTADO**; acceso físico recién desde **2026-11-12** | X6200, `BASE ver1 1.0.0`, fw **1.0.6**; alimentación 9–15 V DC ≥3,5 A (manual Rev.05); antena del owner. **No** se usa el PA ni el TX | `2026-10-29-rx-audible-route-363/logs/a0-preflight.txt`; manual Rev.05 (power supply) | Habilita P0 sólo con operador e instrumento |
| 2 | Operador/owner presente | Persona que opera la radio, escucha y puede cortar el ensayo | **PRESENTE desde 2026-11-12** (no antes) | — | issue #371 (“El propietario no estará en la radio antes del 12/11/2026”) | Obligatorio en P0; sin operador no se conecta |
| 3 | Notebook/PC para guardar WAV/PCM 44100 Hz, reloj monótono y hashes | Host con captura de audio y `sha256` | **PRESENTE-HOST** (esta máquina) | ThinkPad P14s Gen5 AMD, Fedora 44, kernel 7.2.11; codecs HDA **Realtek ALC257** + HDMI/AMD; **sin interfaz USB de audio** (`lsusb`, 2026-10-30) | inspección estática del host (2026-10-30) | Necesario pero **no suficiente**: no es un instrumento de medida |
| 4 | Cable TRS macho 3,5 mm **de tres contactos** | TRS, no TS; un solo cable identificado | **NO DECLARADO** | — | no hay evidencia en repo | Bloquea P0: sin cable correcto **no se conecta** |
| 5 | Breakout / medidor de continuidad (o multímetro) | Verificar pinout con radio **apagada** y cable desconectado | **NO DECLARADO** | — | no hay evidencia en repo | Bloquea P0: `connector_verified=false` ⇒ no se conecta |
| 6 | Interfaz de audio USB externa con **entrada LINE**, alta impedancia, idealmente aislamiento galvánico | Entrada LINE (no MIC con bias), tolerancia de entrada declarada | **NO DISPONIBLE** en esta corrida / **NO DECLARADO** en el proyecto | — | `lsusb` sin dispositivo de audio; evidencia #363: “sin interfaz de audio aislada” | Bloquea P0; si sólo hay MIC/plug-in-power ⇒ `BLOCKED_UNSAFE_AUDIO_INPUT` |
| 7 | Aislamiento galvánico (o interfaz aislada) | Romper lazos de masa entre radio y host | **NO DECLARADO** | — | no hay evidencia en repo | Requisito del prompt; sin declararlo no se conecta el host |
| 8 | Atenuador pasivo ajustable / limitador con valores conocidos | Atenuar la salida S/P a un valor seguro para la interfaz | **NO DECLARADO** | — | no hay evidencia en repo | Sin atenuador con valores **no se dimensiona nada** (no se inventan voltajes) |
| 9 | Instrumento para medir los límites reales de S/P (multímetro True RMS / osciloscopio / sonda) antes de conectar la interfaz | Medir DC y nivel AC de S/P con radio en volumen bajo y entrada de alta impedancia | **NO DISPONIBLE declarado** | — | #363: “sin osciloscopio, sin analizador, sin carga ficticia, sin interfaz de audio aislada”; #0 CB TX: “no hay analizador de espectro / wattmeter” | Sin este paso, el límite de entrada es desconocido ⇒ `BLOCKED_UNSAFE_AUDIO_INPUT` |
| 10 | Sonda externa **no invasiva** del estado real del MUX (select/feedback fuera del equipo) | Fuente de verdad física del estado del MUX | **NO IDENTIFICADA** | — | #363 `handoff-b.md` §2.3; #367 `readback-re.md`; #370 | P1 sigue `BLOCKED_NO_SAFE_RESTORE`; sin esto **no se autoriza MUX** |
| 11 | Documentación OEM verificable del MCU/FPGA (ventana de lectura/estado del MUX) | Documento oficial que especifique un readback con semántica conocida | **NO DISPONIBLE** | fw del MCU encriptado (`.xgf`, 512 KB, sin strings); REA detenida por decisión del épico | #363 `reverse-engineering.md` §5; #369/#370 | Podría reabrir P1 si se aporta de forma verificable; no se busca más por REA |
| 12 | Fixture de audio propio/licenciado (≤ −24 dBFS) si se publica una señal | Material propio o con derecho de uso, inventariado | **NO DISPONIBLE para radio** | el único WAV versionado (`2026-10-15-banco-audio/captura-voz.wav`) es prueba sintética en QEMU, **no** señal HF | `2026-10-28-rx-audio-hardware/physical-handoff.md` §3 | Para P0 no es imprescindible (se usa señal RX propia o ruido); **prohibido** versionar voz de terceros |
| 13 | Medio de backup/rollback del GUI | Copia + hash verificados antes/después | **PRESENTE-DOCUMENTADO, estado actual no verificado** | GUI conocida `sha256 4ee87aef…66b0` / `md5 80bbe5bd70b3`; backups `/mnt/ui-bak-*` (p. ej. `/mnt/ui-bak-rprime-20261030-031717/`) | #369/#370 `rollback.md`, `logs/restore.txt` | P0 **no** despliega nada; sirve como red de seguridad y para el snapshot del preflight |
| 14 | Manual de la revisión real del equipo | Manual Rev.05 en mano antes de insertar cable | **PRESENTE-DOCUMENTADO (PDF público)**; revisión física **sin verificar** | Xiegu X6200 Rev.05, `sha256` del PDF en `commands.log`; S/P 3,5 mm TRS AUDIO/NC/GND; S/P MODE `EAR PHONE`/`SPEAKER` | `https://radioddity.s3.amazonaws.com/Xiegu_X6200_Manual_20240925.pdf` | Condición del handoff: contrastar dibujo y advertencias |
| 15 | Documentación externa Mestre | Sólo como **hipótesis** (otro equipo/ROM) | **PRESENTE-DOCUMENTADO (externa)** | dos fuentes BASE RX / DAC SoC; `voice_rec` = selector; PA por `rxvol>0`; reloj `pll-audio` | `https://git.sa6anw.se/sa6anw/mestre/src/branch/main/audio-architecture.md` | **No** es evidencia de nuestro ejemplar; no habilita escritura I²C |

## No se necesita (y no se usará en este plan)

- Medidor de potencia, carga ficticia, analizador de espectro, wattmeter, atenuador de RF: **no hay TX en
  este plan** y no se induce PTT/ATU. Fueron necesarios en otros issues de TX, no acá.
- Sonda interna, soldadura, apertura de chasis, programador de MCU/FPGA: **prohibido** como iniciativa del
  agente; un estudio de laboratorio con el equipo fuera de servicio y técnico es un trabajo futuro aparte.
- Compras: **no**. Cualquier adquisición la decide el owner; el agente sólo declara características mínimas.

## Cómo se verificó la disponibilidad (y qué no)

- **Evidencia versionada:** #363 (`BLOCKED_SAFE_MUX`, A4 `NOT_EXECUTED`), #369 (`R_INCONCLUSIVE`,
  rollback verificado), #351/#359 (captura Linux, no salida S/P), #347 (handoff: sin instrumentación),
  #0 CB TX (sin wattmeter/analizador).
- **Inspección estática del host (2026-10-30):** `uname -a`; `lsusb` (sin interfaz USB de audio);
  `/proc/asound/cards` y `codec#0` (ALC257 + HDMI/AMD). No se ejecutó captura ni prueba de audio.
- **Manual Rev.05:** se verificó el dibujo del S/P (AUDIO/NC/GND) y las advertencias en el PDF público; el
  `sha256` del PDF quedó registrado en `commands.log`.
- **No verificado:** existencia física de cables, atenuador, medidor de continuidad, osciloscopio, interfaz
  LINE, aislamiento, ni que el host esté junto a la radio el 2026-11-12. Tampoco se verificó la revisión
  impresa del manual ni el estado actual del GUI de la radio.

## Si en la ventana falta equipo: decisión

- Falta **cable TRS de 3 contactos**, **medidor de continuidad**, **interfaz LINE aislada**, **atenuador** o
  **instrumento de límites** ⇒ **`P0_PENDING_EQUIPMENT`**: no se conecta nada y se documenta el faltante.
- Sólo hay entrada **MIC con plug-in power** o una interfaz sin tolerancia declarada ⇒
  **`BLOCKED_UNSAFE_AUDIO_INPUT`**: no se conecta la salida S/P.
- No hay fuente de verdad del MUX ⇒ **P1 `BLOCKED_NO_SAFE_RESTORE`** / `NO_SAFE_MUX_STATE_TRUTH`: no se
  conmuta el MUX aunque el audio se escuche.
- Hay audio audible pero no hay captura instrumentada ⇒ **no** se declara `AUDIO_OUTPUT_VERIFIED`; como
  máximo queda una observación cualitativa del operador, fuera del CSV de medición.

**Ningún faltante habilita una solución improvisada** (sonda casera, abrir el equipo, conectar a ciegas,
prestar otro puerto). El equipo queda exactamente como está hasta que el owner decida.
