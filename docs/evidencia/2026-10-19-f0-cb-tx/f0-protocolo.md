# F0 — TX en CB 27 MHz (issue #146): gates en Linux y protocolo de prueba

> **Estado de este documento: protocolo, NO ejecutado.** Lo escribe el análisis de código
> (teammate `analisis-f0`, task-3) el **2026-10-19**. No se transmitió, no se abrió sesión de
> control sobre la radio, no se desplegó, no se flasheó y no se editó código del repo. El único
> archivo escrito es este. Ver §0.3 (constancia de no transmisión).

| Dato | Valor |
|---|---|
| Fase | **F0** del issue [#146](https://github.com/iariap/x6200_gui/issues/146): nada de código, TX real en 27 MHz con carga fantasma + potencia + armónicos + PTT |
| Fecha de la evidencia | 2026-10-19 |
| Repo del port analizado | `/home/pablo/desa/x6200/x6200_qa`, rama `lu2dsx/x6200-rom`, commit `1e3a1e498fd1` (2026-10-19 10:39 -03) |
| GUI desplegada en la radio (medido) | `/usr/sbin/x6200_gui` md5 **`9a094c421b0eb05c103abf782ea00db6`** (recon read-only 19/10; `recon-radio.txt` §4) |
| Radio | X6200 del owner, `192.168.0.236` (SSH root) — **no tocada por esta tarea** |
| Librería de control | `aether-x6200-control` vendorizada en `/home/pablo/desa/x6200/AetherX6200Buildroot/buildroot/dl/aether-x6200-control/git/` (sin `.git`; archivos del 2026-02-10). El port la enlaza: `src/CMakeLists.txt:69` |
| Convención de estados | `medido` / `código` / `no medido` de [`docs/PRUEBAS.md`](../../PRUEBAS.md) §"Cómo se registra una corrida" (líneas 6-22) |
| Documento | `docs/evidencia/2026-10-19-f0-cb-tx/f0-protocolo.md` (este archivo) |

**Nota sobre números de línea del issue #146:** el propio issue cita la base en la que se
escribió (`radio.c:583/593/858`, `main_screen.c:867`, etc.). La rama analizada está más adelante:
las líneas equivalentes son `radio.c:622/632/907` y `main_screen.c:902`. En este documento se
citan **siempre las líneas de la rama actual**. La semántica no cambió.

---

## 0. Resumen ejecutivo (las cuatro respuestas)

1. **¿Hay gate de banda/frecuencia en Linux que impida transmitir en 27 MHz? NO.** No hay ningún
   chequeo de banda en el camino de TX. El único chequeo en Linux es de *rango* (`radio_check_freq`
   acepta 0,5–55 MHz, `src/radio.c:632-637`) y una *tabla de bandas* que sólo decide si la
   frecuencia se puede **sintonizar** (`src/cfg/band.c:135-160`, `:599-606`), no si se puede
   transmitir. `radio_set_ptt()` sólo prende el bit `x6200_iptt` por I2C (`src/radio.c:907-909`,
   lib `src/control.c:156-166`). El flag `fullband-tx` del firmware oficial **no existe** en esta
   ROM (medido: `/etc/xgradio/` no existe y el barrido de todo el rootfs no encuentra
   `xgradio`/`fullband`; `recon-radio.txt` §1, §2, §7). El gate histórico del firmware oficial era
   **UI-side** (`/etc/xgradio/xgradio.conf` línea 7, leído por la UI Qt `x6200_ui_v100`;
   `ref-fabrica.md` §3-§5), y el port no tiene ese código. El **único gate posible que queda es el
   baseband/MCU/FPGA (STM32)**, que es firmware aparte y no se toca desde Linux: eso es justo lo
   que F0 mide. Detalle en §1–§3.
2. **¿Se puede fijar 27.185 MHz, AM/NFM y potencia por el control remoto (socket JSON)?**
   **Frecuencia y potencia sí. AM sí. NFM por el socket NO es confiable** (bug de mapeo: el nombre
   `"NFM"` escribe el modo 7 = **SAM**; ver §3.1.1). El 26.515 MHz también se puede fijar (cae en
   `CB B`; §3.1 y §1 G2). Todo depende de que la frecuencia caiga en una fila de la tabla `bands` de
   `/mnt/params.db` (§1 G2), cosa que hoy se cumple para 27.185 (CB C) y 26.515 (CB B) con los CSV
   del repo (`sql/bands_cb.csv:2-3`).
3. **¿Se puede accionar PTT por control remoto?**
   - Por el **socket JSON**: **NO**. Está documentado en el propio servidor: *"There is no PTT:
     only the apps transmit"* (`src/remote_server.cpp:20-22`).
   - Por **CAT serial** (`/dev/ttyS2`): **SÍ**, comando CI-V **`C_CTL_PTT 0x1C`** con datos
     `00 01` (on) / `00 00` (off) → `radio_set_ptt()` (`src/cat.cpp:778-795`, `:999`).
   - Por **wfview (Icom LAN, UDP 50001-50003)**: **SÍ**, las tramas CI-V que llegan por el puerto
     50002 se enrutan al mismo parser y por lo tanto el mismo `0x1C` acciona PTT
     (`src/wfview_server.c:9-12`, `:334-340`, `:516-546`; requiere habilitar el servidor en
     Ajustes, `src/dialog_settings.c:960-993`, `src/main.c:615-616`).
   - Por **apps**: sí, pero sólo en los modos que cada app fuerza (FT8/CW/JS8/WSPR/SSTV/WeFax), y
     por **`swrscan`** (0,1 W, LSB, §3.1.2). Ninguna clave AM/FM.
   - Por **PTT físico** de la radio (trigger superior): sí, `KEYPAD_PTT` →
     `radio_set_ptt()` (`src/keypad.c:117-119`, `src/main_screen.c:902-925`). El **PTT del
     micrófono** probablemente lo maneja el MCU y no Linux: `requires-radio` (medirlo en F0).
4. **¿Qué instrumento falta para declarar F0?** **Medidor de potencia/RF calibrado + analizador de
   espectro (con atenuador) + carga fantasma 50 Ω.** Sin eso **F0 no puede ser PASS**. La
   telemetría del equipo (detector interno) **no es un instrumento calibrado** y no sustituye a
   ninguno (§5). Si tampoco hay autorización del owner, **ni siquiera se ejecuta** (§6–§7).

### 0.3 Constancia de NO transmisión de esta tarea (2026-10-19)

- Agente `analisis-f0` (task-3 del Team). Trabajo: **sólo lectura/análisis y escritura de este
  documento**.
- **No se ejecutó** `ssh`, `sshpass` ni ningún cliente de control contra `192.168.0.236`.
- **No se accionó PTT** por ningún camino (socket, CAT, wfview, apps, físico).
- **No se cambió** frecuencia, modo, potencia ni ATU de la radio.
- **No se desplegó, flasheó ni editó código** del repo. El script de §6 **no se ejecutó**.
- El único acceso a la radio en la fecha es el **recon read-only** de otra tarea
  (`recon-radio.txt`, §0: *"Solo lectura. No se transmitió, no se escribio nada en la radio, no se
  reinicio nada."*).
- Si en el futuro se corre el protocolo **sin** `AUTHORIZED=1`, el script de §6.3 escribe
  automáticamente una constancia `f0-NO-TX-<fecha>.md` en esta carpeta y sale con código 2 (§6.6).

---

## 1. Camino de TX en el port (de la orden al RF)

Cadena verificada (sólo los puntos relevantes para banda/frecuencia/PTT):

```
orden de usuario / control remoto / app
        │
        ├─ socket JSON {"cmd":"freq"} ──► main_screen_set_freq ─┐        (src/remote_server.cpp:417-426)
        ├─ CAT C_SET_FREQ 0x05 ─────────────────────────────────┤        (src/cat.cpp:462-469)
        └─ apps: radio_set_freq() (API C) ──────────────────────┤        (src/radio.c:622-630)
                                                                ▼
                                     cfg_cur.fg_freq  (Subject)          (src/cfg/band.c:74-75)
                                                                │
                                          on_fg_freq_change ────┤  TABLA DE BANDAS (get_band_info_by_freq)
                                                                ▼  (src/cfg/band.c:599-606, :135-160, :234-275)
                                  cfg_band.vfo_a/b.freq (Subject por VFO)
                                                                │
                                          on_vfo_freq_change ───┤  (src/radio.c:364-370)
                                                                ▼
                                  x6200_control_vfo_freq_set(vfo, hz)  (lib src/control.c:26-45)
                                                                │
                                          x6200_control_cmd() ──┤  I2C /dev/i2c-0, addr 0x72
                                                                ▼  (lib src/low/control.c:36-37, :50, :70-95, :445-457)
                                   BASE / STM32 / FPGA  ◄──────────  registro x6200_vfoa_freq / x6200_vfob_freq
                                                                │
                                       (PTT)  radio_set_ptt ────┤  bit x6200_iptt 0x40000 en x6200_sple_atue_trx
                                                                ▼  (src/radio.c:907-909; lib src/control.c:156-166)
                                   BASE / MCU  ──► PA ──► antena / carga
```

Lo que hay que retener: **la única validación de Linux es de rango (0,5–55 MHz) y de tabla de
bandas para sintonizar; ninguna es un interlock de TX.** El bit de PTT se manda al BASE sin
consultar banda, frecuencia, modo ni potencia.

### 1.1 Tabla de gates (a)

| # | Etapa | Evidencia `archivo:línea` | ¿Valida banda/frecuencia? | ¿Bloquea TX? | Estado |
|---|---|---|---|---|---|
| G1 | Socket remoto: `freq`/`tune` → `main_screen_set_freq()` | `src/remote_server.cpp:417-426`, `:886-890`; `src/main_screen.c:1331-1334` | **sí**, contra `radio_check_freq` (0,5–55 MHz) y contra la tabla de bandas | no | código |
| G2 | `cfg_cur.fg_freq` → `on_fg_freq_change` → `cfg_band_set_freq_for_vfo()` | `src/cfg/band.c:74-75`, `:599-606`, `:135-160`, `:234-275`; SQL preparado `:559-571` | **sí**: si la frecuencia no cae en ninguna fila de `bands`, se loguea `Unknown band, can't set freq` y **no llega al hardware** | no | código |
| G3 | Observer de frecuencia de VFO → `on_vfo_freq_change` → `x6200_control_vfo_freq_set()` | `src/radio.c:364-370`, `:505-506`; lib `src/control.c:26-45` | **no** | no | código |
| G4 | `radio_set_freq()` (API que usan las apps) | `src/radio.c:622-630` | sólo `radio_check_freq()` | no | código |
| G5 | `radio_check_freq()` | `src/radio.c:632-637` | **rango 500.000–55.000.000 Hz** (o transverter) | no | código |
| G6 | `x6200_control_set_band()` de la lib | lib `src/control.c:26-45` (llamada **comentada**, línea 30); `src/low/control.c:534-546` (`band_index`, comando `x6200_vi_vm` **comentado**, línea 542) | **código muerto**: no se llama; y aunque se llamara, sólo manda un índice de banda al BASE | no | código |
| G7 | Transporte al BASE: `x6200_control_cmd()` | lib `src/low/control.c:445-457`; `:36-37` (fd/addr), `:50` (`/dev/i2c-0`), `:70-95` (`ioctl I2C_RDWR`) | no | no | código |
| G8 | `radio_set_ptt()` | `src/radio.c:907-909` | **no** (no consulta banda, frecuencia, modo ni estado) | no | código |
| G9 | `x6200_control_ptt_set()` | lib `src/control.c:156-166`; bit `x6200_iptt = 0x40000` en `include/.../low/control.h:110-121` | no | no | código |
| G10 | Observer `cfg.pwr` → `x6200_control_txpwr_set()` | `src/radio.c:531`; lib `src/control.c:278-283` | no hay tope por banda (en esta rama **no existe** `cb_pwr_max` ni interlock; `grep` en `src/` = 0) | no | código |
| G11 | Server TX gate: comandos no-stop mientras `state != RADIO_RX` | `src/remote_server.cpp:389-400` | no | **sí, temporal**: mientras transmite el server sólo acepta stop/close | código |
| G12 | `radio_start_swrscan()` / barrido de ROE | `src/radio.c:718-734`; `src/dialog_swrscan.c:876-903`; ventana legal `src/swr_session.h:61-64`, `src/swr_session.c:66-70` | **sí**: la grilla completa debe caer en 0,5–55 MHz y sólo arranca desde `RADIO_RX` | **sí, inicia TX** (0,1 W, LSB) | código |
| G13 | PTT físico (trigger superior `KEYPAD_PTT`) | `src/keypad.c:117-119`; `src/main_screen.c:902-925` | no | **inicia TX** (vía `radio_set_ptt`) | código |
| G14 | CAT serial / wfview LAN (`C_CTL_PTT 0x1C`) | `src/cat.cpp:778-795`, `:999`; `src/wfview_server.c:334-340`, `:516-546` | **no** | **inicia TX** (vía `radio_set_ptt`) | código |
| G15 | Gate `fullband-tx` del firmware de fábrica (`/etc/xgradio/xgradio.conf`, `[mods]`) | **medido** en el firmware oficial V1.0.6: archivo **línea 7** `fullband-tx=disable`, leído por `XBandPlan::XBandPlan()` de la UI Qt `/usr/app_qt/x6200_ui_v100` (`ref-fabrica.md` §3-§5; artefacto `ref-fabrica/xgradio.conf`) | n/a (la UI oficial compara contra `isHamBand(freq)` para ATU/indicador) | **en el firmware oficial, no en esta ROM** | **medido: ausente en esta ROM** (radio: `recon-radio.txt` §1/§2/§7 → `/etc/xgradio/` no existe y el barrido no encuentra `xgradio`/`fullband`). El gate histórico era **UI-side**, no MCU (`ref-fabrica.md` §5) |
| G16 | **BASE / MCU / FPGA (STM32)** | no hay fuente en este repo: firmware aparte | **desconocido** | **posible único gate real** | **no medido / `requires-radio`** → lo mide F0 |

### 1.2 Lo que **no** es un gate (para no confundirse)

- **`band_plan` / plan IARU**: es sólo dibujo en el waterfall; `src/radio.c` no lo consulta. En
  27 MHz no dibuja nada (el plan de aficionado arranca en 28 MHz).
- **Locks de la GUI** (`main_screen_freq_locked()`, `main_screen_mode_locked()`,
  `main_screen_band_locked()` → `false`): son candados de *app abierta*, no de banda
  (`src/main_screen.c:1624-1635`, `src/remote_server.cpp:886-890`). Con la pantalla principal
  libre, no interfieren.
- **`fullband-tx`**: no está en la ROM del port. No hay nada que "destrabar" en el rootfs
  (medido: `recon-radio.txt` §1/§2/§7; en este repo `grep -ral fullband` sólo encuentra el propio
  transcript del recon).
- **`x6200_control_set_band()`**: comentado y no invocado; el índice de banda que calcula no es un
  límite de TX.

### 1.3 El único gate que queda: baseband/MCU (por qué F0 es la prueba)

La librería manda por I2C el registro de frecuencia y el bit de PTT; el **BASE (STM32/FPGA)** es
quien decide si sintetiza y excita el PA. Desde Linux no hay forma de saber si el BASE rechaza
27 MHz. Dos pistas de código que **no** alcanzan para afirmarlo (quedan como `requires-radio`):

1. El registro **`x6200_vfoa_ham_band` / `x6200_vfob_ham_band`** (comandos 0 y 6,
   `include/.../low/control.h:19,26`) se inicializa en **0** y **nunca se vuelve a escribir**
   (`src/low/control.c:398`, `:403`; `grep -rn ham_band` en todo el árbol = sólo esas 3
   apariciones). Si el MCU usara ese registro para limitar el TX por "banda de aficionado", sería
   un candidato de gate **del lado Linux**… pero no hay evidencia de que lo use; por eso F0.
2. El firmware oficial tenía el gate **en la UI Qt**, no en el MCU: `/etc/xgradio/xgradio.conf`
   línea 7 (`fullband-tx=disable`), leído por `XBandPlan::XBandPlan()` de
   `/usr/app_qt/x6200_ui_v100`, y **la UI le baja al baseband el flag y el permiso de TX por banda**
   por su canal I2C propio (`BaseCommIIC`; ver `ref-fabrica.md` §4.4-§5, medido sobre la imagen de
   fábrica V1.0.6). Esto **confirma** que el gate histórico era UI-side y que el MCU/PA nunca fue
   el que leía `xgradio.conf`.
3. **Matiz honesto:** que el gate oficial fuera UI-side **no prueba** que el MCU acepte TX fuera de
   banda sin que la UI se lo habilite. De hecho, en el firmware oficial la UI **sí** le manda al
   baseband un permiso de TX por banda (bytes `+0x30/+0x31` de cada ítem de banda) y el flag
   `fullband-tx` (`ref-fabrica.md` §4.4). El protocolo del port (`aether-x6200-control`) **no
   replica ese permiso por banda**: lo único parecido que manda es `ham_band = 0` (punto 1). Si el
   MCU usa ese dato para gatear, F0 lo va a mostrar. Como los dos protocolos (BaseCommIIC del fw
   oficial vs. registros I2C del aether-control) no se pueden mapear sin el firmware del MCU
   (opaco: `ref-fabrica.md` §5), esto queda **`requires-radio`**, no concluido.

---

## 2. Librería de control (`aether-x6200-control`)

Ruta del fuente vendorizado (no versionado en este repo):
`/home/pablo/desa/x6200/AetherX6200Buildroot/buildroot/dl/aether-x6200-control/git/`.
El port la enlaza como `aether_x6200_control` (`src/CMakeLists.txt:69`). El repo `x6200_qa`
sólo tiene copias parciales de headers para el arnés de UI
(`tools/ui_harness/third_party/aether_radio/x6200_control/`), no el `.c`.

| Función | Archivo:línea | ¿Valida frecuencia/banda? | Qué hace |
|---|---|---|---|
| `x6200_control_vfo_mode_set()` | `src/control.c:20-24` | no | `x6200_control_cmd(vfoa_mode/vfob_mode, mode)` |
| `x6200_control_vfo_freq_set()` | `src/control.c:26-45` | **no** | La llamada a `x6200_control_set_band(freq)` está **comentada** (línea 30). Manda `x6200_vfoa_freq`/`x6200_vfob_freq` con la frecuencia cruda. |
| `x6200_control_ptt_set()` | `src/control.c:156-166` | **no** | Lee el registro `x6200_sple_atue_trx`, prende/apaga el bit `x6200_iptt` (0x40000) y lo escribe. Sin chequeos. |
| `x6200_control_atu_set()` | `src/control.c:216-230` | no | Bit `x6200_atue` (0x01000) del mismo registro. |
| `x6200_control_swrscan_set()` | `src/control.c:244-255` | no | Bit `x6200_swrscan_trx` (0x00010). |
| `x6200_control_txpwr_set()` | `src/control.c:278-283` | **no** | `p = pwr*10`; escribe `x6200_rfg_txpwr` = `prev | (p<<8)`. Rango de hecho 0,0–25,5 W (byte). |
| `x6200_control_set_band()` | `src/low/control.c:534-546` | (muerta) | Calcula `band_index(freq)` (0..22) y, si cambia, setea `cur_band`; **el `x6200_control_cmd(x6200_vi_vm, ...)` está comentado** (línea 542). Devuelve `true`/`false` pero nadie lo llama. |
| `x6200_control_cmd()` | `src/low/control.c:445-457` | no | `addr = cmd*4`; arma `{addr, arg}` y `send_regs()`. |
| `x6200_control_get()` | `src/low/control.c:459-462` | no | Devuelve el cache local `all_cmd.arg[cmd]` (no lee el BASE). |
| `x6200_control_idle()` | `src/low/control.c` (fin) | no | Reenvía **todos** los registros (`all_cmd`) cada tanto; el GUI la llama en reposo (`src/radio.c:320-321`, `IDLE_TIMEOUT` = 1000 ms, `src/radio.c:39`). |

**Transporte (cómo llega la orden al MCU/FPGA):** I2C, no SPI ni `write` a `/dev`:

- `open("/dev/i2c-0", O_RDWR)` y dirección I2C **`0x72`** (`src/low/control.c:36-37`, `:50`).
- `send_regs()` → `struct i2c_msg` + `ioctl(i2c_fd, I2C_RDWR, ...)` (`src/low/control.c:70-95`);
  `get_regs()` para leer (`:106-124`).
- Comando = `{uint16_t addr; uint32_t arg;}` con `addr = cmd*4`, bytes big-endian
  (`src/low/control.c:25-28`, `:445-457`).
- Registros con nombre en `include/.../low/control.h:17-70`; bits del registro de TRX en
  `:108-125` (`x6200_sple`, `x6200_swrscan_trx`, `x6200_atue`, `x6200_atu_tune`,
  `x6200_modem_trx`, `x6200_calibration`, `x6200_power_off`, **`x6200_iptt = 0x40000`**).
- `x6200_control_init()` hace un handshake con el BASE (`get_regs(0x2000)`), lee versión/calibración
  y le manda un bloque inicial (`all_cmd`) donde **`ham_band = 0`** y `rfg_txpwr = (10<<8)|64`
  (`src/low/control.c:129-160`, `:390-420`).

**Conclusión §2:** la librería **no valida frecuencia ni banda en ninguna función del camino de
TX** (frecuencia, modo, PTT, potencia). La orden llega al BASE por I2C como valor crudo. Todo lo
que pueda limitar el TX en 27 MHz vive en el BASE/MCU o no existe.

---

## 3. Control remoto: qué se puede hacer y con qué JSON exacto (b)

Hay **tres** interfaces de control distintas, y confundirlas es el error más fácil de cometer:

| Interfaz | Transporte | ¿PTT? | ¿Frecuencia? | ¿Modo? | ¿Potencia? |
|---|---|---|---|---|---|
| **Socket JSON** de la GUI | UNIX socket `/tmp/x6200_remote.sock` (lo usa la página `/remote` del webserver y la app del teléfono) | **NO** | sí (con tabla de bandas) | sí (con bug de NFM) | sí (décimas de W) |
| **CAT** | serial `/dev/ttyS2` (protocolo Icom CI-V) | **sí** `C_CTL_PTT 0x1C` | sí (pasa por `cfg_cur.fg_freq` → tabla de bandas) | sí (`C_SET_MODE 0x06`) | sí (`C_CTL_LVL 0x14`, sub 0x0A) |
| **wfview LAN** | UDP 50001 (control), 50002 (CI-V), 50003 (audio) | **sí** (las tramas CI-V se enrutan al mismo parser) | sí (CI-V) | sí (CI-V) | sí (CI-V) |

### 3.1 Socket JSON (`src/remote_server.cpp`)

El protocolo está documentado en el encabezado del archivo (`src/remote_server.cpp:1-23`). Formato:
**una línea = un objeto JSON**, respuestas `{"t":"ok"}` o `{"t":"err","err":"..."}` (y líneas
`{"t":"state",...}` para `get`/`sub`). Cliente de referencia: `tools/remote/x6200-remote.c`
(estático, se ejecuta en la radio; el QA ya lo usa).

**Comandos disponibles (tabla exacta, `cmd` + `val`/`text`):**

| Pedido JSON | Efecto | Límites/validación |
|---|---|---|
| `{"cmd":"get"}` | una línea de estado | — |
| `{"cmd":"sub"}` | estado 5/s + espectro 10/s + eventos + texto | hasta 3 suscriptores |
| `{"cmd":"freq","val":27185000}` | fija frecuencia (Hz) | `HF_MIN_FREQ`=100000..500000000 (`SettingsManager::HF_MIN_FREQ`, `src/cfg/settings_manager.h:109`), luego `radio_check_freq` 0,5–55 MHz **y tabla de bandas**; si no cae en una banda, no se sintoniza (§1 G2) |
| `{"cmd":"tune","val":-100}` | paso relativo (Hz) | ±10000, no 0 |
| `{"cmd":"mode","val":"USB"}` | modo por nombre | sólo `LSB, LSB-D, USB, USB-D, CW, CW-R, AM, NFM` (`src/remote_server.cpp:136`); **bug de NFM, §3.1.1** |
| `{"cmd":"band","val":40}` | elige banda por nombre/número | 1..999, `main_screen_band_locked()` → siempre `false` |
| `{"cmd":"pre","val":1}` / `att` / `nb` / `nr` | on/off | 0..1 |
| `{"cmd":"agc"}` | siguiente AGC | — |
| `{"cmd":"rfgain","val":50}` | ganancia de RF | 0..100 |
| `{"cmd":"filter","val":2400}` | ancho de filtro (Hz) | 50..6000 |
| `{"cmd":"pwr","val":40}` | **potencia de TX en décimas de W** (40 = 4,0 W) | 1..100 |
| `{"cmd":"nb_level","val":50}` / `nb_width` | 0..100 | |
| `{"cmd":"nr_level","val":30}` | 0..60 | |
| `{"cmd":"agc_mode","val":3}` | 0..3 (OFF SLOW FAST AUTO) | |
| `{"cmd":"sql","val":50}` / `vol` | 0..100 | |
| `{"cmd":"location","text":"-34.6,-58.4"}` | posición del teléfono | lat/lon válidas |
| `{"cmd":"app","text":"SWRScan"}` | abre/cierra apps por nombre (`"off"` cierra) | `app_names[]` (`:139-150`) |
| `{"cmd":"page","text":"gen|app|key|dfn|dfl"}` | grupo de botones de abajo | |
| `{"cmd":"btn","val":0..4}` | aprieta botón 0..4 de la fila | 0..4 |
| `{"cmd":"qso_log","text":"..."}` / `cluster_feed` | Field Ops / cluster | |
| `{"cmd":"app_base", ...}` | base de app digital | requiere app abierta |
| `{"cmd":"ft8_cq","val":1}` / `ft8_stop` / `ft8_auto` / `ft8_hold` / `ft8_txfreq` (200..2900) / `ft8_reply` | TX RX FT8 | app FT abierta; **transmite** |
| `{"cmd":"cw_send","text":"..."}` / `cw_stop` / `cw_wpm` (5..60) / `cw_dec` | CW | app CW abierta; **transmite** |
| `{"cmd":"js8_send","text":"..."}` / `js8_cq` / `js8_hb` / `js8_stop` | JS8 | app JS8 abierta; **transmite** |
| `{"cmd":"wspr_txpct","val":10}` / `wspr_txnext` | WSPR | 0/10/20/33/50; **transmite** |
| `{"cmd":"sstv_tx","val":"R36","text":"archivo.png"}` / `sstv_stop` | SSTV | app SSTV; **transmite** |
| `{"cmd":"wefax_mode"}` / `wefax_tx` / `wefax_stop` | WeFax | app WeFax; **transmite** |
| `{"cmd":"swrscan","val":1}` / `{"cmd":"swrscan","val":0}` | Run/Stop del barrido de ROE | app **SWRScan** abierta; **transmite a 0,1 W**; §3.1.2 |
| `{"cmd":"field_screen"}` / `field_start` / `field_end` / `field_log` / ... | Field Ops | |

**Formato exacto de la línea de estado** (`src/remote_server.cpp:594-631`):

```json
{"t":"state","vfo":"A","freq":27185000,"freq_bg":14074000,"mode":"AM","band":"CB C",
 "filter":4500,"flo":0,"fhi":4500,"rfgain":100,"sql":0,"vol":30,"pre":false,"att":false,
 "agc":"AUTO","nb":false,"nr":false,"nb_level":50,"nb_width":50,"nr_level":30,
 "pwr":40,"db":-90,"tx":false,"app":"","ax":{},"cwt":null,"logrev":0,"did":"..."}
```

Campos útiles para F0: `freq`, `mode`, `band`, **`pwr` (setpoint en décimas de W, no la potencia
medida)**, `db` (S-meter de RX), **`tx`** (bool; `radio_get_state() != RADIO_RX`), `ax` (estado de
la app abierta). **No hay campo de potencia de TX ni de ROE** (ver §5).

Guardas del servidor:
- Comandos aplicados en el hilo de LVGL por el scheduler; mientras la radio **transmite**
  (`state != RADIO_RX`) sólo pasan los comandos "stop/close" (`src/remote_server.cpp:389-400`).
- `freq`/`tune` se rechazan si `main_screen_freq_locked()` (`src/remote_server.cpp:886-890`).
- `mode` se rechaza si `main_screen_mode_locked()` (`:870-878`).

#### 3.1.1 Ojo: `{"cmd":"mode","val":"NFM"}` **NO** selecciona FM (bug de mapeo)

- `mode_names[]` tiene **8** entradas: `{"LSB","LSB-D","USB","USB-D","CW","CW-R","AM","NFM"}`
  (`src/remote_server.cpp:136`) y `parse_cmd` asigna `cmd->val = i` (índice), `:870-878`.
- El enum real del port es `x6200_mode_sam = 7`, **`x6200_mode_nfm = 8`**
  (`include/.../low/control.h:88-100`; el port mapea `x6100_mode_*` → `x6200_mode_*` en
  `src/cfg/cfg_api.h:66-76`).
- Por lo tanto `"NFM"` (índice 7) escribe **modo 7 = SAM**. El equipo queda en SAM, no en FM.
- Además la línea de estado imprime `mode_names[mode]` con `mode <= x6100_mode_nfm` (= 8)
  (`src/remote_server.cpp:623`), así que con modo 8 (NFM real, puesto desde la radio o CAT) se lee
  **fuera de rango** del array: puede imprimir `"OFF"`, basura o cortar; **no confiar en el campo
  `mode` cuando el equipo está en NFM**.
- **Consecuencia para F0:** el paso de FM se hace **desde la pantalla de la radio o con un cliente
  CAT/wfview** (que mapea `M_NFM 0x05` → 8 correctamente, `src/cat.cpp:292-312`); **nunca** con
  `{"cmd":"mode","val":"NFM"}`. Esto es `código` (no medido en radio). El issue #146 tiene
  `remote_server.cpp` fuera de alcance, así que no se arregla en esta fase: se documenta.

#### 3.1.2 Comando `swrscan` (issue #133) y sus guardas

Fila en `app_cmds[]`: `{"swrscan","SWRScan",dialog_swrscan_remote,0,1,2}`
(`src/remote_server.cpp:191-197`). `stop = 2` a propósito: Run (1) y Stop (0) se aceptan **también
mientras transmite** (si no, un segundo Run sería rechazado por el gate de TX).

Guardas en `dialog_swrscan_remote_run()` (`src/dialog_swrscan.c:876-903`):
1. Sólo con la ventana SWRScan abierta (`dialog_type_is_run`).
2. Idempotencia: Run con barrido en curso / Stop quieto = no-op.
3. Arrancar exige `radio_get_state() == RADIO_RX`; Stop no.
4. Gate del diálogo intacto: multipasada 3/5 deshabilitada (`SWRSCAN_MULTIPASS_TX_ENABLED = 0`,
   `src/dialog_swrscan.h:62-63`) y **grilla fuera de la ventana legal de TX → `Error` sin TX**
   (0,5–55 MHz: `src/swr_session.h:61-64`; `src/swr_session.c:66-70`).

`radio_start_swrscan()` fuerza **LSB** y **0,1 W** (`SWRSCAN_TX_POWER_W`, `src/radio.c:718-734`;
`src/dialog_swrscan.h:72-75`) y restaura modo/potencia al parar (`src/radio.c:736-753`).
Estado por `get`: `ax = {"swrscan_state":"Idle|Scanning|Completed|Stopped|Error","passes":1,
"pass":0,"points":51,"swrscan_tx":false}` (`src/dialog_swrscan.c:914-924`).

### 3.2 CAT serial (`src/cat.cpp`)

- El GUI abre `/dev/ttyS2` como servidor CI-V (Icom) (`src/cat.cpp:999`).
- **PTT**: `C_CTL_PTT 0x1C` (`src/cat.cpp:84`). Con `data_size == 1` devuelve estado; con
  `data_size >= 2`, `data[1] == 0` → `radio_set_ptt(false)`; `data[1] == 1` → `radio_set_ptt(true)`
  (`:778-795`). **Sin chequeo de banda/frecuencia.** Trama CI-V (FE FE dst src cmd datos FD):
  - PTT ON: `FE FE <dst> <src> 1C 00 01 FD`
  - PTT OFF: `FE FE <dst> <src> 1C 00 00 FD`
  - (El parser no filtra direcciones; `LOCAL_ADDRESS 0xA4`, `src/cat.cpp:54`, `:1155-1170`.)
- **Frecuencia**: `C_SET_FREQ 0x05` → `subject_set_int(cfg_cur.fg_freq, ...)` (`:462-469`), o sea
  **pasa por la tabla de bandas** (misma G2).
- **Modo**: `C_SET_MODE 0x06` → `cfg_cur.mode` (`:471-477`), mapeo correcto `M_AM 0x02` → AM,
  `M_NFM 0x05` → NFM (`:292-312`).
- **Potencia**: `C_CTL_LVL 0x14`, subcomando `0x0A` (PWR), 3 bytes BCD
  `valor = W*255/10` (`:595-603`).
- **Telemetría del detector**: `C_RD_SQSM 0x15`, subcomandos `0x11` (power), `0x12` (SWR),
  `0x13` (ALC) (`:618-665`). Son **valores convertidos a escala de medidor**, no unidades
  calibradas (§5).

### 3.3 wfview (Icom LAN) — el PTT "de red" que sí existe

- Servidor Icom LAN en la GUI: control UDP **50001**, CI-V UDP **50002**, audio UDP **50003**
  (`src/wfview_server.c:9-12`, `:516-524`).
- Las tramas CI-V recibidas en 50002 se pasan a `cat_process_frame()` → mismo `process_req()` →
  `C_CTL_PTT` → `radio_set_ptt()` (`src/wfview_server.c:334-340`; `cat_set_net_sink(send_civ)`
  `:546`).
- Se habilita en **Ajustes → "wfview server"** (escribe `/mnt/wfview.conf`;
  `src/dialog_settings.c:960-993`); al arrancar, la GUI llama `wfview_server_init()`, que es
  **no-op si `enabled=0`** (`src/main.c:615-616`). Credenciales por defecto en el archivo:
  `user=x6200`, `pass=x6200` (`src/wfview_server.c:452-469`).
- **Uso para F0:** con la radio en la LAN y el servidor habilitado, un cliente **wfview** de PC se
  conecta y desde su terminal CI-V se mandan `1C 00 01` / `1C 00 00` (PTT on/off). También permite
  fijar frecuencia/modo/potencia. **Es la única forma de accionar PTT de forma realmente remota
  sin tocar el equipo**, pero exige habilitar el servidor (cambio de configuración) y un cliente
  en la LAN.

### 3.4 Apps como caminos de TX (lo que el socket sí puede transmitir)

El socket no tiene PTT, pero los comandos de app **sí** producen TX, cada uno en el modo que la app
fuerza (`src/remote_server.cpp:156-203`):

- **FT8**: `ft8_cq`, `ft8_reply`, etc. → TX USB-D de ~12,6 s (app FT). Es la forma más "larga y
  controlada" de emitir sin PTT físico.
- **CW**: `cw_send` → `radio_set_ptt()` + morse key (app CW), en CW/CWR.
- **JS8 / WSPR / SSTV / WeFax**: ídem, en sus modos (USB-D / USB).
- **SWRScan**: `swrscan 1` → 0,1 W en LSB (§3.1.2).

**Ninguno permite AM/FM.** Para F0 (AM y FM) el PTT tiene que ser físico, CAT o wfview.

### 3.5 Respuesta compacta a "¿se puede keyear PTT por control remoto?"

| Camino | ¿Se puede? | Comando exacto |
|---|---|---|
| Socket JSON (`/tmp/x6200_remote.sock`) | **NO** | no existe; `src/remote_server.cpp:20-22` |
| CAT serial `/dev/ttyS2` | **SÍ** | CI-V `FE FE <dst> <src> 1C 00 01 FD` (on) / `... 1C 00 00 FD` (off) |
| wfview LAN (UDP 50002) | **SÍ** (habilitando servidor) | misma trama CI-V `1C 00 01` / `1C 00 00` |
| Apps (FT8/CW/JS8/WSPR/SSTV/WeFax) | **SÍ** (modos de la app) | p.ej. `{"cmd":"ft8_cq","val":1}`, `{"cmd":"cw_send","text":"..."}` |
| Barrido de ROE | **SÍ** (0,1 W, LSB) | `{"cmd":"app","text":"SWRScan"}` + `{"cmd":"swrscan","val":1}` |
| PTT físico (trigger superior) | **SÍ** (local) | `KEYPAD_PTT` (evento de input) |
| PTT del micrófono | **¿?** | lo maneja el MCU, fuera de Linux: `requires-radio` |

---

## 4. Telemetría: qué reporta el equipo (y por qué no alcanza)

### 4.1 Lo que se puede leer por el socket JSON

Sólo lo de la línea de estado (§3.1): `freq`, `mode`, `band`, `pwr` (setpoint), `db` (S-meter de
RX), `tx`, `ax`. **No hay potencia directa, reflejada ni ROE.** El campo `pwr` es el **valor
pedido**, no el medido.

### 4.2 Lo que el equipo mide internamente (pero no expone por el socket)

El paquete de flujo del BASE trae **potencia de TX, ROE y ALC** (bytes, ×0,1):
`x6200_flow_t` en `include/.../low/flow.h:69-89` (`tx_power`, `vswr`, `alc_level`; flag `tx` en
`:43-67`). El GUI los usa así:

- En TX: `tx_info_update(pack->tx_power * 0.1f, pack->vswr * 0.1f, pack->alc_level * 0.1f)`
  (`src/radio.c:241-246` y `:273`).
- En barrido de ROE: `dialog_swrscan_update(pack->vswr * 0.1f)` (`src/radio.c:278`).
- El widget **TX Info** los muestra en pantalla (PWR/ROE/ALC) (`src/tx_info.c:255-289`).

**No hay log de esos valores** (grep de `vswr`/`tx_power`/`alc_level` en `LV_LOG`/`printf` =
0) y **no están en el estado del socket**. Para registrarlos en F0 hay que **fotografiar la
pantalla de TX Info** (o leerlos por CAT, §4.3). Son el **detector del propio equipo**, no un
instrumento.

### 4.3 Lo que se puede leer por CAT

`C_RD_SQSM 0x15` con subcomando `0x11` (power), `0x12` (SWR), `0x13` (ALC) (`src/cat.cpp:618-665`)
devuelve valores **convertidos a escala de medidor Icom** (`-pwr²+35·pwr`,
`-21·swr²+134·swr-122`, `alc*120/10`), a partir de los mismos bytes del detector interno. Sirve
para **dejar constancia** de lo que el equipo cree que está haciendo, no para medir.

### 4.4 Aclaración obligatoria

> **La telemetría del equipo NO es un instrumento calibrado.** El detector interno del X6200 no
> tiene trazabilidad, su calibración es de fábrica y no se conoce su incertidumbre. Sirve como
> indicador de funcionamiento (¿hay RF?, ¿el TX se corta?), **nunca** como medición de potencia ni
> de armónicos. Un F0 con sólo telemetría del equipo es **BLOCKED**, no PASS (§7).

---

## 5. Requisitos de una F0 válida (instrumentos y condiciones)

F0 se ejecuta con **carga fantasma** y **sin antena**, para no radiar. Cada requisito y por qué:

| # | Requisito | Por qué (qué invalida si falta) |
|---|---|---|
| R1 | **Carga fantasma 50 Ω** (resistencia de terminación de RF, ≥10 W, HF) | Define la impedancia de carga, evita radiación y hace repetible la potencia/ROE. Una antena o una resistencia improvisada cambia el resultado y puede radiar. |
| R2 | **Medidor de potencia/RF calibrado** (HF, 50 Ω, capaz de leer 0–10 W; con fecha de calibración) | Es la **única** forma de saber la potencia real. La telemetría del equipo no vale. Sin medidor no hay PASS de potencia. |
| R3 | **Analizador de espectro** que cubra al menos **hasta ~85 MHz** (2º armónico 54,370 MHz y 3º 81,555 MHz; mejor hasta 200 MHz) | Es la **única** forma de medir armónicos. Sin analizador no hay PASS de armónicos, y el issue prohíbe declarar TX validado sin medirlos. |
| R4 | **Atenuador de potencia** (p.ej. 30–40 dB, ≥10 W) entre la carga/tap y el analizador | Protege la entrada del analizador (típicamente ≤+20 dBm): 4 W = +36 dBm; sin atenuador se daña el instrumento. Además evita compresión del front-end. |
| R5 | **ATU apagado** (verificado en pantalla: `cfg.atu_enabled = false`) | Con el ATU en el camino, la red de adaptación mete pérdidas y desintoniza; la potencia/ROE medidas no son las del equipo y los armónicos se alteran. Además el ATU puede intentar re-sintonizar. |
| R6 | **XPA125B desconectada** (sin amplificador externo) | El PA externo amplifica y agrega sus propios armónicos: la medición dejaría de ser del X6200. |
| R7 | **Autorización explícita del owner** (+ licencia/habilitación que corresponda) | Sin autorización no se transmite. Ver §5.1 (legal). |
| R8 | **Receptor/analizador de modulación** (segundo receptor con antena de "sniffing" cerca de la carga, o SDR; idealmente medidor de desviación FM ±2,5 kHz) | Para verificar que AM/FM **modulan** (el issue pide "modulación audible en otro receptor" y FM ±2,5 kHz). Sin esto, la modulación queda `no medido`. Se hace por campo cercano, **nunca** conectando antena a la radio. |
| R9 | **Fuente/estado de alimentación registrado** (13,8 V externa vs batería) | La potencia depende de la tensión (8 W con 13,8 V / 5 W con batería, issue §3.4). Sin registrar la fuente, el número de potencia no es comparable. |
| R10 | **Bursts cortos y controlados** (≤5 s, con el operador con la mano en el PTT, carga que soporte la potencia) | Minimiza exposición térmica y permite abortar. La carga fantasma debe soportar la potencia y el ciclo de trabajo. |
| R11 | **Registro**: build/md5 de la GUI, fecha, instrumentos (marca/modelo/serie/calibración), lecturas, fotos | Sin trazabilidad la corrida no es evidencia (convención de `docs/PRUEBAS.md`). |

### 5.1 Legal (Argentina) — obligatorio para el owner

Del issue #146 §7.3 (no inventar ni suavizar):

- El SBC argentino está en **26,960–27,410 MHz** y exige **autorización por estación** y **equipo
  homologado/registrado** (Res. SC 47/1982; CABFRA 2025 de ENACOM; alta con F1/F2/F23, D3/COPITEC
  y responsable técnico).
- **El X6200 no es un equipo CB homologado.** Transmitir en 27 MHz con él queda **fuera del
  régimen del SBC y fuera del servicio de aficionados**. Modificar el firmware no cambia la
  homologación.
- **26,515 MHz (grid B)** queda **fuera del SBC** (26,960–27,410). Por eso el protocolo la trata
  como paso **opcional** con una variable de autorización extra (`AUTHORIZED_GRID_B=1`); sin ella
  **no se transmite** y se registra `SKIPPED`.
- **No publicar TX como validado hasta completar F0 y medir armónicos.**

**El agente/QA no transmite.** F0 la ejecuta el owner (o alguien que él designe) con su
autorización y su responsabilidad.

---

## 6. Protocolo F0 (c) — listo para ejecutar cuando el owner autorice

> ### ⚠️ ESTE PROTOCOLO NO SE EJECUTÓ
> El script de §6.3 es una **plantilla**. No se corrió en esta sesión: no se abrió SSH, no se
> encendió el TX, no se desplegó nada. Ejecutarlo **sólo** con `AUTHORIZED=1` y los instrumentos
> conectados, con el owner presente.

### 6.1 Roles y precondiciones

- **Operador**: el owner o quien él designe, **físicamente frente a la radio**.
- **Equipo**: X6200 con la ROM del port, **conectada a la carga fantasma por RF** (sin antena).
- **Instrumentos** conectados según R1–R9, con las lecturas a la vista.
- **ATU apagado** y **XPA125B desconectada** (R5, R6).
- **Helper** `/tmp/x6200-remote` presente en la radio (cliente del socket, estático). Si no está,
  copiarlo es un **paso de despliegue aparte**, fuera de este protocolo (y hoy no autorizado).
- **Binario**: registrar `md5sum /usr/sbin/x6200_gui` antes de empezar (el build analizado es
  `9a094c421b0eb05c103abf782ea00db6`; si difiere, anotar el md5 real).
- **Frecuencias**: 27.185 MHz (canal 19, CEPT/Argentina, dentro del SBC) y —sólo con autorización
  extra— 26.515 MHz (grid B, fuera del SBC).
- **Modos**: AM y FM/NFM.
- **Potencia**: setpoint 4,0 W (`pwr=40`) en AM/FM; 1,0 W (`pwr=10`) en el paso de 26.515 por
  defecto. **En esta rama no hay tope CB** (`cb_pwr_max` no existe todavía), así que la potencia se
  fija explícitamente y aplica a todas las bandas: **verificarla antes de cada PTT**.

### 6.2 Variables del protocolo

| Variable | Obligatoria | Significado |
|---|---|---|
| `AUTHORIZED=1` | **SÍ** (sin ella se aborta y se deja constancia de NO TX) | el owner autorizó la TX de esta corrida |
| `DUMMY_LOAD=1` | sí | carga fantasma 50 Ω conectada |
| `POWER_METER=1` | sí | medidor de RF calibrado en la línea |
| `SPECTRUM_ANALYZER=1` | sí | analizador de espectro listo |
| `ATTENUATOR=1` | sí | atenuador de potencia en la entrada del analizador |
| `ATU_OFF=1` | sí | el operador verificó **ATU OFF** en pantalla |
| `NO_XPA=1` | sí | XPA125B desconectada |
| `AUTHORIZED_GRID_B=1` | no (default 0) | autoriza además TX en 26.515 (fuera del SBC) |
| `ATU_WAS_ON=1` | no (default 0) | el ATU estaba encendido antes: hay que restaurarlo al final |
| `PTT_MODE=physical\|wfview\|cat` | no (default physical) | cómo va a accionar el PTT el operador |
| `AM_DW`, `FM_DW` | no (default 40 = 4,0 W) | setpoint en décimas de watt |
| `GRIDB_DW` | no (default 10 = 1,0 W) | setpoint para 26.515 |
| `BURST_S`, `MAX_TX_S` | no (default 5 / 15 s) | duración objetivo y techo de seguridad del burst |

### 6.3 Plantilla de script (NO ejecutado)

Se corre **desde el host de QA**, contra la radio por SSH. No es necesario copiarlo a la radio;
sí requiere `/tmp/x6200-remote` en la radio y `sshpass`+`python3` en el host.

```bash
#!/usr/bin/env bash
# =============================================================================
#  F0 CB 27 MHz (issue #146) — PLANTILLA DE TX CONTROLADA
#  ⚠️ NO EJECUTADO en la sesión de análisis (2026-10-19). Documentación.
#  ⚠️ Ejecutar SOLO con autorización explícita del owner y con los instrumentos.
#  ⚠️ Sin AUTHORIZED=1 escribe la constancia de NO TX y sale con código 2.
# =============================================================================
set -u

# --- guardas (se evalúan ANTES de abrir SSH) --------------------------------
AUTHORIZED="${AUTHORIZED:-0}"
AUTHORIZED_GRID_B="${AUTHORIZED_GRID_B:-0}"
DUMMY_LOAD="${DUMMY_LOAD:-0}"
POWER_METER="${POWER_METER:-0}"
SPECTRUM_ANALYZER="${SPECTRUM_ANALYZER:-0}"
ATTENUATOR="${ATTENUATOR:-0}"
ATU_OFF="${ATU_OFF:-0}"
NO_XPA="${NO_XPA:-0}"
ATU_WAS_ON="${ATU_WAS_ON:-0}"
PTT_MODE="${PTT_MODE:-physical}"      # physical | wfview | cat
CC_FREQ="${CC_FREQ:-27185000}"        # 27.185 MHz
GRIDB_FREQ="${GRIDB_FREQ:-26515000}"  # 26.515 MHz (grid B, fuera del SBC)
AM_DW="${AM_DW:-40}"                  # 40 = 4.0 W
FM_DW="${FM_DW:-40}"
GRIDB_DW="${GRIDB_DW:-10}"            # 1.0 W
BURST_S="${BURST_S:-5}"
MAX_TX_S="${MAX_TX_S:-15}"
RADIO_IP="${RADIO_IP:-192.168.0.236}"
RADIO_USER="${RADIO_USER:-root}"
RADIO_PASS="${RADIO_PASS:-123}"
GUI_MD5_ESPERADO="${GUI_MD5_ESPERADO:-9a094c421b0eb05c103abf782ea00db6}"
SSH_OPTS="-F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
 -o ConnectTimeout=8 -o LogLevel=ERROR -o PreferredAuthentications=password \
 -o PubkeyAuthentication=no"

EVID="docs/evidencia/2026-10-19-f0-cb-tx"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG="$EVID/f0-run-$STAMP.log"
mkdir -p "$EVID"

say()  { printf '%s\n' "$*" | tee -a "$LOG"; }
sayq() { printf '%s\n' "$*" >> "$LOG"; }

# --- constancia de NO TX ----------------------------------------------------
rec_no_tx() {                    # $1 = guarda que abortó, $2 = qué se envió
    local reason="$1" sent="$2"
    local rec="$EVID/f0-NO-TX-$STAMP.md"
    cat > "$rec" <<EOF
# F0 CB 27 MHz (issue #146) — constancia de NO transmisión

- Fecha/hora: $(date -Iseconds)
- Operador: $(id -un)@$(hostname)
- Guarda que abortó: **$reason**
- Radio: $RADIO_IP
- Comandos enviados: $sent
- PTT accionado: **no** (ni físico, ni CAT, ni wfview, ni app, ni swrscan)
- Frecuencia/modo/potencia/ATU: sin cambios atribuibles a este script
- Instrumentos usados: **ninguno**
- Resultado: **F0 NO EJECUTADA — no se transmitió**

Firma (owner/QA): ______________________  Fecha: ____________
EOF
    say "ABORTADO (NO TX): $reason"
    say "Constancia: $rec"
    exit 2
}

NOHUBO="ninguno (no se abrió SSH)"
[ "$AUTHORIZED" = "1" ]      || rec_no_tx "AUTHORIZED != 1 (no autorizado)" "$NOHUBO"
[ "$DUMMY_LOAD" = "1" ]      || rec_no_tx "DUMMY_LOAD != 1 (sin carga fantasma)" "$NOHUBO"
[ "$POWER_METER" = "1" ]     || rec_no_tx "POWER_METER != 1 (sin medidor de RF)" "$NOHUBO"
[ "$SPECTRUM_ANALYZER" = "1" ] || rec_no_tx "SPECTRUM_ANALYZER != 1 (sin analizador)" "$NOHUBO"
[ "$ATTENUATOR" = "1" ]      || rec_no_tx "ATTENUATOR != 1 (sin atenuador)" "$NOHUBO"
[ "$ATU_OFF" = "1" ]         || rec_no_tx "ATU_OFF != 1 (ATU no apagado)" "$NOHUBO"
[ "$NO_XPA" = "1" ]          || rec_no_tx "NO_XPA != 1 (XPA125B conectada)" "$NOHUBO"
case "$PTT_MODE" in physical|wfview|cat) ;; *)
    rec_no_tx "PTT_MODE inválido: $PTT_MODE" "$NOHUBO" ;; esac
command -v sshpass >/dev/null || rec_no_tx "falta sshpass en el host" "$NOHUBO"
command -v python3 >/dev/null || rec_no_tx "falta python3 en el host" "$NOHUBO"

# --- helpers ----------------------------------------------------------------
ssh_radio() { sshpass -p "$RADIO_PASS" ssh $SSH_OPTS "$RADIO_USER@$RADIO_IP" "$1"; }
remote()    { ssh_radio "/tmp/x6200-remote '$1'"; }
state()     { remote '{"cmd":"get"}'; }
jf()        { python3 -c 'import json,sys
d=json.load(sys.stdin)
v=d.get(sys.argv[1],"")
if isinstance(v,bool): print("true" if v else "false")
elif v is None: print("")
else: print(v)' "$1" 2>/dev/null; }
req() {
    local r
    r=$(remote "$1") || { sayq "FALLO SSH: $1"; return 1; }
    sayq ">> $1"; sayq "<< $r"
    printf '%s\n' "$r" | grep -q '"t":"ok"'
}
set_freq() { req "{\"cmd\":\"freq\",\"val\":$1}"; }
set_pwr()  { req "{\"cmd\":\"pwr\",\"val\":$1}"; }
set_mode() { req "{\"cmd\":\"mode\",\"val\":\"$1\"}"; }
wait_field() { local i; for i in $(seq 1 "${3:-12}"); do
    [ "$(state | jf "$1")" = "$2" ] && return 0; sleep 0.5; done; return 1; }
sayq "== $STAMP F0 CB 27 MHz =="
sayq "PTT_MODE=$PTT_MODE AM_DW=$AM_DW FM_DW=$FM_DW GRIDB_DW=$GRIDB_DW GRID_B=$AUTHORIZED_GRID_B"

# --- preflight (SÓLO LECTURA) -----------------------------------------------
say "== Preflight =="
MD5=$(ssh_radio "md5sum /usr/sbin/x6200_gui 2>/dev/null" | awk '{print $1}')
say "GUI md5 en la radio: ${MD5:-?}  (build analizado: $GUI_MD5_ESPERADO)"
[ "$MD5" = "$GUI_MD5_ESPERADO" ] || say "AVISO: el binario NO es el del build analizado; registrar el md5 real."
BASE=$(state)
[ -n "$BASE" ] || rec_no_tx "la GUI no responde por /tmp/x6200_remote.sock" "get (sólo lectura)"
printf '%s\n' "$BASE" >> "$LOG"
BASE_FREQ=$(printf '%s' "$BASE" | jf freq)
BASE_MODE=$(printf '%s' "$BASE" | jf mode)
BASE_PWR=$(printf '%s'  "$BASE" | jf pwr)
BASE_BAND=$(printf '%s' "$BASE" | jf band)
[ "$(printf '%s' "$BASE" | jf tx)" = "false" ] || \
    rec_no_tx "el equipo ya estaba transmitiendo (tx=true)" "get (sólo lectura)"
say "Baseline: freq=$BASE_FREQ mode=$BASE_MODE pwr=$BASE_PWR band=$BASE_BAND"

ATU_WAS_ON_BAK="$ATU_WAS_ON"
TX_DONE=0
fail() {                     # $1=motivo
    if [ "$TX_DONE" -eq 0 ]; then
        rec_no_tx "$1" "get; intento de freq/modo/potencia (RX, sin PTT)"
    else
        say "ABORTO DESPUÉS DE TX: $1"; restore; exit 3
    fi
}
ensure_freq() {
    set_freq "$1" || fail "el socket rechazó la frecuencia $1"
    wait_field freq "$1" 12 || fail "la frecuencia $1 no quedó fijada (¿banda ausente en params.db?)"
    say "OK freq=$1 band=$(state | jf band)"
}
ensure_am() {
    [ "$(state | jf mode)" = "AM" ] || set_mode AM
    wait_field mode AM 8 || say "AVISO: no se pudo verificar AM por el socket"
}
ensure_pwr() {
    set_pwr "$1"
    wait_field pwr "$1" 8 || say "AVISO: el setpoint de potencia aún no coincide"
}
ask() { printf '  > %s ' "$1"; read -r v; printf '%s; %s=%s\n' "$STAMP" "$1" "$v" >> "$LOG"; printf '%s' "$v"; }

burst() {                    # $1=etiqueta  $2=segundos
    local t0 now t st seen=0 last=0
    say ""; say "--------------------------------------------------------------"
    say ">>> $1"
    say ">>> 1) Empezá a transmitir AHORA ($PTT_MODE)."
    say ">>> 2) Soltá a los ${2}s. Si el script avisa que el TX sigue encendido:"
    say ">>>    soltá; si no cesa, APAGÁ LA RADIO."
    TX_DONE=1
    t0=$(date +%s)
    while :; do
        now=$(date +%s); t=$((now - t0)); st=$(state)
        printf '%s; +%ss; %s\n' "$STAMP" "$t" "$st" >> "$LOG"
        case "$st" in *'"tx":true'*)
            [ $seen -eq 0 ] && say "  +${t}s TX ON"; seen=1; last=$now;; esac
        if [ $seen -eq 1 ] && [ $((now - last)) -ge 1 ]; then
            say "  +${t}s TX OFF"; break; fi
        if [ $seen -eq 1 ] && [ $t -ge "$MAX_TX_S" ]; then
            say "  !!! TX SIGUE ON (+${t}s): SOLTÁ EL PTT / APAGÁ LA RADIO"; fi
        [ $t -ge $(( ${2} + MAX_TX_S )) ] && { say "  ventana agotada"; break; }
        sleep 0.5
    done
    [ $seen -eq 0 ] && say "  OBSERVACIÓN: no se detectó 'tx:true' (anotarlo, no inventar)"
}

restore() {
    say ""; say "== Restauración =="
    set_freq "$BASE_FREQ"
    wait_field freq "$BASE_FREQ" 12 || say "AVISO: no se pudo restaurar la frecuencia"
    case " LSB LSB-D USB USB-D CW CW-R AM NFM " in
        *" $BASE_MODE "*) set_mode "$BASE_MODE"
                          wait_field mode "$BASE_MODE" 8 || say "AVISO: no se pudo restaurar el modo";;
        *) say "modo base '$BASE_MODE' no seteable por socket (SAM/WFM/NFM): restaurar a mano";;
    esac
    set_pwr "$BASE_PWR"
    wait_field pwr "$BASE_PWR" 8 || say "AVISO: no se pudo restaurar la potencia"
    F=$(state); printf '%s\n' "$F" >> "$LOG"
    [ "$(printf '%s' "$F" | jf tx)" = "false" ] || \
        say "!!! TX SIGUE ACTIVO: soltar PTT y APAGAR la radio"
    say "Estado final: $F"
    [ "$ATU_WAS_ON_BAK" = "1" ] && say "ATU: volver a ON a mano (no hay comando remoto de ATU)"
    say "Verificar en pantalla: frecuencia, modo, potencia, ATU y RX."
}
trap 'say "Interrumpido: soltá PTT"; restore; exit 130' INT TERM

# --- PASO 1: 27.185 MHz AM --------------------------------------------------
say ""; say "===== PASO 1: 27.185 MHz AM ====="
ensure_freq "$CC_FREQ"
say "banda reportada: $(state | jf band) (se espera CB C)"
ensure_am; ensure_pwr "$AM_DW"
burst "PASO 1 — 27.185 MHz AM — setpoint ${AM_DW} (décimas de W)" "$BURST_S"
P1_FWD=$(ask "P_fwd del medidor de RF (W)")
P1_SWR=$(ask "ROE del medidor de RF")
P1_H2=$(ask "2º armónico 54.370 MHz en el analizador (dBc)")
P1_H3=$(ask "3º armónico 81.555 MHz en el analizador (dBc)")
P1_MOD=$(ask "¿AM audible en el receptor de control? (s/n)")
sayq "PASO1 P_fwd=$P1_FWD ROE=$P1_SWR H2=$P1_H2 H3=$P1_H3 mod=$P1_MOD"

# --- PASO 2: 27.185 MHz FM/NFM ---------------------------------------------
say ""; say "===== PASO 2: 27.185 MHz FM/NFM ====="
ensure_freq "$CC_FREQ"
say "El socket JSON NO puede fijar NFM con confianza (bug §3.1.1):"
say "fijá NFM en la PANTALLA de la radio (o con wfview/CAT, M_NFM 0x05)."
printf '  > Apretá Enter cuando la radio muestre NFM... '; read -r _
say "mode reportado por el socket: $(state | jf mode) (puede ser erróneo; §3.1.1)"
ensure_pwr "$FM_DW"
burst "PASO 2 — 27.185 MHz FM/NFM — setpoint ${FM_DW}" "$BURST_S"
P2_FWD=$(ask "P_fwd (W)")
P2_SWR=$(ask "ROE")
P2_H2=$(ask "2º armónico (dBc)")
P2_H3=$(ask "3º armónico (dBc)")
P2_MOD=$(ask "¿FM audible en el receptor? (s/n)")
P2_DEV=$(ask "desviación FM (kHz; '-' si no hay medidor)")
sayq "PASO2 P_fwd=$P2_FWD ROE=$P2_SWR H2=$P2_H2 H3=$P2_H3 mod=$P2_MOD dev=$P2_DEV"

# --- PASO 3: 26.515 MHz (grid B, opcional y con autorización extra) --------
GRIDB_STATUS="SKIPPED: AUTHORIZED_GRID_B != 1 (no se transmitió en 26.515)"
P3_FWD=""; P3_SWR=""; P3_H2=""; P3_H3=""
if [ "$AUTHORIZED_GRID_B" = "1" ]; then
    say ""; say "===== PASO 3: 26.515 MHz AM (grid B, FUERA del SBC) ====="
    ensure_freq "$GRIDB_FREQ"
    say "banda reportada: $(state | jf band) (se espera CB B)"
    ensure_am; ensure_pwr "$GRIDB_DW"
    burst "PASO 3 — 26.515 MHz AM — setpoint ${GRIDB_DW} (1.0 W por defecto)" "$BURST_S"
    P3_FWD=$(ask "P_fwd (W)"); P3_SWR=$(ask "ROE")
    P3_H2=$(ask "2º armónico (dBc)"); P3_H3=$(ask "3º armónico (dBc)")
    GRIDB_STATUS="MEDIDO"
else
    say ""; say "PASO 3 (26.515): $GRIDB_STATUS"
fi

restore
trap - INT TERM

# --- resumen y veredicto PRELIMINAR (lo firma el humano) --------------------
chk_pwr() { awk -v p="${1:--999}" 'BEGIN{exit !(p>=3.58 && p<=4.49)}'; }
chk_h()   { awk -v h="${1:--999}" 'BEGIN{exit !(h<=-43)}'; }
if chk_pwr "$P1_FWD" && chk_pwr "$P2_FWD" \
   && chk_h "$P1_H2" && chk_h "$P1_H3" && chk_h "$P2_H2" && chk_h "$P2_H3"; then
    PRE="PASS PRELIMINAR (potencia y armónicos dentro del criterio provisional §7); falta la firma y el resto de §7"
else
    PRE="NO PASS PRELIMINAR: hay valores fuera de criterio, vacíos o no medidos (ver §7)"
fi
SUMMARY="$EVID/f0-resumen-$STAMP.md"
cat > "$SUMMARY" <<EOF
# F0 CB 27 MHz — resumen de la corrida $STAMP

- Radio: $RADIO_IP | GUI md5: ${MD5:-?} (analizado: $GUI_MD5_ESPERADO)
- Baseline: freq=$BASE_FREQ mode=$BASE_MODE pwr=$BASE_PWR band=$BASE_BAND
- PTT: $PTT_MODE | ATU_WAS_ON=$ATU_WAS_ON_BAK
- Log crudo: $LOG

| Paso | Frecuencia | Modo | Setpoint (déc. W) | P_fwd (W) | ROE | 2º arm (dBc) | 3º arm (dBc) | Observación |
|---|---|---|---|---|---|---|---|---|
| 1 | 27.185 | AM | $AM_DW | $P1_FWD | $P1_SWR | $P1_H2 | $P1_H3 | mod=$P1_MOD |
| 2 | 27.185 | FM/NFM | $FM_DW | $P2_FWD | $P2_SWR | $P2_H2 | $P2_H3 | mod=$P2_MOD dev=$P2_DEV kHz |
| 3 | 26.515 | AM (opc.) | $GRIDB_DW | $P3_FWD | $P3_SWR | $P3_H2 | $P3_H3 | $GRIDB_STATUS |

## Veredicto (lo decide y firma el humano; criterios en §7)
- **$PRE**
- PASS / FAIL / BLOCKED: ____________
- Firma (owner/QA): ______________________  Fecha: ____________
EOF
say ""; say "Resumen: $SUMMARY"
say "Veredicto preliminar: $PRE"
exit 0
```

### 6.4 Lista de pasos manuales (si se prefiere no usar el script)

1. **Preflight**: confirmar `AUTHORIZED=1`, carga fantasma, medidor, analizador+atenuador, ATU off,
   sin XPA, y que la radio esté en **RX** (`{"cmd":"get"}` → `"tx":false`). Registrar
   `md5sum /usr/sbin/x6200_gui` y el estado inicial (freq/modo/potencia/banda).
2. **PASO 1 — 27.185 AM**: `{"cmd":"freq","val":27185000}` (verificar `freq` y `band`=`CB C`;
   la banda deja el modo en AM por `sql/band_params.csv:33-34`), `{"cmd":"pwr","val":40}`. PTT
   **físico** (o CAT/wfview) durante 5 s; anotar P_fwd y ROE del medidor, y los armónicos 2º/3º
   del analizador (dBc). Soltar y verificar `"tx":false`.
3. **PASO 2 — 27.185 FM/NFM**: **fijar NFM en la pantalla** (o por CAT `C_SET_MODE 0x06` con
   `M_NFM 0x05`), NUNCA con `{"cmd":"mode","val":"NFM"}` (§3.1.1). Repetir el burst de 5 s y las
   mediciones.
4. **PASO 3 — 26.515** (sólo `AUTHORIZED_GRID_B=1`): `{"cmd":"freq","val":26515000}` (banda
   `CB B`), potencia 1,0 W (`{"cmd":"pwr","val":10}`), burst y mediciones. Sin la variable, **NO
   se transmite**: registrar `SKIPPED`.
5. **Restaurar**: frecuencia → modo → potencia originales (leídos en el preflight); ATU a su
   estado previo (a mano); verificar `"tx":false` y estado final.
6. **Cerrar la evidencia**: log, resumen, fotos de la pantalla TX Info y de los instrumentos,
   md5 de la GUI. Volcar el resultado a `docs/PRUEBAS.md` (**otra tarea**: este documento no edita
   `PRUEBAS.md`).

### 6.5 Restauración (obligatoria, en este orden)

| Qué | Cómo | Verificación |
|---|---|---|
| **RX / PTT** | El operador suelta el PTT; el script no puede soltarlo | `{"cmd":"get"}` → `"tx":false`; si sigue en TX: **apagar la radio** |
| **Frecuencia** | `{"cmd":"freq","val":<baseline>}` (primero, porque el cambio de banda recarga parámetros de banda) | `state.freq == baseline` |
| **Modo** | `{"cmd":"mode","val":"<baseline>"}` si está en la lista remota; si era SAM/WFM/NFM, **a mano** | `state.mode` y pantalla (ojo bug NFM) |
| **Potencia** | `{"cmd":"pwr","val":<baseline>}` | `state.pwr == baseline` |
| **ATU** | Si estaba ON (`ATU_WAS_ON=1`), volver a ON **a mano** (el socket no tiene comando de ATU) | Pantalla/indicador de ATU |
| **Split/filtros/AGC** | Los restaura la carga de `band_params` al volver la banda; verificar en pantalla | — |

**Si el TX no se corta al soltar el PTT: APAGAR LA RADIO** (botón de encendido) y desconectar la
carga. El script no tiene forma de forzar el PTT off.

### 6.6 Constancia de NO transmisión si no hay autorización

Dos mecanismos, ambos ya previstos:

1. **Automático**: correr el script **sin** `AUTHORIZED=1` (o con cualquier guarda en 0). Antes de
   abrir SSH escribe `docs/evidencia/2026-10-19-f0-cb-tx/f0-NO-TX-<fecha>.md` y sale con código 2.
   Ese archivo dice: guarda que abortó, que **no se abrió SSH**, que no se accionó PTT, que no se
   cambiaron freq/modo/potencia/ATU, que no se usaron instrumentos y que **no se transmitió**, con
   línea para la firma del owner/QA.
2. **Manual** (si no se corre nada): dejar en esta carpeta una nota firmada con: fecha/hora, que
   no se ejecutó el protocolo, que no se abrió sesión de control sobre la radio, que no se accionó
   PTT, y que el único acceso fue el recon read-only (`recon-radio.txt`).

La constancia de esta sesión de análisis está en **§0.3**.

### 6.7 Evidencia a capturar (nombres de archivo)

En `docs/evidencia/2026-10-19-f0-cb-tx/`:

- `f0-run-<fecha>.log` — log crudo del script (cada JSON enviado/recibido y cada estado).
- `f0-resumen-<fecha>.md` — tabla de mediciones + veredicto preliminar firmado.
- `f0-NO-TX-<fecha>.md` — sólo si se abortó sin autorización.
- `tx-info-<fecha>.jpg` — foto de la pantalla TX Info (PWR/ROE/ALC del equipo; secundario).
- `medidor-<fecha>.jpg`, `espectro-<fecha>.png` — fotos/capturas de los instrumentos.
- `pantalla-<fecha>-AM.jpg`, `pantalla-<fecha>-FM.jpg` — foto de la pantalla de la radio en cada
  paso (frecuencia + modo + banda visibles; es la única verificación confiable del modo FM).
- Instrumentos: marca, modelo, número de serie y fecha de calibración en el resumen.

---

## 7. Qué falta para validar y qué resultado sería PASS/FAIL/BLOCKED (d)

**Lo que falta (resumen):**

1. **Potencia real** medida con medidor calibrado sobre carga fantasma, en AM y FM, a setpoint
   conocido, con la fuente registrada (13,8 V vs batería).
2. **Armónicos 2º y 3º** medidos con analizador de espectro + atenuador (54,370 y 81,555 MHz), en
   dBc respecto de la fundamental.
3. **Modulación** AM/FM verificada (receptor de campo cercano; idealmente desviación FM ~±2,5 kHz).
4. **PTT**: que corte al soltar, por los tres caminos relevantes (físico, CAT/wfview, y el del
   micrófono —que es el que puede estar fuera de Linux—).
5. **Restauración** verificada (freq/modo/potencia/ATU/RX).
6. **Autorización** y condiciones legales (§5.1).

**Criterios provisionales** (no son una norma ni una homologación; el owner puede ajustarlos antes
de la corrida, pero deben quedar escritos **antes**):

- Potencia: a setpoint 4,0 W, P_fwd medida en **3,58–4,49 W** (4,0 W ±1 dB) en AM y en FM.
- Armónicos: 2º y 3º **≤ −43 dBc** respecto de la fundamental (referencia conservadora de espurios
  HF, 47 CFR 97.307(d); para CB la norma aplicable la define el owner/legal).
- PTT: `tx` vuelve a `false` en ≤1 s de soltar, y no hay TX residual.
- 26.515: si no está autorizado, **`SKIPPED` sin TX** (no afecta el PASS de 27.185); si está
  autorizado, se registra aparte (grid B está fuera del SBC).

| Resultado | Cuándo | Consecuencia |
|---|---|---|
| **PASS** | Con **todos** los instrumentos y la autorización: RF en 27.185 en AM y FM; potencia en el rango provisional; 2º/3º armónico ≤ −43 dBc; PTT corta; restauración verificada; evidencia completa (md5 del build, instrumentos, fotos, log); 26.515 autorizado+medido o `SKIPPED` sin TX. | Se puede registrar F0 como cumplida en `PRUEBAS.md` y pasar a F1. **Sigue sin ser homologación CB ni autorización legal**: sólo evidencia técnica. |
| **FAIL** | Se transmitió y: **no hay RF** en 27.185 (medidor=0) → el MCU no transmite en 27 MHz (el módulo pasa a **RX-only**, issue §7.1); o potencia fuera de rango; o armónicos por encima del criterio; o el TX **no corta** al soltar (falla de seguridad, reportar ya); o no hay modulación. | Corregir/replantear alcance; no declarar TX validado. |
| **BLOCKED** | **Sin autorización** del owner; **sin carga fantasma**, **sin medidor calibrado**, **sin analizador+atenuador**; ATU que no se puede apagar; XPA125B conectada; radio inalcanzable o helper ausente; la frecuencia no queda fijada (banda ausente en `params.db`); instrumentos sin calibración declarada; operador ausente. | **F0 no se declara PASS** (regla dura). Se deja constancia y se vuelve cuando estén los medios. |
| **Parcial** | Se midió potencia pero no armónicos (o al revés) | Se registra lo medido como `medido` y lo que falta como `no medido`; el veredicto global es **BLOCKED**, no PASS. |

> **Regla dura del issue #146:** *"No publicar TX como validado hasta completar F0 y medir
> armónicos"* (§7.3). Y: **si no hay instrumento, la F0 no se puede declarar PASS** — a lo sumo
> `no medido`/`requires-radio`.

---

## 8. Referencias (e)

### 8.1 Código del port (`/home/pablo/desa/x6200/x6200_qa`)

- `src/radio.c:39` (`IDLE_TIMEOUT`), `:236-278` (máquina de estados RX/TX/ATU/SWRSCAN y
  `tx_info_update`), `:320-321` (`x6200_control_idle`), `:364-370` (`on_vfo_freq_change`),
  `:372-377` (`on_vfo_mode_change`), `:505-531` (registro de observers, incluido `cfg.pwr`),
  `:622-630` (`radio_set_freq`), `:632-637` (`radio_check_freq` 0,5–55 MHz), `:718-753`
  (`radio_start_swrscan`/`radio_stop_swrscan`), `:773-800` (`radio_state_save/restore`,
  `radio_set_pwr`), `:907-909` (`radio_set_ptt`).
- `src/cfg/band.c:74-75` (observer de `cfg_cur.fg_freq`), `:135-160`
  (`cfg_band_set_freq_for_vfo`, gate de tabla de bandas), `:234-275`
  (`get_band_info_by_freq`), `:559-571` (SQL de banda por frecuencia), `:599-606`
  (`on_fg_freq_change`).
- `src/main_screen.c:902-925` (`KEYPAD_PTT`), `:1331-1334` (`main_screen_set_freq`),
  `:1624-1635` (locks de la GUI).
- `src/remote_server.cpp:1-23` (contrato del socket: *"There is no PTT"*), `:120-137`
  (`cmd_id_t`, `mode_names[]`), `:139-203` (`app_names[]`, `app_cmds[]`, fila `swrscan`),
  `:389-400` (gate de TX del server), `:417-430` (`freq`/`tune`/`mode`), `:456-458` (`pwr`),
  `:594-631` (línea de estado), `:745-760` (tabla de enteros), `:870-890` (parseo de modo,
  rango de frecuencia y locks).
- `src/cat.cpp:54` (`LOCAL_ADDRESS 0xA4`), `:84` (`C_CTL_PTT 0x1C`), `:292-312`
  (`ci_mode_2_x_mode`), `:462-469` (`C_SET_FREQ`), `:471-477` (`C_SET_MODE`), `:565-616`
  (`C_CTL_LVL`, PWR en 0x0A), `:618-665` (`C_RD_SQSM`: power/SWR/ALC), `:778-795` (`C_CTL_PTT`),
  `:999` (`open("/dev/ttyS2")`).
- `src/wfview_server.c:9-12` (protocolo Icom LAN, puertos 50001/50002/50003), `:334-340`
  (CI-V → `cat_process_frame`), `:452-469` (credenciales por defecto), `:516-546` (init y
  `cat_set_net_sink`); `src/main.c:615-616` (init no-op si `enabled=0`);
  `src/dialog_settings.c:960-993` (switch "wfview server").
- `src/keypad.c:117-119` (trigger superior → `KEYPAD_PTT`).
- `src/dialog_swrscan.c:876-903` (guardas del comando remoto), `:914-924` (`ax` de SWRScan);
  `src/dialog_swrscan.h:62-63` (multipasada TX deshabilitada), `:72-75` (potencia del barrido
  0,1 W); `src/swr_session.h:61-64` y `src/swr_session.c:66-70` (ventana legal de TX
  0,5–55 MHz).
- `src/cfg/settings_manager.cpp:113-115` (`is_valid_hw_freq` = `radio_check_freq`);
  `src/cfg/settings_manager.h:107-115` (`HF_MIN_FREQ`/`HF_MAX_FREQ`);
  `src/cfg/cfg_api.h:66-76` (alias `x6100_mode_*` → `x6200_mode_*`).
- `src/tx_info.c:255-289` (`tx_info_update`/`tx_info_refresh`; el widget de pantalla).
- `src/CMakeLists.txt:69` (enlaza `aether_x6200_control`).
- `sql/bands_cb.csv:1-4` (CB A–D; CB B=26,515–26,745, CB C=26,965–27,195);
  `sql/band_params.csv:29-36` (`vfoa_mode = 6` = AM para las cuatro CB);
  `sql/params.sql:12` (import de `bands_cb.csv`).
- `tools/remote/x6200-remote.c` (cliente del socket; helper estático para la radio).

### 8.2 Librería de control (`/home/pablo/desa/x6200/AetherX6200Buildroot/buildroot/dl/aether-x6200-control/git/`)

- `src/control.c:20-24` (`vfo_mode_set`), `:26-45` (`vfo_freq_set`; llamada a `set_band`
  **comentada** en `:30`), `:156-166` (`ptt_set`), `:216-230` (`atu_set`), `:244-255`
  (`swrscan_set`), `:278-283` (`txpwr_set`).
- `src/low/control.c:25-28` (`cmd_struct_t`), `:36-37` (fd/addr I2C `0x72`), `:50`
  (`/dev/i2c-0`), `:70-95` (`send_regs` con `ioctl I2C_RDWR`), `:129-160` (`init`/handshake),
  `:390-420` (bloque inicial: `ham_band=0`, `rfg_txpwr`), `:445-457` (`cmd`), `:459-462` (`get`),
  `:483-532` (`band_index`), `:534-546` (`set_band`; comando `vi_vm` comentado en `:542`).
- `include/aether_radio/x6200_control/low/control.h:17-70` (enum de registros), `:88-100` (enum
  de modo: `sam=7`, `nfm=8`, `wfm=9`), `:108-125` (bits del registro de TRX, `x6200_iptt`).
- `include/aether_radio/x6200_control/low/flow.h:43-67` (flags, `tx`), `:69-89` (`x6200_flow_t`:
  `tx_power`, `vswr`, `alc_level`).

### 8.3 Radio y evidencia de la fecha

- `docs/evidencia/2026-10-19-f0-cb-tx/recon-radio.txt` — recon **read-only** en la radio
  (2026-10-19 13:59 UTC): §0 (identificación), §1 (`/etc/xgradio/` **no existe**),
  §2 (`grep fullband` vacío), §4 (`x6200_gui` md5 `9a094c421b0eb05c103abf782ea00db6`), §7
  (`find / -xdev -iname '*xgradio*' -o -iname '*fullband*'` → vacío).
- `docs/evidencia/2026-10-19-f0-cb-tx/ref-fabrica.md` — gate `fullband-tx` del firmware oficial
  V1.0.6 (task-2): `/etc/xgradio/xgradio.conf` **línea 7** `fullband-tx=disable`, leído por
  `XBandPlan::XBandPlan()` de `/usr/app_qt/x6200_ui_v100`; la UI baja el flag y el permiso de TX por
  banda al baseband por `BaseCommIIC` (§4.4); conclusión **UI-side** (§5); firmware del MCU opaco.
- `docs/PRUEBAS.md` §"Cómo se registra una corrida" (líneas 6-22): convención `medido`/`código`/
  `no medido` y destino de la evidencia cruda.

### 8.4 Issue y referencias externas

- Issue #146 (definición de F0, §3.2/§3.3/§7/§11):
  <https://github.com/iariap/x6200_gui/issues/146>
- Comentario del owner (2026-10-19, gate `fullband-tx` confirmado ausente en la radio):
  <https://github.com/iariap/x6200_gui/issues/146#issuecomment-6268894381>
- Full TX / `fullband-tx` (referencia pública del mod del firmware oficial):
  <https://github.com/tom-acco/Xiegu-X6200-Research/blob/main/Full%20TX/README.md>
- Normativa citada por el issue (SBC/ENACOM, FCC CB):
  <https://enacom.gob.ar/tramites/autorizaciones-solicitud-alta-operar-servicio-banda-ciudadana-sbc-_t16>,
  <https://www.law.cornell.edu/cfr/text/47/95.967>

---

### Anexo A — Resumen de hallazgos nuevos de este análisis (para el lead)

1. **No hay gate de banda/frecuencia en Linux** en el camino de TX (sólo rango 0,5–55 MHz y tabla
   de bandas para sintonizar). El único gate posible es el BASE/MCU → `requires-radio` (F0).
2. **El socket remoto no tiene PTT** (`src/remote_server.cpp:20-22`), pero **CAT y wfview sí**
   (`C_CTL_PTT 0x1C` → `radio_set_ptt`). Catálogo en §3.5.
3. **Bug de mapeo de modo por socket:** `{"cmd":"mode","val":"NFM"}` selecciona **SAM** (modo 7);
   el modo 8 (NFM real) se imprime fuera de rango en la línea de estado. Código, no medido.
   `src/remote_server.cpp:136`, `:623`, `:870-878`; enum `control.h:88-100`.
4. **El registro `ham_band` (comandos 0 y 6) se inicializa en 0 y nunca se actualiza**
   (`low/control.c:398,403`): candidato a gate del lado Linux si el MCU lo usara — **no fundado**,
   queda como hipótesis a medir en F0. Se refuerza con el hallazgo de la task-2: en el firmware
   oficial **la UI le manda al baseband un permiso de TX por banda y el flag `fullband-tx`**
   (`ref-fabrica.md` §4.4), y el port no replica ese permiso por banda (sólo manda `ham_band=0`):
   el MCU está "acostumbrado" a recibir ese dato, y si lo usa para gatear es justo lo que F0 mide.
5. **Telemetría del equipo insuficiente para F0**: el socket no expone potencia/ROE; el detector
   interno (`tx_power`/`vswr`/`alc`) sólo se ve en pantalla (TX Info) o por CAT convertido a escala
   de medidor. No es instrumento.
6. **El 26.515 cae en CB B y el 27.185 en CB C** con la tabla de bandas actual (`sql/bands_cb.csv`),
   pero **27.205–27.405 no tiene banda** (limitación conocida del issue, se arregla en F1): si F0
   quisiera probar otro canal de la parte alta del CEPT con el socket, hoy no se sintonizaría.

---

## Nota de actualización — prueba en vivo del 19/10 14:36 UTC (corrige lo que sigue de este documento)

La prueba en vivo con autorización del owner (ver [`live-tx/transcript.md`](live-tx/transcript.md))
midió estas cosas que corrigen/afinan el protocolo de arriba:

1. **El comando correcto es `{"cmd":"swrscan","val":1}`** (el nombre de la app como `cmd`); la
   variante `{"cmd":"app_cmd",...}` **no existe en este build** y contesta
   `{"t":"err","err":"Unknown command"}`. El resto del protocolo (§3.1.2) queda igual.
2. **`state.pwr` está congelado en 50** (`remote_server.cpp` usa `cfg_sm.p_pwr.get()`, que devuelve
   `value_`; el valor real vive en `getf()`/`cfg.pwr`). Por eso **no sirve** para verificar la
   potencia seteada (el paso de restauración `state.pwr == baseline` de §6 no se puede usar así).
3. **El evdev no sirvió para keyear PTT**: se inyectaron `input_event` sintéticos
   (`BTN_TRIGGER_HAPPY4` en `event0` = `matrix_keypad`) y la GUI no cambió de estado; la tecla ATU
   inyectada tampoco cambió el indicador. No se insistió (podía dejar una tecla pegada).
4. **Resultado**: el barrido de ROE transmite en 27.185 MHz (`tx:true`) y el puente de ROE mide RF
   en **43/51** puntos de 26,935–27,435 MHz → **el MCU no bloquea el TX**. La F0 completa sigue
   BLOCKED por falta de instrumentos (potencia real y armónicos).
