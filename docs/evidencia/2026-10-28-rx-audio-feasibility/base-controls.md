# Matriz de controles del BASE que actúan sobre el audio de recepción

- **Issue**: [iariap/x6200_gui#345](https://github.com/iariap/x6200_gui/issues/345) — tarea **A** (matriz de controles, sin tocar la radio).
- **Base**: `a1c667eee6d4c138416e5ec459dd86de93cb630f` (= `origin/main` al arrancar).
- **Worktree**: `state/worktrees/x6200/rx-audio-feasibility`, rama `factory/rx-audio-feasibility`.
- **Qué es**: inventario de lo que la GUI le pide al BASE por I2C en la familia `nr*`, `nb*`, `dnf*`, `rx_filter*`,
  `rx_eq*` (y los tres controles de audio adyacentes: `rxvol`, `record`, `spmode`), con fuente del valor, rango
  **sustentado**, default, persistencia, estado OFF y qué se sabe del efecto en CW/digital.
- **Qué NO es**: no se tocó la radio, ni registros, ni config, ni BT, ni PulseAudio/ALSA. **No hay ninguna
  medición nueva** en este documento: lo medido que se cita viene de corridas anteriores y se marca así.
- **No se propone** NR nuevo, ni presets, ni UI, ni routing.

## Clases de evidencia

`CODE` (archivo:línea) · `MEASURED-HOST` · `MEASURED-HARDWARE` (registro previo, citado) · `HYPOTHESIS` ·
`BLOCKED` · `UNVERIFIED`. Cada celda de la columna *evidencia* lleva su clase.

## 0. De dónde salen los rangos (y por qué casi todos son `UNKNOWN`)

El SDK del BASE (`aether-x6200-control`) **no vive en este repo**: lo provee Buildroot y en la radio es
`/usr/lib/libaether_x6200_control.so.0.15.3` (paquete `aether-x6200-control-v0.1.0`,
`docs/evidencia/assessment-rom-194/02-rom/capturas/packages-installed.txt:1`; el `.so` aparece en
`docs/evidencia/2026-10-19-f0-cb-tx/recon-radio.txt:544`). Lo que sí está versionado son **copias fieles** de sus
headers:

| Copia en el repo | Qué dice de sí misma | Verificación hecha en esta corrida |
|---|---|---|
| `tools/ui_harness/third_party/aether_radio/x6200_control/**` | `tools/ui_harness/third_party/README.md`: "They are copied verbatim" de `${CMAKE_SYSROOT}/usr/include/` | `diff -q` contra el checkout público del SDK `gdyuldin/X6200Control@0ee1ae3` (`/home/pablo/desa/x6200/.x6200_ctl`): `control.h` y `low/control.h` **idénticos** |
| `tests/native/stubs_radio/aether_radio/x6200_control/**` | `tests/native/stubs_radio/README.md`: "copias fieles del SDK publico"; los stubs mínimos son otros headers | `diff -q` contra la copia de `ui_harness`: **idénticos** |

Consecuencia, y es la regla dura de esta matriz:

- El **tipo** de cada parámetro sí está en el header (p. ej. `uint8_t level`, `control.h:106`) ⇒ `CODE`.
- El **ancho del campo** en el registro sale de la implementación del SDK (`set_kv` de bits), que está en el
  checkout externo ⇒ se cita como `CODE (SDK, fuera del repo)`, con commit, y **no** se usa para afirmar el rango
  semántico del BASE.
- El **rango que el BASE acepta o interpreta** (0..100? 0..60? 0..255? qué significa cada paso) **no está en
  ningún header accesible** ⇒ `UNKNOWN` con causa, en cada fila. `BLOCKED`: no se puede leer del equipo en esta
  tarea (no se toca la radio) y no hay documentación del fabricante en el repo.
- Que la lib de la ROM (`0.15.3`) se haya compilado exactamente de ese commit del SDK **no está probado**
  (`UNVERIFIED`): el ancho de campo se usa como evidencia de *forma*, no de versión.

## 1. Cadena de una perilla hasta el registro

```
cfg.<item>.val (Subject)            src/cfg/cfg.c (default + nombre en params.db)
        │  subject_set_int(...)
        ▼
observer registrado en radio_init  src/radio.c:669-690   (subject_add_observer_and_call)
        │  on_change_uint8 / on_change_int8 / on_change_bool / on_change_uint16
        │  (src/radio.c:345-367: cast al tipo de la API + WITH_RADIO_LOCK; **no** valida rango ni modo)
        ▼
x6200_control_<fn>()                SDK  → empaqueta bits en un registro de 32 bits
        ▼
registro I2C (cmd × 4)              docs/HALLAZGOS.md:4405-4413 (§69, framing de lectura verificado)
```

`CODE`: el cast de `on_change_uint8` es `(uint8_t) new_val` (`src/radio.c:357-361`), sin `limit()`; el recorte de
rango, cuando existe, lo hace la UI o el remoto **antes** de escribir el subject (§2, columna *rango*). Un valor
fuera de 0..255 en `params.db` se truncaría en silencio: `HYPOTHESIS` sobre la consecuencia, `CODE` sobre el cast.

## 2. Tabla A — DSP del BASE: NR, NB, DNF

| control | API/función | fuente del valor (cfg/UI/remoto) | rango sustentado en headers reales (archivo:línea) | default | persistencia (dónde) | estado OFF | efecto sobre CW/digital | evidencia |
|---|---|---|---|---|---|---|---|---|
| **NR on/off** (NR del BASE) | `x6200_control_nr_set(bool)` (`control.h:105`) | `cfg.nr`: MFK **NR** `src/mfk.cpp:481-489`; botón `controls_toggle_nr()` `src/controls.cpp:63-66`; `ACTION_NR_TOGGLE` (tecla/F del HMic) `src/main_screen.c:526-530` y `src/dialog_settings.c:1238`; CAT `0x40` `src/cat.cpp:686-694`; remoto `{"cmd":"nr"}` `src/remote_server.cpp:767` | `bool` (`control.h:105`); bit 24 del registro 23 (`x6200_nrthr_nbw_nbthr_nre_nbe`, `low/control.h:43`) — `CODE (SDK, fuera del repo) src/control.c:502-506`. Rango del BASE: **UNKNOWN** (no aplica: es 1 bit) | `0` (`cfg.c:390`) | `params` (`name='nr'`) en `/mnt/params.db` (`src/cfg/params.c:23-28`, path `src/params/db.c:33`) | escribe 0 en el bit 24: no hay «estado OFF» distinto del valor 0 | `UNVERIFIED`: ningún camino de la GUI avisa el modo al BASE por este registro y el observer no consulta `cfg_cur.mode` (`src/radio.c:675`) | `CODE src/radio.c:675,345-349`; `CODE src/cfg/cfg.c:390` |
| **NR level** | `x6200_control_nr_level_set(uint8_t)` (`control.h:106`) | `cfg.nr_level`: MFK **NR level** `src/mfk.cpp:498-505` (`limit(...,0,100)`); remoto `{"cmd":"nr_level"}` `src/remote_server.cpp:481` (setter), **limitado a 0..60** (`:13`, `:774`) | `uint8_t` (`control.h:106`); campo de 8 bits, bits 0-7 (`CODE (SDK) src/control.c:508-512`) ⇒ 0..255 representables. **Rango semántico del BASE: `UNKNOWN`** — el header no lo declara y no hay doc del fabricante | `0` (`cfg.c:391`) | `params` (`name='nr_level'`) | **no hay gate**: se escribe aunque `cfg.nr` = 0 (el observer no consulta `nr`, `src/radio.c:676`) | `UNVERIFIED`. Dato de código que **no** cierra el rango: el comentario de `src/mfk.cpp:499` dice `0, 60` y el código aplica `0, 100`; el remoto usa `0..60`. Tres cifras distintas, ninguna sustentada en el SDK | `CODE src/radio.c:676`; `CODE src/mfk.cpp:498-505`; `CODE src/remote_server.cpp:13,774` |
| **NB on/off** (noise blanker) | `x6200_control_nb_set(bool)` (`control.h:101`) | `cfg.nb`: MFK **NB** `src/mfk.cpp:431-441`; `controls_toggle_nb()` `src/controls.cpp:59-62`; `ACTION_NB_TOGGLE` `src/main_screen.c:566-571`, `src/dialog_settings.c:1239`; CAT `0x22` `src/cat.cpp:677-684`; remoto `{"cmd":"nb"}` `src/remote_server.cpp:766` | `bool` (`control.h:101`); bit 25 del registro 23 (`low/control.h:43`) — `CODE (SDK) src/control.c:484-488` | `0` (`cfg.c:386`) | `params` (`name='nb'`) | valor 0 en el bit 25 | `UNVERIFIED` (mismo razonamiento que NR: observer sin lógica de modo, `src/radio.c:672`) | `CODE src/radio.c:672`; `CODE src/cfg/cfg.c:386` |
| **NB level** | `x6200_control_nb_level_set(uint8_t)` (`control.h:102`) | `cfg.nb_level`: MFK **NB level** `src/mfk.cpp:448-455` (`0..100`, paso 5); remoto `{"cmd":"nb_level"}` `src/remote_server.cpp:476` (setter), `0..100` (`:772`) | `uint8_t` (`control.h:102`); bits 16-23 del registro 23 (`CODE (SDK) src/control.c:490-494`). Rango del BASE: **`UNKNOWN`** | `10` (`cfg.c:387`) | `params` (`name='nb_level'`) | sin gate (se escribe aunque `nb`=0, `src/radio.c:673`) | `UNVERIFIED` | `CODE src/radio.c:673`; `CODE src/cfg/cfg.c:387` |
| **NB width** | `x6200_control_nb_width_set(uint8_t)` (`control.h:103`) | `cfg.nb_width`: MFK **NB width** `src/mfk.cpp:465-471` (`0..100`, paso 5); remoto `{"cmd":"nb_width"}` `src/remote_server.cpp:479` (setter), `0..100` (`:773`) | `uint8_t` (`control.h:103`); bits 8-15 del registro 23 (`CODE (SDK) src/control.c:496-500`). Rango del BASE: **`UNKNOWN`**; la **unidad** tampoco está en el header (la UI rotula "Hz", `src/mfk.cpp:471`; el parámetro del SDK se llama `hz`, `control.h:103`) ⇒ `UNVERIFIED` que 1 paso = 1 Hz | `10` (`cfg.c:388`) | `params` (`name='nb_width'`) | sin gate (`src/radio.c:674`) | `UNVERIFIED` | `CODE src/radio.c:674`; `CODE src/cfg/cfg.c:388` |
| **DNF modo** (notch del BASE: off/manual/auto) | `x6200_control_dnf_set(x6200_dnf_mode_t)` (`control.h:97`) | `cfg.dnf`: MFK **DNF** cicla `%3` `src/mfk.cpp:381-386`; `controls_loop_dnf()` `src/controls.cpp:48-53`; etiqueta `cfg_dnf_label_get()` `src/cfg/cfg.c:116-124` | enum de 3 valores `x6200_dnf_off/manual/auto` = 0/1/2 (`low/control.h:153-157`) — `CODE`; 2 bits en bits 24-25 del registro 24 (`x6200_dnfcnt_dnfwidth_dnfe`, `low/control.h:44`) — `CODE (SDK) src/control.c:466-470`. Valores 3 (reservado) ⇒ `UNKNOWN` qué hace el BASE | `x6200_dnf_off` = 0 (`cfg.c:382`) | `params` (`name='dnf'`) | `x6200_dnf_off` = 0 | `UNVERIFIED` (`src/radio.c:669`; el observer no mira el modo) | `CODE src/radio.c:669`; `CODE src/cfg/cfg.c:382,116-124` |
| **DNF center** | `x6200_control_dnf_center_set(uint16_t)` (`control.h:98`) | `cfg.dnf_center`: MFK **DNF center** `src/mfk.cpp:399-405` (`limit(...,100,3000)`, paso 50 Hz) | `uint16_t` (`control.h:98`); campo de 12 bits, bits 0-11 (`CODE (SDK) src/control.c:472-476`) ⇒ 0..4095 representables. Rango del BASE: **`UNKNOWN`**; la UI usa 100..3000 como subconjunto, no como límite del equipo | `1000` (`cfg.c:383`) | `params` (`name='dnf_center'`) | sin gate (se escribe aunque `dnf`=off, `src/radio.c:670`) | `UNVERIFIED` | `CODE src/radio.c:670`; `CODE src/cfg/cfg.c:383` |
| **DNF width** | `x6200_control_dnf_width_set(uint16_t)` (`control.h:99`) | `cfg.dnf_width`: MFK **DNF width** `src/mfk.cpp:415-421` (`limit(...,10,100)`, paso 5) | `uint16_t` (`control.h:99`); 12 bits, bits 12-23 (`CODE (SDK) src/control.c:478-482`). Rango del BASE: **`UNKNOWN`** | `50` (`cfg.c:384`) | `params` (`name='dnf_width'`) | sin gate (`src/radio.c:671`) | `UNVERIFIED` | `CODE src/radio.c:671`; `CODE src/cfg/cfg.c:384` |

Filas: **8**.

## 3. Tabla B1 — filtro de RX (`rx_filter*`)

| control | API/función | fuente del valor (cfg/UI/remoto) | rango sustentado en headers reales (archivo:línea) | default | persistencia (dónde) | estado OFF | efecto sobre CW/digital | evidencia |
|---|---|---|---|---|---|---|---|---|
| **Filtro RX — borde bajo** | `x6200_control_rx_filter_set_low(int16_t)` (`control.h:49`) | `cfg_cur.filter.low`, alimentado por el editor **DFL** (`src/dfl_rotary.c:52-60`, `src/dfl_inline.c:224-225`) y por el filtro guardado por modo (`src/cfg/mode.c:67,82-83`) | `int16_t` (`control.h:49`); 16 bits altos del registro 30 (`x6200_rxfilter`, `low/control.h:50`) — `CODE (SDK) src/control.c:140-145` (lee-modifica-escribe). Rango del BASE en Hz: **`UNKNOWN`** | por modo, de `mode_default_values()`: LSB/USB `50..2950`, **CW `−250..250`**, AM/SAM `0..4500`, NFM `0..3500`, WFM `0..8000` (`src/cfg/mode.c:260-300`) | `mode_params` por modo (`src/cfg/mode.c:77-78`; escritura `src/params/params.c:651`) | no aplica (es un borde) | **`CODE` y relevante para CW**: en `cw/cwr/am/sam/nfm/wfm` el observer **no** escribe el borde bajo (`src/radio.c:517-535`, `break` en `:520-525`) | `CODE src/radio.c:634,517-535` |
| **Filtro RX — borde alto** | `x6200_control_rx_filter_set_high(int16_t)` (`control.h:50`) | idem borde bajo (`cfg_cur.filter.high`) | `int16_t` (`control.h:50`); 16 bits bajos del registro 30 (`CODE (SDK) src/control.c:147-152`). Rango del BASE: **`UNKNOWN`** | idem | `mode_params` (`src/cfg/mode.c:78`) | no aplica | **`CODE`**: en `cw/cwr/am/sam/nfm/wfm` escribe **los dos bordes juntos** con simetría `set(-high, high)` (`src/radio.c:537-558`, `:541-550`) ⇒ en CW el ancho se fija con `high` y el borde bajo queda en `-high` | `CODE src/radio.c:635,537-558` |
| **Filtro RX — par (low, high)** | `x6200_control_rx_filter_set(int16_t low, int16_t high)` (`control.h:48`) | no lo llama ningún camino de UI: sólo el helper del observer de `high` | `int16_t, int16_t` (`control.h:48`); empaqueta `low<<16 \| high` (`CODE (SDK) src/control.c:134-138`) | — | — | — | el camino de CW/AM/NFM/WFM de la GUI lo usa (`src/radio.c:549`) | `CODE src/radio.c:549` |

Filas: **3**.

## 4. Tabla B2 — RX EQ (`rx_eq*` y `rx_eq_wfm*`)

El EQ de RX tiene **dos juegos de 5 bandas**: uno general (`x6200_rxeq`, registro 35, `low/control.h:55`) y uno
para WFM (`x6200_rxeqwfm`, registro 36, `low/control.h:56`). La UI elige cuál según el modo
(`src/dialog_eq.cpp:306-310,341-347`).

| control | API/función | fuente del valor (cfg/UI/remoto) | rango sustentado en headers reales (archivo:línea) | default | persistencia (dónde) | estado OFF | efecto sobre CW/digital | evidencia |
|---|---|---|---|---|---|---|---|---|
| **RX EQ on/off** | `x6200_control_rx_eq_set(bool)` (`control.h:135`) | `cfg.eq.rx.en`: botón **RX EQ** `src/dialog_eq.cpp:341-348` | `bool` (`control.h:135`); bit 25 del registro 35 (`CODE (SDK) src/control.c:580-584`) | `0` (`cfg.c:401`) | `params` (`name='eq_rx_en'`) | valor 0 | `UNVERIFIED`; el observer no consulta el modo (`src/radio.c:678`) | `CODE src/radio.c:678`; `CODE src/cfg/cfg.c:401` |
| **RX EQ p1** (300 Hz) | `x6200_control_rx_eq_p1_set(int8_t)` (`control.h:136`) | `cfg.eq.rx.p1`: slider `src/dialog_eq.cpp:246` (**−10..+10**), bandas `src/dialog_eq.cpp:144` | `int8_t` (`control.h:136`); 5 bits con signo, bits 0-4 (`CODE (SDK) src/control.c:586-590`) ⇒ −16..+15 representables. Rango del BASE: **`UNKNOWN`** | `0` (`cfg.c:402`) | `params` (`name='eq_rx_p1'`) | sin gate (se escribe aunque `en`=0) | `UNVERIFIED` | `CODE src/radio.c:679`; `CODE src/dialog_eq.cpp:246` |
| **RX EQ p2** (700 Hz) | `..._p2_set` (`control.h:137`) | `cfg.eq.rx.p2` | 5 bits, bits 5-9 (`CODE (SDK) src/control.c:592-596`) ⇒ −16..+15. **`UNKNOWN`** el del BASE | `0` (`cfg.c:403`) | `params` (`eq_rx_p2`) | sin gate | `UNVERIFIED` | `CODE src/radio.c:680` |
| **RX EQ p3** (1200 Hz) | `..._p3_set` (`control.h:138`) | `cfg.eq.rx.p3` | 5 bits, bits 10-14 (`CODE (SDK) src/control.c:598-602`). **`UNKNOWN`** | `0` (`cfg.c:404`) | `params` (`eq_rx_p3`) | sin gate | `UNVERIFIED` | `CODE src/radio.c:681` |
| **RX EQ p4** (1800 Hz) | `..._p4_set` (`control.h:139`) | `cfg.eq.rx.p4` | 5 bits, bits 15-19 (`CODE (SDK) src/control.c:604-608`). **`UNKNOWN`** | `0` (`cfg.c:405`) | `params` (`eq_rx_p4`) | sin gate | `UNVERIFIED` | `CODE src/radio.c:682` |
| **RX EQ p5** (2300 Hz) | `..._p5_set` (`control.h:140`) | `cfg.eq.rx.p5` | 5 bits, bits 20-24 (`CODE (SDK) src/control.c:610-614`). **`UNKNOWN`** | `0` (`cfg.c:406`) | `params` (`eq_rx_p5`) | sin gate | `UNVERIFIED` | `CODE src/radio.c:683` |
| **RX EQ WFM on/off** | `x6200_control_rx_eq_wfm_set(bool)` (`control.h:143`) | `cfg.eq.rx_wfm.en`: botón RX EQ con modo WFM `src/dialog_eq.cpp:341-348` | `bool` (`control.h:143`); bit 25 del registro 36 (`CODE (SDK) src/control.c:617-621`) | `0` (`cfg.c:408`) | `params` (`eq_rx_wfm_en`) | valor 0 | sólo aplica a WFM (`CODE src/dialog_eq.cpp:342-346`) | `CODE src/radio.c:685`; `CODE src/cfg/cfg.c:408` |
| **RX EQ WFM p1** (50 Hz) | `..._wfm_p1_set(int8_t)` (`control.h:144`) | `cfg.eq.rx_wfm.p1`; bandas `src/dialog_eq.cpp:145` | `int8_t` (`control.h:144`); 5 bits con signo (`CODE (SDK) src/control.c:623-627`). **`UNKNOWN`** el del BASE | `0` (`cfg.c:409`) | `params` (`eq_rx_wfm_p1`) | sin gate | sólo WFM | `CODE src/radio.c:686` |
| **RX EQ WFM p2** (300 Hz) | `..._wfm_p2_set` (`control.h:145`) | `cfg.eq.rx_wfm.p2` | 5 bits (`CODE (SDK) src/control.c:629-633`). **`UNKNOWN`** | `0` (`cfg.c:410`) | `params` (`eq_rx_wfm_p2`) | sin gate | sólo WFM | `CODE src/radio.c:687` |
| **RX EQ WFM p3** (1500 Hz) | `..._wfm_p3_set` (`control.h:146`) | `cfg.eq.rx_wfm.p3` | 5 bits (`CODE (SDK) src/control.c:635-639`). **`UNKNOWN`** | `0` (`cfg.c:411`) | `params` (`eq_rx_wfm_p3`) | sin gate | sólo WFM | `CODE src/radio.c:688` |
| **RX EQ WFM p4** (5000 Hz) | `..._wfm_p4_set` (`control.h:147`) | `cfg.eq.rx_wfm.p4` | 5 bits (`CODE (SDK) src/control.c:641-645`). **`UNKNOWN`** | `0` (`cfg.c:412`) | `params` (`eq_rx_wfm_p4`) | sin gate | sólo WFM | `CODE src/radio.c:689` |
| **RX EQ WFM p5** (12000 Hz) | `..._wfm_p5_set` (`control.h:148`) | `cfg.eq.rx_wfm.p5` | 5 bits (`CODE (SDK) src/control.c:647-651`). **`UNKNOWN`** | `0` (`cfg.c:413`) | `params` (`eq_rx_wfm_p5`) | sin gate | sólo WFM | `CODE src/radio.c:690` |

Filas: **12**.

## 5. Tabla C — controles de audio del BASE adyacentes (no son de la familia NR)

Están acá porque deciden **si algo suena**, que es la pregunta del issue; se marcan aparte para no confundirlos con
NR/NB/DNF/EQ.

| control | API/función | fuente del valor (cfg/UI/remoto) | rango sustentado en headers reales (archivo:línea) | default | persistencia (dónde) | estado OFF | efecto sobre CW/digital | evidencia |
|---|---|---|---|---|---|---|---|---|
| **Volumen del parlante** (registro 14) | `x6200_control_rxvol_set(uint8_t)` — SDK lo documenta como "Speaker volume" (`control.h:43`) | `radio_vol_value()` = `cfg.vol.val`, con 0 si `mute` o si el mute por auriculares A2DP está activo (`src/radio.c:792-802`); perilla `src/vol.c`, remoto `{"cmd":"vol"}` `src/remote_server.cpp`, CAT `src/cat.cpp:572-574` | `uint8_t` (`control.h:43`); la GUI acota con `VOL_MIN=0`/`VOL_MAX=100` (`src/radio.h:22-23`, `src/radio.c:797`). **Rango del BASE: `UNKNOWN`** (el CAT de esta GUI mapea 0..55: `src/cat.cpp:572-574`, y `docs/HALLAZGOS.md:965,4242` lo deja como no medido) | `20` (`cfg.c:327`) | `params` (`name='vol'`) | `0` = silencio del parlante | `UNVERIFIED` (es el nivel, no un filtro) | `CODE src/radio.c:792-802`; `CODE src/cfg/cfg.c:327` |
| **Ruteo record / mic-speaker** | `x6200_control_record_set(bool)` — "Toggle audio stream mic/speaker or RX/TX audio" (`control.h:44`) | **no** es un ajuste de usuario: lo escribe `audio_play_en()` mientras la GUI reproduce (`src/audio.c:378-387`), y el arranque lo fuerza a `false` (`src/radio.c:710`) | `bool` (`control.h:44`). Semántica exacta de cada valor: **`UNKNOWN`** (el header no la define) | `false` al arrancar (`src/radio.c:710`) | no persiste (estado transitorio) | `false` | `HYPOTHESIS`: podría afectar el audio de RX si el BASE queda latcheado en modo record (es la clase de falla de #244, mitigada por el arranque a `false`, `src/radio.c:707-710`) | `CODE src/audio.c:370-389`; `CODE src/radio.c:710` |
| **Speaker / Phone out** | `x6200_control_spmode_set(bool)` — "Toggle Speaker/Phone audio out" (`control.h:45`) | `params.spmode`: interruptor **Speaker mode** en Ajustes `src/dialog_settings.c:1720-1728` (callback) y `:1731-1760` (fila); `radio_change_spmode()` `src/radio.c:891-902` | `bool` (`control.h:45`). Qué salida física activa cada valor: **`UNKNOWN`** | `false` (`src/params/params.c:93`) | `params` (`name='spmode'`) | valor 0 = «Speaker mode» apagado (la UI lo rotula así, `src/dialog_settings.c:1739`) | `UNVERIFIED` | `CODE src/radio.c:711,891-902` |

Filas: **3**. Total de la matriz: **26 filas** (8 + 3 + 12 + 3).

## 6. Resumen de rangos: qué está sustentado y qué no

| Parámetro | Tipo en header | Ancho de campo (SDK) | Rango que aplica la GUI | Rango que aplica el remoto | Rango del BASE |
|---|---|---|---|---|---|
| `nr` (on) | `bool` `control.h:105` | bit 24 | 0/1 | 0/1 (`remote_server.cpp:767`) | no aplica |
| `nr_level` | `uint8_t` `control.h:106` | 8 bits | **0..100** (`mfk.cpp:499-505`) | **0..60** (`remote_server.cpp:13,774`) | **`UNKNOWN`** |
| `nb` (on) | `bool` `control.h:101` | bit 25 | 0/1 | 0/1 (`:766`) | no aplica |
| `nb_level` | `uint8_t` `control.h:102` | 8 bits | 0..100 | 0..100 (`:772`) | **`UNKNOWN`** |
| `nb_width` | `uint8_t` `control.h:103` | 8 bits | 0..100 | 0..100 (`:773`) | **`UNKNOWN`** (unidad tampoco sustentada) |
| `dnf` | enum 0..2 `low/control.h:153-157` | 2 bits | 0..2 (`%3`) | — (no expuesto) | **`UNKNOWN`** para el valor 3 |
| `dnf_center` | `uint16_t` `control.h:98` | 12 bits | 100..3000 | — | **`UNKNOWN`** |
| `dnf_width` | `uint16_t` `control.h:99` | 12 bits | 10..100 | — | **`UNKNOWN`** |
| `rx_filter.low/high` | `int16_t` `control.h:48-50` | 16 bits cada borde | del editor DFL / por modo | sólo `filter` (ancho) 50..6000 Hz (`:770`), no los bordes | **`UNKNOWN`** |
| `rx_eq.p1..p5` | `int8_t` `control.h:136-140` | 5 bits con signo | **−10..+10** (`dialog_eq.cpp:246`) | — | **`UNKNOWN`** |
| `rx_eq_wfm.p1..p5` | `int8_t` `control.h:144-148` | 5 bits con signo | −10..+10 | — | **`UNKNOWN`** |
| `rxvol` (reg 14) | `uint8_t` `control.h:43` | — | 0..100 (`radio.h:22-23`) | 0..100 | **`UNKNOWN`** |

**Por qué `UNKNOWN`**: el SDK sólo expone el tipo y el nombre del parámetro; el rango semántico (cuántos pasos,
qué escala, qué hace el BASE al recibir un valor fuera de rango) no está en los headers accesibles ni en la
documentación del fabricante que hay en el repo, y esta tarea no puede leerlo del equipo. Lo que sí está
sustentado: el tipo (`CODE`, header) y el ancho de bits (`CODE`, implementación del SDK en un checkout externo,
con la salvedad de versión de §0).

## 7. Tres cosas distintas que el issue pide no confundir

| Nombre en la UI / código | Qué es | Dónde vive | Cómo se controla | Evidencia |
|---|---|---|---|---|
| **NR** (`ACTION_NR_TOGGLE`, `MFK_NR`, `cfg.nr`) | Reducción de ruido **del BASE**, dentro del equipo; actúa sobre el audio que el operador escucha | registro 23 del BASE | toggle por tecla/F, MFK, CAT, remoto; nivel por `nr_level` | `CODE src/main_screen.c:526-530`; `CODE src/radio.c:675-676` |
| **Audio enhance** (`Off / CW weak / SSB voice / AM broadcast / Digital`) | Cadena **en la GUI** sobre la **copia de captura** (AF leveler + Speex NR + notch); no toca el BASE y su salida no se escribe en el stream de reproducción (`signal-flow.md` §5) | `src/audio_enhancement.c` | desplegable de Ajustes (`src/dialog_settings.c:1396-1416`), persistido en `params.audio_enh_preset` (`src/params/params.c:164`, `.max = 4`) | `CODE src/audio_enhancement.c:165-214`; ver `signal-flow.md` §1 y §4 |
| **Speex NR** (`audio_enh_nr_*`) | El motor concreto que usa el preset `SSB voice`/`AM broadcast` de *Audio enhance*: `speex_preprocess_run()` sobre frames de 256 | `src/audio_enhancement.c:293-318` | sólo a través del preset (no hay control directo en la UI) | `CODE src/audio_enhancement.c:151-158,188-202` |

`CODE` de que son independientes: `src/audio_enhancement.c` incluye `radio.h` (`:13`) pero **no** llama a ninguna
`x6200_control_*` (grep: 0 ocurrencias en el archivo) ni a `pa_*`/`snd_*`/`amixer` (0 ocurrencias). Y a la inversa,
el camino del NR del BASE (`src/radio.c:675`) no conoce `g_audio_enh`. El único punto de contacto es que **ambos** procesan el mismo audio de RX en momentos distintos: el
BASE antes de que el códec lo digitalice; Speex después, sobre la copia de la GUI.

Corolario verificable: apagar el **NR del BASE** no apaga el Speex, y elegir `Audio enhance = Off` no apaga el NR
del equipo.

## 8. `audio_enh_apply_mode_preset()` y la razón de evitar Speex NR en CW/CWR (PR #232)

- `CODE` **Qué hace**: lee el preset del usuario (`params.audio_enh_preset`) y el modo vigente
  (`cfg_cur.mode`); si el modo es `cw` o `cwr` y el preset **no** es `OFF` ni `DIGITAL`, aplica
  `AUDIO_ENH_PRESET_CW_WEAK` **sin modificar la preferencia guardada**
  (`src/audio_enhancement.c:331-345`). `CW_WEAK` deja el NR apagado y el leveler encendido
  (`src/audio_enhancement.c:180-186`).
- `CODE` **Cuándo corre**: al arrancar (`src/main.c:602`) y en cada cambio de modo (`src/dsp.cpp:293`,
  observer de `cfg_cur.mode`). **No** corre al cambiar el preset en Ajustes: ese camino llama directo a
  `audio_enh_preset_load()` (`src/dialog_settings.c:426-432`). Consecuencia de código, sin juzgar si es correcta:
  elegir `SSB voice` estando en CW aplica NR+leveler de voz hasta el próximo cambio de modo.
- `CODE` **La razón registrada (PR #232)** está en el comentario de `src/audio_enhancement.c:321-330`: en CW con
  preset `SSB_VOICE` se midió (en la radio) captura continua pero envolvente con **12.9 % de bloques por debajo de
  la mitad de la mediana**, y el decodificador perdía pulsos; el Recorder graba **después** del procesamiento
  (`src/audio.c:63` antes de `:68`). Este documento **no** juzga si el perfil CW es la respuesta correcta; sólo
  registra que el mecanismo es el de ese PR y que su efecto es sobre la copia de la GUI, no sobre el parlante
  (`signal-flow.md` §5).
- `CODE` **`AUDIO_ENH_PRESET_OFF` y `DIGITAL` se preservan en CW** (la condición los excluye,
  `src/audio_enhancement.c:337`): son intenciones explícitas de no procesar.

## 9. Verificado por código vs. no probado en hardware

| Afirmación | Clase | Qué la cerraría |
|---|---|---|
| Cada control de §2-§5 tiene un observer que escribe su registro con el valor del subject | `CODE` (`src/radio.c:669-690`, `:517-558`, `:634-635`) | — |
| Los tipos y el ancho de campo de cada parámetro | `CODE` (header + SDK externo, §0) | — |
| El rango **semántico** que acepta el BASE para `nr_level`, `nb_level`, `nb_width`, `dnf_*`, `rx_eq_*`, `rx_filter_*` | `UNKNOWN` | leer el registro después de escribir con valores extremos (el framing de lectura I2C está verificado: `docs/HALLAZGOS.md:4405` §69), o documentación del fabricante |
| Que un valor fuera de rango se recorte en el BASE en vez de dar la vuelta (bit masking del SDK) | `HYPOTHESIS` | mismo ensayo de lectura/escritura + audio audible |
| Que NR/NB/DNF/EQ del BASE mejoren o empeoren CW y modos digitales | `UNVERIFIED` | escucha A/B controlada (mismo VFO/volumen/filtro) y/o captura del parlante; el issue lo deriva al pipeline físico |
| Que `record_set(true/false)` y `spmode` cambien el audio audible | `UNVERIFIED` | prueba RX-only con la GUI reproduciendo algo conocido y medición de la salida |
| Que la lib `0.15.3` de la ROM corresponda al commit del SDK citado | `UNVERIFIED` | `strings`/símbolos del `.so` de la imagen, o build reproducible del paquete |
| Efecto de cada control sobre el **parlante** cuando hay auriculares A2DP conectados (el loopback y el mute se componen) | `CODE` para el mute (`src/btaudio.c:108-131`), `UNVERIFIED` para el resultado audible | medir con auriculares reales; la última corrida con hardware fue el 13/10 (`rootfs/usr/sbin/btaudio-ctl:136`) |

## 10. Fuera de esta matriz (para que no se lea como omisión)

- `x6200_control_mic_eq*` (`control.h:151-156`): EQ de **TX**, no de recepción. Mismo patrón de observers
  (`src/radio.c:692-697`).
- `x6200_control_monitor_level_set` (monitor de TX, `src/radio.c:723,884`), `key_vol`, `line_in/line_out`,
  `sql*`: afectan audio pero no son la familia NR/NB/DNF/EQ/filtro.
- El **notch de la GUI** (`audio_enh_notch_add`, `src/audio_enhancement.c:65-107`) no tiene llamador de
  producción (grep en `src/`: sólo definición y tests) ⇒ hoy no se ejercita.
- `docs/AUDIO_ENHANCEMENT_GUIDE.md` contiene cifras y comparaciones ("Comparable to mid-range professional rigs
  (ICOM IC-7300 level)", "~8-12 % CPU", "Matches or exceeds", "audio procesado llega al speaker") **sin medición
  que las respalde** en el repo: se toma como documentación aspiracional, no como evidencia (`docs/AUDIO_ENHANCEMENT_GUIDE.md:34,97-110,156`).

## 11. Reproducción (read-only)

```sh
git rev-parse HEAD                                    # a1c667ee...
grep -n "x6200_control_nr\|x6200_control_nb\|x6200_control_dnf\|x6200_control_rx_filter\|x6200_control_rx_eq" src/radio.c
grep -n "cfg.nr\|cfg.nb\|cfg.dnf\|eq.rx" src/cfg/cfg.c src/mfk.cpp src/controls.cpp src/main_screen.c
grep -n "nr_level\|nb_level\|nb_width" src/remote_server.cpp src/cat.cpp
grep -n "case MFK_NR_LEVEL" -A8 src/mfk.cpp
# SDK: copias fieles en el repo
diff -q tests/native/stubs_radio/aether_radio/x6200_control/control.h \
        tools/ui_harness/third_party/aether_radio/x6200_control/control.h
# ancho de campo (checkout externo del SDK, no versionado acá)
grep -n "x6200_control_nr_level_set\|x6200_control_dnf_set\|x6200_control_rx_eq_p1_set" \
     /home/pablo/desa/x6200/.x6200_ctl/src/control.c
```
