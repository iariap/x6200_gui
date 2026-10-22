# WiFi ocioso: pump de GLib sin wakeups inútiles — evidencia de la corrida

Corrida de implementación de **`iariap/x6200_gui#218`** (`[PROMPT][NEXT][P2] Reducir timer WiFi
ocioso conservando NetworkManager`), hijo **2/6** de la épica **#216** (F-06 del dossier #194).

| | |
|---|---|
| Worktree | `/home/pablo/desa/asf/state/worktrees/x6200/next-218` (privado; único escritor) |
| Rama | `factory/next-218` |
| Base | `b798f13b` = tip de `origin/lu2dsx/x6200-rom` al arrancar (incluye el hardening de #211) |
| HEAD del código | `93fbce45c5e940b44eeb22db4a66d1cd0aad2a38` |
| Tip de la rama | el commit de este reporte (código `93fbce45` + esta documentación), sin push |
| Push | **ninguno**: el PR lo abre el orquestador contra `lu2dsx/x6200-rom`, título `[P2][WiFi] Reduce idle GLib pump wakeups`, `Refs #216` + `Closes #218` |
| Alcance | **sólo `src/wifi.cpp` + pruebas** (contrato del issue). Ningún otro archivo de producción se tocó |

> ## **Hardware pendiente — PENDING_HARDWARE**
> Todo lo de abajo es **HOST**. No se usó la radio: **no** hubo SSH, flash, `sudo`, deploy, TX/RF
> ni SD del owner. Lo medido es el **número de vueltas del pump de GLib de este timer** y el
> comportamiento del módulo contra un doble de NetworkManager; **la energía real (vatios, mA),
> el pin físico y la WiFi verdadera no se midieron** y quedan pendientes de instrumento. Este
> documento **no** afirma ahorro energético ni mejora de batería.

## 1. Qué se cambió (contrato #218)

`src/wifi.cpp::wifi_power_setup()` creaba el `loop_timer` con 30 ms **incondicionalmente**. Ese
timer es el **único pump de GLib** de la GUI: su callback
(`loop_iterations_cb()` → `g_main_context_iteration()`) es quien despacha las callbacks async de
NetworkManager, incluidas las que llegan **después** de apagar la radio. Por eso el timer se
conserva; lo único que cambia por estado es **su periodo**.

| dónde | antes | después |
|---|---|---|
| `wifi_power_setup()` | `lv_timer_create(loop_iterations_cb, 30, NULL)` | crea con el periodo del estado inicial (`params.wifi_enabled`: 30 ms ON / 250 ms OFF) y lo **sincroniza al final** |
| `wifi_power_on()` | — (no tocaba el timer) | tras params/pin/estado/mensaje: `lv_timer_set_period(loop_timer, 30)` con guarda `if (loop_timer)` |
| `wifi_power_off()` | — (no tocaba el timer) | tras estado/pin/cancelar scan: `lv_timer_set_period(loop_timer, 250)`. **El timer NO se borra** (las desconexiones tardías lo necesitan) |
| callback GLib, red, pin, API, scan | — | **sin cambios** (el diff toca sólo esos tres lugares + dos `#define`) |

`LOOP_PERIOD_ON_MS 30` / `LOOP_PERIOD_OFF_MS 250` son constantes con nombre, con el porqué del
250 ms en el comentario del fuente (donde no hay adaptador ni scan que atender). No se agregaron
settings, modos ECO, hilos ni cronómetros.

## 2. La medición: antes / después

Instrumento: el **mismo test** (y el mismo fixture) compilado contra `git show <ref>:src/wifi.cpp`
y contra el árbol de trabajo. El fixture avanza un **reloj virtual** (sin reloj de pared) y
**cuenta las llamadas a `g_main_context_iteration()`** —cada una es un wakeup del pump— en una
ventana de **10 s con la WiFi ociosa** (sin scan pendiente). Es **sólo este timer**: no incluye el
tick de LVGL, el DSP ni la pantalla.

Reproducir:

```
sh tests/native/run_wifi_idle_timer.sh --ab origin/lu2dsx/x6200-rom
```

```
== numeros (vueltas del pump de GLib en 10000 ms de WiFi ociosa) ==
  arbol de trabajo (HEAD):          OFF (periodo 250 ms):  40 vueltas ( 4,0/s)   ON (periodo 30 ms): 333 vueltas (33,3/s)
  origin/lu2dsx/x6200-rom (antes):  OFF (periodo  30 ms): 333 vueltas (33,3/s)   ON (periodo 30 ms): 333 vueltas (33,3/s)
```

| estado | antes | después | cambio |
|---|---|---|---|
| WiFi **OFF** ociosa | 333 / 10 s (33,3/s) | **40 / 10 s (4,0/s)** | **−88,0 %** (−293 vueltas cada 10 s) |
| WiFi **ON** ociosa | 333 / 10 s (33,3/s) | 333 / 10 s (33,3/s) | 0 (el ritmo de la radio encendida no cambia) |
| timers vivos / creados | 1 / 1 | 1 / 1 | igual (el timer nunca se borra ni se duplica) |

Los números salen de la aritmética del propio fixture y del periodo real del timer
(`10000/periodo`): el instrumento **falla** si el periodo no es el del contrato, así que no
puede reportar un ahorro que el código no tenga. La corrida completa de la medición, con las
aserciones del "antes" en rojo (esperado), está en
[`medicion-ab.txt`](medicion-ab.txt).

**Determinismo del instrumento:** dos corridas dan salida byte a byte idéntica
([`lab/determinismo-instrumento.txt`](lab/determinismo-instrumento.txt), sha256
`a6d2d0dc…`).

**Lo que este número NO es:** no es el perfil de wakeups del sistema ni del lazo de la GUI del
harness (que sigue igual: `14 wakeups/1000 ms` con el drive,
[`gates-wakeups.txt`](gates-wakeups.txt)), y **no** es ahorro en vatios (PENDING_HARDWARE).

## 3. NetworkManager sigue igual (constancia)

El diff **no toca** ningún callback, señal, pin ni API: sólo el periodo. Y el test lo prueba por
comportamiento, no por inspección (597 checks, 0 fallos; el detalle de cada escenario está en el
fuente `tests/native/test_wifi_idle_timer.cpp`):

| escenario | qué verifica | resultado |
|---|---|---|
| OFF al inicio | estado visible `WIFI_OFF`, pin en 1, `params.wifi_enabled=false`, pump vivo | OK |
| ON al inicio | pin en 0, params true, `WIFI_STARTING` sin adaptador, pump a 30 ms | OK |
| **100 ciclos ON/OFF** | un **solo** timer creado en toda la corrida, periodo 250/30 alternando, sin duplicados ni leaks, 1 escritura de pin por transición | OK |
| **scan pendiente al OFF** | `wifi_start_scan()` encola en NM y la respuesta llega **por el pump**; al apagar se cancela el scan (`scan_timer` borrado, `scanning=false`) y el pump queda a 250 ms; al reencender, `device-added` reengancha el device y el scan se reanuda y termina por su timer de 500 ms | OK |
| **desconexión tardía** | una callback post-OFF se despacha en la **primera** vuelta del pump, y correr 10 s más **no** reenciende la radio: 0 escrituras de encendido, pin en 1, params false | OK |
| **estado visible** | `ACTIVATED`→`WIFI_CONNECTED`, `FAILED/NO_SECRETS`→`"Wrong WiFi password"`, `DISCONNECTED`→`WIFI_DISCONNECTED`, avisos al bus | OK |
| sin adaptador NM | arranca igual (el `g_signal_connect(NULL,…)` preexistente se declara, no se "arregla"), error+timer+pump | OK |

La prueba más fuerte de que NM no cambió es la corrida **"antes"**: de sus **110 fallos**, los
110 son aserciones de periodo/wakeups de OFF (`wifi_fx_timer_period(loop_timer) == 250` ×104,
`g_off_wakeups == 40` ×1, `idle_wakeups(...) == 40` ×3, `pump_once(250) == 250` ×1,
`g_on_wakeups > g_off_wakeups` ×1). **Ninguna** de las aserciones de NetworkManager, del scan,
de la desconexión tardía o de la contabilidad de timers falla en el código de la base: esas
partes del comportamiento ya eran las que son y siguen siéndolo.

## 4. Gates (salida real)

Todos corridos sobre `93fbce45` (el commit con el código y las pruebas). Los logs completos están
en este directorio.

| gate | comando | resultado | log |
|---|---|---|---|
| Suite nativa (host) | `sh tests/native/run_native_tests.sh` | **OK** (exit 0; `597 comprobaciones, 0 fallos` del stage nuevo) | [`gates-native.txt`](gates-native.txt) |
| Paridad con CI | `sh tests/native/run_native_tests.sh --ci-parity` | **OK** — `ci-parity: OK (build host sin lvgl/)` | [`gates-ci-parity.txt`](gates-ci-parity.txt) |
| Suite completa (rápida) | `sh tests/run_all_tests.sh --quick` | **OK** — `19 OK, 0 fallos` (+ 4 SKIP con razón) | [`gates-run-all-tests.txt`](gates-run-all-tests.txt) |
| Determinismo del harness | `sh tests/check_harness_determinism.sh` | **PASS** — PNGs idénticos por fixture | [`gates-determinism.txt`](gates-determinism.txt) |
| Perfil natural | `sh tests/check_harness_natural.sh` | **PASS** — 11.208.000 px en 66 flushes / 40 frames | [`gates-natural.txt`](gates-natural.txt) |
| Waterfall incremental | `sh tests/check_waterfall_incremental.sh` | **PASS** — 195 checks, 0 fallas (+ ASan/UBSan) | [`gates-waterfall.txt`](gates-waterfall.txt) |
| Wakeups del lazo | `sh tests/check_harness_wakeups.sh` | **PASS** | [`gates-wakeups.txt`](gates-wakeups.txt) |
| Registro de pruebas | `python3 tests/audita-pruebas.py` | **OK** — estructura y evidencia en orden | [`gates-audita-pruebas.txt`](gates-audita-pruebas.txt) |
| Matriz de la app | `scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>` con `VOLUMEN=x6200-next218-banco` | **exit 0** — `ok` en nativos + 9 estáticos + identidad + temas + contratos del harness + sintaxis, 4 `SKIP` razonados, 1 `BLOCKED` (banco QEMU) | [`gates-check-matrix.txt`](gates-check-matrix.txt) (sobre `93fbce45`) y [`gates-check-matrix-tip.txt`](gates-check-matrix-tip.txt) (sobre el tip de la rama) |

La matriz se corrió dos veces: sobre `93fbce45` (código + pruebas) y sobre el tip de la rama (el
commit de este reporte, que sólo agrega documentación). El resultado es el mismo salvo el sha que
rotula `x6200-ab contracts` y el `uv sync` (ya no hace falta con el venv creado): `docs/**` no
dispara ningún trigger por path de la matriz.

El stage nuevo corre también dentro de `run_native_tests.sh` (host y `--ci-parity`), así que el CI
del PR lo ejecuta: `PASS: un loop_timer, periodo 30/250 ms, pump vivo y NetworkManager intacto`.

**Warnings:** el cambio **no agrega ni quita** ningún warning/error en `src/wifi.cpp` (mismo set,
11 líneas, antes y después; los que hay son preexistentes y están en funciones que no se
tocaron): [`lab/warnings-wifi.txt`](lab/warnings-wifi.txt). No se silenció nada.

## 5. SKIP y BLOCKED (con razón, nunca PASS)

| qué | estado | razón |
|---|---|---|
| `wspr`, `sstv`, `wefax` | **SKIP** | la imagen local `x6200-uiharness` es el target `ci` (sin `fftw3.h`/`sndfile.h`); el runner declara el SKIP y `run_all_tests.sh` lo cuenta como omitido. Mismo estado que en la base |
| `js8` | **SKIP** | omitido por `--quick` (el issue pide esa variante) |
| `webserver` | **SKIP** | no hay checkout de `.x6200_webserver` en este host |
| **banco QEMU** (`check-matrix`, y con él la confirmación QEMU_USER del ejecutable real) | **BLOCKED** | el preflight (`tools/captura-banco.sh --check`) reporta que faltan: el **chroot** (`$REPO/../.vfix3`, o sea el directorio **compartido** entre los worktrees de x6200), el **volumen** de build y `tools/remote/x6200-remote`. El banco resuelve por defecto volumen `x6200-build-output`, `BUILDROOT=$REPO/../buildroot_new/AetherX6200Buildroot` y contenedores `banco-apps`/`banco-audio`: **recursos compartidos de otras sesiones en vuelo**, que las reglas de laboratorio de #216 prohíben usar. Por eso se invocó la matriz con un volumen **propio** (`VOLUMEN=x6200-next218-banco`) a propósito, para que el banco no pudiera tocar nada ajeno. Construir un par privado (rootfs + cross-build completo del ROM) excede el presupuesto de esta corrida y se reporta en vez de improvisarse. **No es un pase** |
| Cross-build ARM del firmware | **no corrido** | mismo motivo: necesita el workspace de Buildroot y el volumen de build compartidos. El cambio usa sólo APIs que ya existían (`lv_timer_set_period`, declarada en `lvgl/src/misc/lv_timer.h` del LVGL 8.3.11 pineado) y no toca headers ni el `CMakeLists.txt`; la verificación de compilación ARM queda **pendiente** (CI/banco) |
| `docs/PRUEBAS.md` | **no modificado** | es el registro de builds **de la radio** (`audita-pruebas.py` exige que cada md5 de la tabla tenga su sección y al revés). Esta corrida no flasheó ni midió la radio: no hay build que registrar. La cobertura/deuda de release la cierra el hijo #222 |
| energía/pin físico/WiFi real | **PENDING_HARDWARE** | sin instrumento; ver el recuadro de arriba |

## 6. Blast radius (hecho probado, no afirmación)

- **Callers del módulo**: `src/main.c:657` (`wifi_power_setup()` al boot, antes de
  `backlight_init()`/`cat_init()`) y `src/dialog_wifi.c:279/286` (el toggle de la app WiFi/BT).
  Ambos siguen compilando contra la misma interfaz `wifi.h` (sin cambios) y los gates que los
  cubren (`native`, `check_actions`, `check_app_handlers`, `x6200-ab contracts`) pasan.
- **Único usuario del contexto GLib por defecto** en todo `src/`: `wifi.cpp`. Ningún otro módulo
  registra fuentes en ese contexto (`grep` sin resultados), así que bajar el ritmo con la radio
  apagada no demora callbacks de nadie más: [`lab/blast-radius.txt`](lab/blast-radius.txt).
- **Quien lee el estado**: `dialog_wifi.c`, `dialog_cluster.c` (`wifi_get_status()`,
  `wifi_scanning()`). La semántica de esos valores no cambia (los escenarios de la sección 3 los
  recorren: `WIFI_OFF`/`STARTING`/`DISCONNECTED`/`CONNECTED` y `scanning`).
- **Estado visible del operador**: los `MSG_WIFI_STATE_CHANGED` y los textos
  (`"Wrong WiFi password"`) siguen saliendo en los mismos momentos (contados por el fixture).

## 7. El fixture: qué espeja y qué no (honestidad del instrumento)

El módulo bajo prueba es el **real** (`src/wifi.cpp` staged); lo que se reemplaza es su entorno:

- **Motor de timers**: pool propio que espeja `lvgl/src/misc/lv_timer.c` (`due cuando
  now − last_run ≥ period`, `last_run = now` antes del callback, `set_period` sólo cambia el
  periodo). **No** se puede linkear el motor real: el job de CI de la suite nativa hace checkout
  **sin el submódulo `lvgl/`** y `--ci-parity` corre todo el árbol así (ese es el gate que ya
  rompió un PR antes: ver `docs/evidencia/2026-10-21-ciparity/`). Un timer del fixture que no
  venciera o no ejecutara no daría igual los números: las aserciones son exactas contra
  `10000/periodo`.
- **Pump de GLib**: `g_main_context_iteration()` cuenta sus llamadas y despacha **todo** lo
  pendiente (como el contexto por defecto). `g_main_loop_quit()` se cuenta y **no** destruye el
  loop (en la radio nadie corre `g_main_loop_run()`: el tráfico se bombea desde el callback de
  LVGL). El `GMainLoop`/`GMainContext` son tokens opacos, como lo son para el módulo.
- **libnm**: dos escenarios (sin `wlan0` / con `wlan0`), enums y claves **copiados del libnm del
  host** (`/usr/include/libnm/nm-dbus-interface.h`, `nm-setting-*.h`), y la respuesta del scan
  llega como **fuente async** del contexto (no en la misma vuelta). El camino de
  agregar/editar/borrar conexiones y el de leer la IP **no** se ejercitan (no son parte de lo que
  #218 mide) y sus stubs están declarados como tales en el fuente.
- **Lo que no se puede concluir de acá**: latencia real de NM en hardware, estado del pin físico,
  consumo. Por eso son PENDING_HARDWARE.

## 8. Laboratorio

- Sólo **HOST/QEMU_USER/CI**: `gcc`/`g++` del host para la suite nativa y Docker
  (`x6200-uiharness`, sólo lectura) para los gates del harness.
- **Recursos privados**: el worktree, su `tools/ui_harness/build`, el venv de `tools/ab_testing`
  y el volumen del banco (nombre propio) son de esta corrida. **No** se usaron ni compartieron
  `banco-apps`, `banco-audio` ni `x6200-build-output` (hay varias sesiones en vuelo).
- **Prohibido y no hecho**: SSH a la radio, flash, deploy, TX/RF, formatear la SD, releases,
  `sudo` interactivo. **No** se rebajó ningún budget ni se actualizó ningún golden.
- Todos los commits con la identidad del owner (`Pablo Iaria <iariap@gmail.com>`), por rutas
  explícitas, sin `git add -A`, sin push; `.asf-active` sin commitear.

## 9. Diagnóstico preexistente

**Ninguno nuevo**: todos los gates que aplican pasan y ninguna falla se atribuyó al cambio. Lo
único preexistente que aparece es el set de warnings de `src/wifi.cpp` (`-Wnarrowing`, variables
sin usar en funciones que no se tocaron, `connection_adding_cb` sin usar), **idéntico antes y
después** ([`lab/warnings-wifi.txt`](lab/warnings-wifi.txt)).

## 10. Cómo reproducir

```bash
cd <worktree>                     # state/worktrees/x6200/next-218, rama factory/next-218

# contrato + medición (el stage del gate)
sh tests/native/run_wifi_idle_timer.sh

# antes/después con el MISMO test (333/333 antes; 40/333 después)
sh tests/native/run_wifi_idle_timer.sh --ab origin/lu2dsx/x6200-rom

# gates de la épica
sh tests/native/run_native_tests.sh
sh tests/native/run_native_tests.sh --ci-parity
sh tests/run_all_tests.sh --quick
sh tests/check_harness_determinism.sh
sh tests/check_harness_natural.sh
sh tests/check_harness_wakeups.sh
sh tests/check_waterfall_incremental.sh
python3 tests/audita-pruebas.py
# (el banco QEMU necesita un chroot/volumen propios; no se usan los compartidos)
VOLUMEN=x6200-next218-banco sh <asf>/scripts/apps/x6200/check-matrix.sh \
    origin/lu2dsx/x6200-rom HEAD "$PWD"
```

## 11. Pendientes

1. **PENDING_HARDWARE**: medir en la radio el efecto real (corriente/vatios con medidor, pin
   físico, WiFi real, reconexión con la red caída). No se declara GO por CI verde.
2. **Banco QEMU / cross-build ARM**: BLOCKED en esta corrida por recursos compartidos; queda para
   el pipeline de QA remoto (hijo #222) o para una corrida con chroot/volumen propios.
3. **Integración**: este PR se integra **después** de #217 y antes de #219 (orden de la épica).
   Ningún merge automático.
