# UI AUDIT — cobertura de huecos con QEMU + capturas (épica #269, hijo de cierre)

Cierre de los **huecos de cobertura** que quedaron abiertos tras A (#270), B (#271) y C (#272).
El owner pidió **usar QEMU + capturas de pantalla** para completar el análisis. Esta corrida
**desbloquea el runner del banco de apps** (era el **P1** del hijo C), **captura los 3 diálogos
que el protocolo remoto no puede abrir** (entrada de frecuencia, mensajes CW y de voz), **mide los
estados de BT/WiFi** que hoy quedan en OFF y **documenta VISION y la barra legacy de 206 px**.

| | |
|---|---|
| Issue de cierre | `iariap/x6200_gui#269` (épica UI AUDIT); reúne los huecos del P1 de #272 |
| SHA base (congelado) | `e07ad61660643aa10dc45727e920ba6fd6044ba6` (= `origin/main`) |
| Worktree | `state/worktrees/x6200/ui-audit-gaps` |
| Rama | **`factory/ui-audit-gaps`** (sin push; el PR lo abre el orquestador contra `main`) |
| Fecha | 2026-10-23 |
| Evidencia | `docs/evidencia/2026-10-23-ui-audit/gaps/` |
| Capturas | **38** PNG nativos 800×480 (25 QEMU + 13 del arnés de host), sin escalar |
| Contenedores propios | `x6200-gaps-qemu` (efímero, `--rm`); volumen `x6200-gaps-build`; LAB `/tmp/x6200-gaps-qemu` |

> **Alcance.** Se tocó **infraestructura de tests** (`tests/qemu/run_apps_qemu.sh`,
> `tests/qemu/dialog-coverage.csv`, `tools/ui_harness/**`) y se agregó evidencia (`docs/**`).
> **No** se tocó `src/`, `rootfs/`, temas, settings persistidos, radio, RF ni firmware.

---

## 0. Resultado en una línea

| Hueco | Antes (C, #272) | Ahora | Cómo |
|---|---|---|---|
| Runner del banco de apps | **exit 1, 0 capturas** (P1) | **arranca y captura**: 25 PNG, PASS 12 / PARTIAL 6 / FAIL 7 / BLOCKED 0 | el contenedor ahora monta `/proc` y una DATA real (§1) |
| Entrada de frecuencia / mensajes CW / mensajes de voz | `UNSUPPORTED` (el protocolo no inyecta teclas) | **3 capturas `REAL`** de las ventanas reales | nuevo binario `ui_harness_dialogs` que linkea las fuentes reales y recibe el evento real (§2) |
| BT on/conectado, WiFi on/conectado | host en OFF; QEMU consulta el stack real (sin adaptador) | **8 capturas `SYNTHETIC`** de los tres estados visibles de cada icono + combinados | doble de `bt_state`/`wifi` inyectado por clave de escenario (§3) |
| VISION | no alcanzable | **1 captura `SYNTHETIC`** forzada + razón exacta | `theme_shape.vision` no tiene setter ni tema que lo fije (§4) |
| Barra legacy 206 px | no alcanzable | **1 captura `SYNTHETIC`** forzada + razón exacta | `params.theme` (THEME_LEGACY) no lo escribe ninguna UI (§4) |

**Sin defectos P0/P1 nuevos.** Lo medido vs lo hipotetizado va separado en §6.

---

## 1. El runner del banco de apps (P1 de C) — **arreglado**

### 1.1 Causa raíz (la misma que midió C)

El binario de la GUI está **endurecido** (#211–#215, r3): `src/params/db.c::data_mount_ok()` lee
`/proc/self/mountinfo` y aborta con **`EXIT_PERSISTENCE_UNAVAILABLE` (78)** si `/mnt` no es un
montaje real `rw` de un filesystem distinto de la raíz. El runner original arrancaba el banco así:

```sh
docker run -d --name "$CONT" --rm --entrypoint "" -v "$CHROOT:/t" "$IMAGEN" \
    sh -c "chroot /t /bin/busybox env LD_PRELOAD=... $BIN > /t/out-banco.txt 2>&1"
```

Ese contenedor **no trae `/proc`** (y el chroot monta un `/mnt` que no es un montaje real), así que
la GUI salía **78** y el barrido abortaba con *"el banco privado no arrancó"*: exit 1 y cero capturas.

### 1.2 Los dos cambios (diff en `tests/qemu/run_apps_qemu.sh`)

1. **`arrancar()`**: se agrega `--cap-add SYS_ADMIN` y, antes de la GUI, se monta
   `mount --bind /proc /t/proc` y `mount -t tmpfs -o size=64M tmpfs /t/mnt` sembrado con
   `params.default.db`. Es **la receta canónica del QA de #213** (`run-arm.sh`/`arm-inner.sh`).
   El aislamiento no cambia: contenedor, volumen, chroot, red y espacio de montaje siguen siendo
   **propios** de la corrida.
2. **`comprobar()`**: el `--check` ya **no exige que el chroot exista de antemano** cuando el
   volumen tiene la GUI: `armar_chroot` lo extrae de `$VOL/target` (lo que su propio mensaje
   prometía). Antes declaraba `FALTA` y el preflight salía **77** aunque el script supiera armarlo.

### 1.3 Verificación real (comandos y salida)

```sh
LAB=/tmp/x6200-gaps-qemu CONT=x6200-gaps-qemu VOL=x6200-gaps-build IMAGEN=x6200-buildroot \
  tests/qemu/run_apps_qemu.sh --check          # -> exit 0  (log: logs/runner_check.txt)
RONDAS=1 DWELL=2 LAB=... tests/qemu/run_apps_qemu.sh   # -> arranca y captura 25 PNG (logs/runner_rondas1.log)
```

Salida real del preflight (`logs/runner_check.txt`):

```
== todo listo para el barrido QEMU (no se tocó nada) ==
  contenedor privado: x6200-gaps-qemu
  volumen privado:    x6200-gaps-build
  laboratorio:        /tmp/x6200-gaps-qemu
  version del repo:   e07ad616 [UI AUDIT] Apps evidence (refs #272) (#273)
exit=0
```

Salida real de la corrida completa (`logs/runner_rondas1.log`), cola:

```
  ronda 1  SWRScan: PASS
  ronda 1  Propagation: PARTIAL
  ronda 1  DFLEditor: FAIL   FAIL: la app pedida no abrio (app='')
{"PASS": 12, "PARTIAL": 6, "FAIL": 7, "BLOCKED": 0}
== evidencia en /tmp/x6200-gaps-qemu/out (barrido.json, results.csv, index.txt, capturas/, logs/) ==
EXIT:1
```

**25 capturas reales** en `capturas/` (guardadas acá en `gaps/capturas/`). El runner sale **1** por
los **7 FAIL de contrato** — no por el entorno: son las mismas apps que C ya documentó (FT8/FT4/JS8/
WSPR recuerdan su dial; CW/CwTrainer fuerzan modo CW **por diseño**; DFLEditor es un panel inline
cuyo campo `app` queda vacío). **Ningún FAIL es nuevo** y **`BLOCKED` es 0**.

| App | Resultado | `app_estado` | Colores | Motivo (recortado) |
|---|---|---|---|---|
| FT8 | FAIL | FT8 | 176 | cambia freq 10100000→10136000 |
| FT4 | FAIL | FT4 | 177 | cambia freq 10100000→10140000 |
| JS8 | FAIL | JS8 | 249 | cambia mode USB→USB-D |
| WSPR | FAIL | WSPR | 227 | cambia freq 10100000→10138700 |
| SSTV | PASS | SSTV | 193 | app abierta, captura real, home restaurada |
| WeFax | PASS | WeFax | 180 | ídem |
| NavTex | PASS | NavTex | 185 | ídem |
| RTTY | PASS | RTTY | 176 | ídem |
| CW | FAIL | CW | 301 | cambia mode USB→CW (por diseño) |
| Field | PARTIAL | Field | 1856 | sin red (QRZ/POTA): solo el render |
| GPS | PARTIAL | GPS | 264 | sin receptor GNSS: mensaje controlado |
| Bands | PASS | Bands | 222 | app abierta, captura real, home restaurada |
| Settings | PASS | Settings | 381 | ídem |
| BT | PARTIAL | BT | 210 | sin adaptador: estado controlado |
| Cluster | PARTIAL | Cluster | 207 | sin red: sólo el render |
| CwTrainer | FAIL | CwTrainer | 307 | cambia mode USB→CW (por diseño) |
| Eq | PASS | Eq | 306 | app abierta, captura real, home restaurada |
| Status | PASS | Status | 223 | ídem |
| QTH | PASS | QTH | 242 | ídem |
| Callsign | PASS | Callsign | 241 | ídem |
| Recorder | PASS | Recorder | 179 | ídem |
| WiFi | PARTIAL | WiFi | 248 | sin NetworkManager ni adaptador |
| SWRScan | PASS | SWRScan | 215 | app abierta, captura real, home restaurada |
| Propagation | PARTIAL | Propagation | 189 | sin spots/NTP |
| DFLEditor | FAIL | (vacío) | 379 | panel inline, no diálogo (artefacto) |

### 1.4 La tabla de cobertura (`tests/qemu/dialog-coverage.csv`) queda al día

Con el barrido corriendo, las 22 filas dejan de ser `BLOCKED` y pasan a su cobertura **medida**
(`REAL`/`PARTIAL`, 13 + 6) con la captura de la corrida; las 3 que el protocolo no puede abrir siguen
**`UNSUPPORTED`**, ahora con la razón ampliada (mencionan que **el arnés de host sí las renderiza**).
`python3 tests/check_dialog_coverage.py` → **OK** (exit 0).

---

## 2. Los 3 diálogos `UNSUPPORTED` por tecla — ahora **`REAL`** (vía arnés de host)

### 2.1 Por qué C los marcó `UNSUPPORTED`

El **protocolo de control remoto no inyecta teclas**, y las tres ventanas se abren con una tecla
física:

| Diálogo | Fuente real | Se abre por |
|---|---|---|
| **Entrada de frecuencia** | `src/dialog_freq.c` | `HKEY_FINP` (`hkey.c` → `events.c::event_send_key` → `LV_EVENT_KEY` al espectro enfocado → `spectrum_key_cb` → `dialog_construct(dialog_freq)`) |
| **Mensajes CW** | `src/dialog_msg_cw.c` | tecla `MSG` con la radio en **CW/CW-R** (`keypad.c` → `EVENT_KEYPAD` → `main_screen_keypad_cb`) |
| **Mensajes de voz** | `src/dialog_msg_voice.c` | tecla `MSG` con la radio en **LSB/USB/AM/NFM** (ídem) |

### 2.2 El camino que sí inyecta la tecla (y por qué hacía falta un binario nuevo)

El arnés de host **sí** puede empujar esos eventos (`tools/ui_harness/src/screens.c` ya tenía
`dfl_keypad_ui()`, que manda `EVENT_KEYPAD` a la pantalla). Pero **los tres descriptores eran
inertes** (`mocks/mock_main_screen.c`: `dialog_t *dialog_freq = &mock_dialog;`): abrir la ruta no
renderizaba ninguna ventana. Promover el binario por defecto a `REAL` rompería la invariante de la
épica (`tests/check_dialog_coverage.py`: *"el harness de host sigue siendo sintético para ellas y su
etiqueta de cobertura NO se puede promover a REAL"*).

La solución acotada es un **segundo target**, `ui_harness_dialogs`, que compila **las mismas**
fuentes/LVGL pero **linkea las ventanas reales**:

* `tools/ui_harness/CMakeLists.txt`: nuevo target con `-DHARNESS_REAL_DIALOGS=1`, que linkea
  `src/dialog_freq.c`, `src/dialog_msg_cw.c`, `src/dialog_msg_voice.c`;
* `mocks/mock_main_screen.c`: los 3 descriptores quedan bajo `#ifndef HARNESS_REAL_DIALOGS` — el
  binario por defecto los conserva (y la tabla de cobertura sigue siendo la de la épica);
* `mocks/dialogs/mock_dialog_deps.c`: **solo los servicios que no son UI** de esas ventanas (keyer
  CW, ventana de texto, store `params_msg_cw_*`, `audio_play*()`);
* `mocks/dialogs/sndfile.h` + `sndfile_stub.c`: la imagen de build (`x6200-uiharness`, stage `ci`)
  **no trae libsndfile**, y la ventana de voz incluye `<sndfile.h>`; el stub es un no-op del backend
  de WAV (nunca se abre/lee/escribe un archivo). Es un mock de una librería ausente, no una
  reimplementación;
* `src/screens.c`: pantallas `dialog-freq` / `dialog-msg-cw` / `dialog-msg-voice` que **empujan el
  evento real** en `scenario_settle` (después de que corran los timers, para que el grupo de teclado
  y el espectro enfocado existan) y **cortan** (`exit 1`) si la ventana no se construyó
  (`dialog_*->obj == NULL`).

Con el **binario por defecto**, esos fixtures **salen 1** con el mensaje *"la ventana real no se
construyó (descriptor inerte): esta captura necesita el binario ui_harness_dialogs"* — probado.

### 2.3 Formato obligatorio por captura

| Pantalla | Cobertura | Cómo se abrió (evento real) | `exit` | PNG | Fuente |
|---|---|---|---|---|---|
| Frequency entry | **REAL** | `LV_EVENT_KEY` + `HKEY_FINP` al espectro enfocado | 0 | `png/dialog-freq.png` | `src/dialog_freq.c` |
| CW messages | **REAL** | `EVENT_KEYPAD` `KEYPAD_MSG` + `KEYPAD_RELEASE`, modo CW | 0 | `png/dialog-msg-cw.png` | `src/dialog_msg_cw.c` |
| Voice messages | **REAL** | `EVENT_KEYPAD` `KEYPAD_MSG` + `KEYPAD_RELEASE`, modo USB | 0 | `png/dialog-msg-voice.png` | `src/dialog_msg_voice.c` |

Comando exacto (una captura), con `SHA` = base congelada:

```sh
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
  -v "<worktree>:/src" -v "<scratch>:/scratch" -w /src/tools/ui_harness/build \
  x6200-uiharness -lc "./ui_harness_dialogs \
    --scenario /src/tools/ui_harness/fixtures-gaps/dialog-freq.scn \
    --png /scratch/dialog-freq.png --json /scratch/dialog-freq.json --inspect"
```

Lo que se ve: la entrada de frecuencia dibuja el `lv_textarea` con el placeholder **`Freq in MHz`**
centrado; los mensajes CW y de voz dibujan su tabla (vacía, con el store mockeado) y la barra
`(MSG 1:2)` / `Send` / `Beacon` / `Beacon Period`. En las dos últimas **la home se ve a través** de
la superficie de la ventana: es la **translucidez** que C reportó como **P2-1** (el `dialog_style` no
fija `bg_color`/`bg_img_src`), reproducida acá de forma independiente.

### 2.4 QEMU sigue `UNSUPPORTED` (y es correcto)

El barrido QEMU **no cambió su veredicto** para estos tres: sigue sin poder abrirlos (el protocolo no
inyecta teclas). La cobertura `REAL` es **del arnés de host**, no del dispositivo; la tabla
`dialog-coverage.csv` lo dice así (y su columna `host_status` **sigue `INERT`**, como manda la
invariante: el binario por defecto no las promueve).

---

## 3. BT encendido/conectado y WiFi activo — **`SYNTHETIC`**

Hoy el arnés fijaba BT y WiFi en **OFF**. Se extendió el doble (`mocks/mock_ui_deps.c`) para inyectar
el estado por **clave de escenario**, y se capturaron **todos los estados visibles** de los dos
iconos de la barra (`src/info.c`): apagado → `icon_off`, prendido → `text_dim`, conectado → `text`.

| Fixture | `bt_state` | `wifi_status` | Icono BT (medido) | Icono WiFi (medido) | PNG |
|---|---|---|---|---|---|
| `net-bt-off` | off | off | `icon_off` (39 px) | `icon_off` (27 px) | `png/net-bt-off.png` |
| `net-bt-on` | on | off | `text_dim` (24 px) | `icon_off` (27 px) | `png/net-bt-on.png` |
| `net-bt-connected` | connected | off | `text` (24 px) | `icon_off` (27 px) | `png/net-bt-connected.png` |
| `net-wifi-off` | off | off | `icon_off` | `icon_off` | `png/net-wifi-off.png` |
| `net-wifi-on` | off | **on** | `icon_off` | **`text_dim`** | `png/net-wifi-on.png` |
| `net-wifi-connected` | off | **connected** | `icon_off` | **`text`** | `png/net-wifi-connected.png` |
| `net-bt-wifi-connected` | connected | connected | **`text`** | **`text`** | `png/net-bt-wifi-connected.png` |
| `net-bt-wifi-on` | on | on | **`text_dim`** | **`text_dim`** | `png/net-bt-wifi-on.png` |

Colores = los tres roles de Aurora: `icon_off #3A2D50`, `text_dim #D5C4E0`, `text #F8EEFF`
(tabla completa con los conteos por píxel: `tablas/iconos-bt-wifi.csv`). El fixture **corta** si el
color dibujado no es el que la regla de `src/info.c` pide para el estado inyectado
(`scenario_settle`), así que no puede reportar verde con el icono equivocado.

### 3.1 Cómo se construyó el doble

* `mocks/mock_ui_deps.c`: `bt_state_on/discoverable/connected/mute()` y `wifi_get_status()` leen un
  estado inyectable (`mock_bt_state_set` / `mock_wifi_state_set`, valores `off|on|connected`); el
  default sigue siendo OFF, así que **ningún fixture existente cambia de píxeles** (§8).
* `src/scenario.c`: claves `bt_state` y `wifi_status` (un valor desconocido es error, nunca se
  ignora).
* `src/screens.c`: la **única** diferencia de comportamiento es que, cuando el fixture fija un
  `wifi_status`, se manda **una vez** `MSG_WIFI_STATE_CHANGED` después de construir la pantalla. Es
  el mismo aviso que manda la radio real al cambiar de estado (`src/wifi.cpp`), que el arnés no
  compila: sin él el icono de WiFi queda en su color de arranque y no refleja `wifi_get_status()`.
  Sin la clave, no se manda: los fixtures de siempre quedan byte a byte iguales.

### 3.2 Nota honesta: `net-wifi-on` coincide con `home-usb-20m`

El color de arranque del icono de WiFi (`src/info.c` lo inicializa en `text_dim`) **coincide** con el
estado "prendido": forzar `wifi_status on` rinde el mismo PNG que `home-usb-20m` (sin claves).
Es un hecho del código, no un error: en la radio el `MSG_WIFI_STATE_CHANGED` de arranque corrige el
icono; en el arnés lo manda el fixture. `net-wifi-off` y `net-bt-off` también comparten PNG (los dos
están apagados).

### 3.3 ¿Y en QEMU?

No se hizo **bluez/NM falsos por D-Bus**: el binario endurecido consulta el stack real (sin
adaptador ⇒ OFF) y montar un stack falso es un doble de sistema fuera del alcance de esta corrida
(los recursos privados del QEMU son para el **banco de apps**). El resultado de BT/WiFi en QEMU sigue
`PARTIAL`/OFF (foto del estado controlado) y **no se reclama lo contrario**.

---

## 4. VISION y la barra legacy de 206 px — razón exacta + forzado **`SYNTHETIC`**

### 4.1 VISION: el firmware **no puede entrar**

`theme_shape.vision` (`src/theme.h:196`) decide el layout VISION (fuentes +3 pasos con
`fonts_scale(VISION_FONT_STEPS)`, `src/main.c:628`; celdas de info reescaladas, `src/info.c`). **No
tiene setter**: `src/theme.c` sólo parsea un subconjunto de la forma del tema con `get_int`/`get_bool`
(radio, bordes, gradientes, `mode_box`, `rule`, `waterfall*`, `font*`, `mark*`) y **`vision` no está
en esa lista**; ninguna de las seis formas base de `base_shapes[]` lo pone en `true`; y los seis temas
JSON (`rootfs/usr/share/x6200/themes/*.json`) no lo traen. Conclusión medida: **en runtime no hay
forma de alcanzar VISION**. A y B lo habían medido; acá se confirma leyendo las tres fuentes.

**Captura forzada (declarada SYNTHETIC):** el arnés lo enciende por clave (`vision true`) **antes de
`styles_init()`**, en el mismo orden que `src/main.c`, y rinde `png/vision-home.png`. Sirve para ver
el layout (fuentes escaladas). **No** prueba que la radio lo pueda seleccionar. De hecho el propio
repo documenta que bajo VISION `VFO-A` se parte y `CW-R`/`AUTO` se pegan (UI-012/UI-014): por eso
este fixture vive en `fixtures-gaps/` y **no** entra al corpus que evalúan los contratos (`§7`).

### 4.2 La barra legacy de 206 px: variante de estilo **no seleccionable en runtime**

El ancho de la barra de info es **206 px** en `src/styles.c::setup_theme_legacy()` (bitmaps
`images/top_short.bin`, claros) y **209 px** en `setup_theme_simple()` (bitmaps `*_dark.bin`). Cuál
corre lo elige **`params.theme`** (`src/main.c:637`, `styles_init(params.theme.x)`), un
`params_uint8_t` cuyo default es `THEME_SIMPLE`. **Ninguna UI escribe ese parámetro**: no hay
`params_uint8_set`/`params_uint8_change` sobre `params.theme`, ni una fila de Ajustes que lo toque
(la fila "Theme" del menú escribe `params.theme_name` — los seis temas v2 —, que es otra cosa).
Conclusión medida: **`THEME_LEGACY` (la barra de 206 px) no es seleccionable en runtime**; sólo se
llega editando `params.db`.

**Captura forzada (declarada SYNTHETIC):** el arnés elige el estilo por clave (`bar_theme legacy`) y
rinde `png/bar-legacy-home.png` (barra de 206 px + bitmaps claros). **No** prueba que la radio la
pueda elegir. El `_Static_assert` de `src/ui_tokens.h` ya fija que la rejilla de celdas entra en los
206 px, así que la barra no recorta una celda.

---

## 5. Manifiesto resumido

`manifest.csv` tiene **38 filas** con las 14 columnas de la épica (`screen, state, theme,
source_coverage, sha, command, exit_code, png, width, height, issue_id, severity, source_path,
observation`). `mediciones/medidas.csv` trae, por PNG, las medidas PIL (dimensiones, colores,
contraste fondo/primer plano, fracción de píxeles ≠ fondo, bbox del contenido, sha256).

| Grupo | Capturas | Cobertura | Severidad |
|---|---|---|---|
| Barrido QEMU (25 apps) | 25 | **REAL** 18 · **PARTIAL** 7 · **BLOCKED** 0 | P3 |
| Diálogos por tecla | 3 | **REAL** | P3 |
| BT/WiFi inyectado | 8 | **SYNTHETIC** | P3 |
| VISION / barra legacy (forzadas) | 2 | **SYNTHETIC** | P3 |
| **Total** | **38** | **REAL 21 · PARTIAL 7 · SYNTHETIC 10 · UNSUPPORTED 0** | todas P3 |

> Las 7 `PARTIAL` son las funciones que necesitan un servicio/periférico que el chroot no tiene
> (WSPR, Field, GPS, Cluster, **BT**, WiFi, Propagation) — la misma clasificación que el inventario
> de C (#272) y que `tests/qemu/dialog-coverage.csv`. Las apps cuyo **contrato de estado dio FAIL**
> pero cuya ventana no depende de la red quedan **REAL** (FT8/FT4/JS8/CW/CwTrainer/DFLEditor): lo
> que se clasifica es la **cobertura de la ventana** (se capturó), y el veredicto PASS/FAIL con su
> motivo está en `logs/barrido.json`. No hay ningún PASS falso: una función que no se puede validar
> **nunca** se declara REAL.

---

## 6. Medido vs hipótesis

**Medido (hechos, con su artefacto):**

* El runner arranca el banco endurecido con `/proc` + DATA real: `--check` exit 0 y 25 capturas,
  `BLOCKED 0` (`logs/runner_check.txt`, `logs/runner_rondas1.log`, `logs/barrido.json`).
* Las 3 ventanas reales renderizan por el evento real del firmware (`png/dialog-*.png`), y el
  binario por defecto las rechaza (exit 1 con mensaje) — el fail-closed es real.
* Los tres estados visibles de BT y de WiFi pintan los roles `icon_off`/`text_dim`/`text` — medido
  por píxel (`tablas/iconos-bt-wifi.csv`) y por el árbol de objetos (`--inspect`).
* VISION no tiene setter ni tema que lo fije; `params.theme` no lo escribe ninguna UI (leído en
  `src/theme.c`, `src/main.c`, `src/params/params.h`, `src/styles.c`).
* Los renders de fixtures **existentes** son byte a byte idénticos a la evidencia ya mergeada de B
  (`logs/regresion.txt`), y los 13 fixtures nuevos son deterministas (`logs/determinismo.txt`).

**Observaciones (no defectos nuevos, contexto para el owner):**

* La **translucidez** de los diálogos sobre la home (C **P2-1**) se reproduce en las capturas de
  mensajes CW y de voz: texto de la home visible a través de la ventana.
* `net-wifi-on == home-usb-20m`: el color de arranque del icono de WiFi coincide con "prendido" (§3.2).
* Bajo VISION forzado se ven los defectos **conocidos** UI-012 (VFO-A partido) y UI-014 (CW-R pegado
  a AUTO) — no se reportan como nuevos: ya están en `docs/UI_KNOWN_ISSUES.md`.

**Hipótesis (no medidas, marcadas como tales):** la legibilidad física en el LCD de 4" y el
comportamiento real de BT/WiFi con un adaptador: requires-radio / requires-hardware.

---

## 7. Defectos P0/P1 nuevos y huecos que quedan

**P0 nuevos: 0. P1 nuevos: 0.** El único P1 abierto era el runner de C y está cerrado (§1).

Huecos que siguen abiertos (con su razón):

| Hueco | Estado | Razón |
|---|---|---|
| BT/WiFi reales en QEMU | **no cubierto** | exigiría bluez/NetworkManager falsos por D-Bus; el binario consulta el stack real (§3.3) |
| VISION como estado real | **UNSUPPORTED** | `theme_shape.vision` sin setter ni tema; lo capturado es forzado (SYNTHETIC) |
| Barra legacy 206 px como estado real | **UNSUPPORTED** | `params.theme` sin UI que lo escriba; forzado (SYNTHETIC) |
| Subpáginas/sesiones de cada app (SWRScan multipasada, FT8 RX, Eq, etc.) | **fuera de alcance** | el contrato del barrido abre, permanece `DWELL` y cierra; no recorre subpáginas |
| Legibilidad/táctil/térmica/consumo en el panel físico | **BLOCKED** | requires-hardware: una captura no lo mide |
| GPS/Cluster/Field/Propagation/WSPR/WiFi y BT (función) | **PARTIAL** | sin red/periférico: se ve el render y el estado controlado, no la función |

---

## 8. Determinismo y no-regresión (hecho probado)

* **13/13 fixtures nuevos deterministas** (mismo PNG en dos corridas; `logs/determinismo.txt`).
* **3/3 fixtures existentes byte a byte idénticos** a la evidencia mergeada de B (#271):
  `home-usb-20m` (= el control de B), `settings-home`, `bt-dialog` (`logs/regresion.txt`). Es la
  prueba de que el doble inyectable, las claves nuevas y el mensaje de WiFi **no tocan** los renders
  que ya estaban.
* El cambio de mock mantiene el set de descriptores inertes: `python3 tests/check_dialog_coverage.py`
  → **OK**.

### Por qué los fixtures nuevos viven en `tools/ui_harness/fixtures-gaps/`

`x6200-ab contracts` y `tests/check_harness_determinism.sh` recorren **todo** `fixtures/*.scn`. Los
fixtures nuevos no deben cambiar el veredicto de esos gates (VISION, por ejemplo, **produce a
propósito** los defectos conocidos UI-012/UI-014, y el binario por defecto rechaza los 3 diálogos).
Por eso se agrupan en **`fixtures-gaps/`**, que se corre con la ruta explícita y **no** entra al
corpus de CI. El blast radius queda acotado: los gates existentes no ven un fixture nuevo.

---

## 9. Reproducción

```sh
WT=/home/pablo/desa/asf/state/worktrees/x6200/ui-audit-gaps
LAB=/tmp/x6200-gaps-qemu CONT=x6200-gaps-qemu VOL=x6200-gaps-build IMAGEN=x6200-buildroot
```

```sh
# 1) preflight y barrido QEMU del runner del repo (25 apps, 1 ronda)
LAB=$LAB CONT=$CONT VOL=$VOL IMAGEN=$IMAGEN sh "$WT/tests/qemu/run_apps_qemu.sh" --check     # exit 0
RONDAS=1 DWELL=2 LAB=$LAB CONT=$CONT VOL=$VOL IMAGEN=$IMAGEN sh "$WT/tests/qemu/run_apps_qemu.sh"

# 2) binarios del arnes de host (por defecto + dialogos)
REV=$(git -C "$WT" rev-parse HEAD)
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" -v "$WT:/src" \
  -w /src/tools/ui_harness x6200-uiharness \
  -lc "mkdir -p build && cd build && cmake .. -DHARNESS_REVISION=$REV && make -j8"

# 3) las 13 capturas del arnes de host
F=/src/tools/ui_harness/fixtures-gaps
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" -v "$WT:/src" -v "<scratch>:/scratch" \
  -w /src/tools/ui_harness/build x6200-uiharness -lc "
    for f in dialog-freq dialog-msg-cw dialog-msg-voice; do
      ./ui_harness_dialogs --scenario $F/\$f.scn --png /scratch/\$f.png --json /scratch/\$f.json --inspect; done
    for f in net-bt-off net-bt-on net-bt-connected net-wifi-off net-wifi-on net-wifi-connected \
             net-bt-wifi-connected net-bt-wifi-on vision-home bar-legacy-home; do
      ./ui_harness --scenario $F/\$f.scn --png /scratch/\$f.png --json /scratch/\$f.json --inspect; done"

# 4) manifiesto + mediciones
python3 "$WT/docs/evidencia/2026-10-23-ui-audit/gaps/manifiesto.py"
python3 "$WT/docs/evidencia/2026-10-23-ui-audit/gaps/medir_iconos.py"

# 5) gates
python3 "$WT/tests/check_dialog_coverage.py"
```

> Los bitmaps de estilo (`images/*.bin`) en `src/styles.c` se reescriben a `A:<repo>/` al compilar
> (`CMakeLists.txt`), así que **todas las capturas del arnés salen de dentro del contenedor**; fuera
> de él LVGL no lee los bitmaps y cambia los colores.

---

## 10. Estructura del directorio

```text
docs/evidencia/2026-10-23-ui-audit/gaps/
  README.md                 este informe
  manifest.csv              38 filas, las 14 columnas de la épica
  manifiesto.py             genera manifest.csv + mediciones/medidas.csv (PIL)
  medir_iconos.py           mide el rol de color de los iconos BT/WiFi por estado
  capturas/                 25 PNG REALES del barrido QEMU (800x480, sin escalar)
  png/                      13 PNG del arnés de host + sus 13 JSON (--inspect)
  mediciones/medidas.csv    colores/contraste/bbox/sha256 por PNG
  tablas/iconos-bt-wifi.csv medición por píxel de los 8 estados de BT/WiFi
  tablas/control/           home-usb-20m.png (control de no-regresión, = B)
  logs/                     runner_check.txt, runner_rondas1.log, barrido.json,
                            results.csv, index.txt, intentos.jsonl,
                            determinismo.txt, regresion.txt
```

Cambios de infraestructura (fuera de `docs/`):

```text
tests/qemu/run_apps_qemu.sh        monta /proc + DATA real; --check acepta armar el chroot
tests/qemu/dialog-coverage.csv     las 22 filas pasan a la cobertura medida (REAL/PARTIAL/UNSUPPORTED)
tools/ui_harness/CMakeLists.txt    target ui_harness_dialogs (-DHARNESS_REAL_DIALOGS)
tools/ui_harness/src/screens.c     pantallas dialog-* / settle_net_state + assert de iconos
tools/ui_harness/src/scenario.{c,h} claves bt_state / wifi_status / vision / bar_theme
tools/ui_harness/src/coverage.c    tabla de cobertura de las pantallas nuevas
tools/ui_harness/src/harness_main.c VISION forzado + variante de estilo
tools/ui_harness/mocks/mock_ui_deps.c doble BT/WiFi inyectable
tools/ui_harness/mocks/mock_main_screen.c 3 descriptores bajo #ifndef HARNESS_REAL_DIALOGS
tools/ui_harness/mocks/dialogs/    stubs de los servicios no-UI + <sndfile.h> stand-in
tools/ui_harness/fixtures-gaps/    13 fixtures nuevos (no entran al corpus de CI)
```

---

## 11. Lo que este PR **no** hace

No modifica `src/`, `rootfs/`, temas, `ui_tokens.h`, settings persistidos, radio, RF ni firmware; no
usa SSH, flash, deploy, TX/RF ni `sudo`; no toca la radio física; no toca recursos compartidos del
owner (contenedor/volumen/lab propios, efímeros); no mergea (lo revisa e integra el orquestador); no
declara validación en hardware ni ROM. No propone rediseño.
