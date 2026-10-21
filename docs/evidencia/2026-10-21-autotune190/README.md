# Auto Tune v2 (issue #190) — evidencia de la corrida

Corrida de implementación del issue **`iariap/x6200_gui#190`** ("[RX][CW] Auto
Tune v2 — asistencia de sintonía precisa, segura y KISS").

| | |
|---|---|
| Worktree | `/home/pablo/desa/asf/state/worktrees/x6200/autotune-v2-190` |
| Rama | `factory/autotune-v2-190` |
| Base | `02801c0c` (`origin/lu2dsx/x6200-rom`) |
| HEAD al escribir esto | los 4 commits de la rama (ver *Commits*) |
| Volumen privado del build | `x6200-build-output-at190` (copia con `--reflink` del `x6200-build-output`) |
| Chroot privado del banco | `state/worktrees/x6200/at190-chroot` (copia con `--reflink` del `.vfix3` del owner) |

> ## **Hardware RF validation pending**
> Todo lo de abajo es host/banco (núcleo puro, DSP con señal sintética, arnés de
> UI sin RF y binario ARM sin radio). La precisión con señales reales se valida
> en el X6200: la lista está en *Pendientes* y en `docs/autotune.md`.

## 1. El signo CW vs CWR (la evidencia del contrato)

El contrato del issue (§2) exige que un tono **35 Hz por encima del pitch**
produzca **+35 Hz de dial en CW** y **−35 Hz en CWR**, y prohíbe explícitamente
dar por válido "un test que inyecte un error ya normalizado y por lo tanto no
compruebe la orientación real".

Por eso la prueba (`tests/native/test_cw_tune_sign.c`) genera un **tono complejo
sintético** y corre la MISMA cadena que `src/cw.cpp`:

```
DDS (dds_cccf_create(NUM_STAGES, (pitch+offset)/rate, MAX_CW_BW/rate, 60))
  -> decimación x64 -> ventana Hann (liquid_hann, scaled)
  -> FFT de 128 (liquid, LIQUID_FFT_FORWARD) -> argmax
  -> cw_peak_freq_from_bin()   (la fórmula "Fix fft order" de cw.cpp)
  -> cw_tune_error_hz()        (error = rx_tone_offset - pico)
```

Las dos funciones de conversión viven en `src/cw.h` y las usa también el lazo de
audio: **no hay copia**. Así, lo que se mide es la orientación real del DSP, no
una convención inventada por el test. El crudo de la fórmula es el **negativo**
del corrimiento del tono (`docs/HALLAZGOS.md` 52), y el test lo comprueba.

### 1.a Salida cruda — FFT real, liquid 1.4.0 (la del firmware), ASan+UBSan

`signo/signo-cw-cwr-asan-liquid140.txt` (comando y salida):

```
$ docker run --rm --user $(id -u):$(id -g) -v $PWD:/src:ro -v /tmp/opencode/at:/t -w /tmp \
    x6200-uiharness-full sh -c 'cc -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter \
      -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover \
      -I/src/tests/native -I/src/src /src/src/autotune.c /src/tests/native/test_cw_tune_sign.c \
      /src/tests/native/test_framework.c /src/tests/native/cw_tune_sign_main.c \
      -L/usr/local/lib -Wl,-rpath,/usr/local/lib -lliquid -lm -o sign_14 && ./sign_14'

23 comprobaciones, 0 fallos
```

Lo que afirman esas 23 comprobaciones:

| comprobación | resultado |
|---|---|
| tono en `pitch + 35` → `error_hz` **positivo** y ≈ +35 Hz (±8 Hz: un bin = 5,383 Hz) | OK |
| tono en `pitch − 35` → `error_hz` **negativo** y ≈ −35 Hz | OK |
| tono en el pitch → ≈ 0 | OK |
| con `rx_tone_offset = −30` y con `+45`: el valor y el signo NO cambian (se compensa el centro del DDS del AUTO #139 y el error se mide contra el pitch **manual**) | OK |
| el pitch objetivo es el configurado (700 Hz con pitch 720 → −20 Hz), no un target fijo de 700 | OK |
| **ciclo cerrado CW**: señal 35 Hz arriba del pitch → error +35 → `autotune_dial_delta` = **+35** → dial +35 → error medido ≈ 0 (|error| ≤ 10 Hz) | OK |
| **ciclo cerrado CWR**: la misma señal → error +35 → delta = **−35** → dial −35 → error medido ≈ 0 | OK |

### 1.b Por qué con liquid 1.4.0 y no con la del sistema

La imagen `x6200-uiharness` (target *ci*) trae la liquid del sistema
(1.6.0, Ubuntu 24.04); la `x6200-uiharness-full` además construye la **1.4.0** en
`/usr/local` — la misma que linkea el firmware. Con la 1.6.0 el binario con ASan
reporta un leak **de la librería** (72 B por cada `dds_cccf_create`, reproducido
con un programa de 5 líneas que sólo crea y destruye): `signo/leak-liquid160-ubuntu.txt`.
El runner nativo prefiere `/usr/local/lib` (1.4.0) cuando existe por ese motivo, y
lo documenta en su comentario. Con la 1.4.0 el test pasa limpio bajo ASan+UBSan.

### 1.c En el host del owner (sin liquid) — SKIP razonado

`nativos/run_native_tests-host.txt`:

```
== Auto Tune v2 (#190): signo del tono (FFT real, liquid) ==
SKIP: liquid-dsp no esta instalada en este host (no se puede medir el signo).
      El contenedor x6200-uiharness y el job functional del CI si la tienen;
      el comando exacto esta en docs/evidencia/2026-10-21-autotune190/README.md.
```

**Nunca reportado como PASS.** (En el CI el job *functional* tampoco instala
liquid, así que ahí también sale SKIP; el que sí lo corre es cualquier host o
imagen que tenga liquid-dsp.)

## 2. Núcleo puro (tests nativos)

`nativos/run_native_tests-host.txt` y `nativos/run_native_tests-docker-full-asan-ubsan-tsan.txt`
(el mismo runner, en la imagen `x6200-uiharness-full`, con ASan+UBSan y TSan):

```
== compilador: gcc (GCC) 16.2.1 ...
== sanitizers: NO disponibles en este toolchain (se omite)      <-- host del owner
...
44513 comprobaciones, 0 fallos        <-- suite principal (incluye test_autotune)
...
== ejecutando ==
== sanitizers: ASan + UBSan                                    <-- contenedor full
...
== Auto Tune v2 (#190): signo del tono (FFT real, liquid) ==
23 comprobaciones, 0 fallos
== TSan: disponible en este host ==
== TSan: el modulo del espectro no reporta razas ==
== TSan: fixture de carrera deliberada (el detector tiene que disparar) ==
```

`tests/native/test_autotune.c` (**169 comprobaciones** propias; el conteo suelto
está en `nativos/nucleo-autotune-standalone.txt`) cubre la lista del §7:

- gate de la trama: inválidas, `NaN`/`inf`, `ratio = 0` (divisor de ruido cero),
  `ratio < 4` (SNR < 6 dB), fuera del rango del estimador y "overflow";
- menos de 3 tramas; 3 tramas distintas y estables; una muestra atípica (la
  mediana es robusta y la ventana corrediza la deja salir); spread 20 (acepta) y
  21 (rechaza);
- error ±10 (éxito sin write), ±11 (requiere corrección), ±300 (permite), ±301
  (rechaza), redondeos ±, destino fuera de la banda y `radio_check_freq`;
- CW y CWR: tono a ±35 Hz del pitch en cada modo, signo del `delta_hz` y
  convergencia del error a ≈ 0;
- con `rx_tone_offset` distinto de 0 (AUTO #139) el destino sale del **pitch
  manual**, y cambiar el offset **descarta las tres medidas acumuladas**;
- después de la propuesta: guarda de 300 ms + 3 tramas NUEVAS para verificar,
  rollback condicionado, **una sola corrección** y timeout total de 7000 ms;
- dos triggers seguidos, cancelación antes y después de la provisional, giro
  manual durante COLLECT y VERIFY, cambio externo (CAT) durante VERIFY, lock,
  modo/banda/VFO, RIT, TX antes de empezar y durante, cambio de pitch, y la
  invariante de que **el VFO final obedece la regla de propiedad**;
- una trama vieja/repetida/desordenada **nunca** se reutiliza.

## 3. Banco de UI (fixtures, sin RF ni DSP)

`banco-ui/corrida.txt` + las capturas y los JSON de cada fixture:

```
ui_harness: autotune -> out/autotune.png (249304 bytes), flushes=6, ...
exit=0 autotune
ui_harness: autotune-progress -> out/autotune-progress.png (249850 bytes), ...
exit=0 autotune-progress
ui_harness: autotune-found -> out/autotune-found.png (249850 bytes), ...
exit=0 autotune-found
ui_harness: autotune-notcw -> out/autotune-notcw.png (248668 bytes), ...
exit=0 autotune-notcw
```

Qué codifica cada uno (el fixture corta la corrida en vez de capturar y reportar
verde):

| fixture | qué prueba |
|---|---|
| `autotune` | la acción arranca la sesión en CW; el dial **no se mueve** durante COLLECT (v2 no barre) y el segundo pulsado cancela dejándolo igual |
| `autotune-progress` | la **única corrección provisional**: el dial exactamente en `start + 35` con la sesión todavía viva (y falla si hubo una segunda corrección o si la sesión ya había terminado) |
| `autotune-found` | la corrección **verificada** de punta a punta: si la verificación fallara, el rollback devolvería el dial al arranque, así que quedar en `start + 35` ES la prueba |
| `autotune-notcw` | el **rechazo fuera de CW/CWR**: no abre sesión, no pide ninguna muestra de CW y no toca el dial |

El banco no compila `src/cw.cpp` (liquid + hilo de audio), así que los fixtures
con tono usan una **inyección sólo en tests** de la instantánea del DSP
(`tools/ui_harness/mocks/mock_cw_tune.c`), que modela el receptor (mover el dial
cambia el error medido). No se fabrica RSSI ni se falsea el flujo de producción.

## 4. Chequeos estáticos del repo

`estaticos/checks-del-repo.txt` — los mismos que el job *functional* del CI (más
el nuevo `check_autotune_v2.py`): **11/11 en exit 0**.

`check_autotune_v2.py` fija sobre el fuente lo que ningún banco puede ver: los 9
textos exactos de UX (la barra de mensajes no se dibuja en el host porque
`event_send()` es un no-op documentado), la entrada exacta de Settings, que
ninguna entrada preexistente de `long_action_items[]` desaparezca, `ACTION_ATUNE
== 129`, que ningún default pase a Auto Tune, que el núcleo siga puro (sin LVGL,
radio, `cfg`/`params` ni S-meter) y que la UI sea estrictamente RX (sin
`radio_set_ptt`, sin escribir `key_tone`/`rit`, sin S-meter). Tiene su ciclo
rojo→verde demostrado en `estaticos/check-autotune-v2-rojo.txt`.

## 5. Arnés A/B (ruff / format / ty / pytest / themes)

`ab/ruff-format-ty-pytest-themes.txt` (los mismos comandos que el CI):

```
$ uv run ruff check .            -> All checks passed!
$ uv run ruff format --check .   -> 199 files already formatted
$ uv run ty check src tests      -> WARN No python files found under the given path(s) / All checks passed!
$ uv run pytest -m 'not e2e' -q  -> 227 passed, 13 deselected
$ uv run x6200-ab themes         -> OK (todos los pares WCAG pasan)
```

(El `ty` avisa "No python files found under the given path(s)" **antes** del
cambio: es el comportamiento del comando tal como está en el CI, no algo que
introduzca esta corrida.)

`ab/contracts.txt` — `x6200-ab contracts` sobre el HEAD (mismo comando que el
job *UI Quality Contracts*).

## 5.b `tests/run_all_tests.sh --quick` y los e2e del marco

`nativos/run_all_tests-quick.txt`:

```
================ resumen ================
  ... autotune_v2: OK ...
  host: OK   138 comprobaciones, 0 fallos
  host_ci_parity: OK   138 comprobaciones, 0 fallos
  wspr: SKIP SKIP: wspr: la imagen x6200-uiharness no trae /usr/include/fftw3.h (build full: ...)
  sstv: SKIP ... wefax: SKIP ...
  navtex: OK   all OK
  (omitidos: wspr sstv wefax js8 webserver)
  17 OK, 0 fallos
exit=0
```

(Esos tres SKIP son los codecs que sólo compilan con la imagen `full`; js8 y el
webserver quedan omitidos por `--quick`. Ninguno es un fallo ni un pase.)

`ab/pytest-e2e.txt` — los e2e del marco (`TMPDIR` en disco):

```
$ TMPDIR=/tmp/opencode/at/tmp uv run pytest -m e2e -q
1 failed, 12 passed, 227 deselected in 257.22s
FAILED tests/test_e2e.py::test_all_fixtures_can_be_captured - assert 2 == 0
  "status": "BLOCKED", ... "harness failed for scenario dfl-mirror-lsb ...
```

El único rojo es el fixture preexistente `dfl-mirror-lsb` (§6), que también hace
que `x6200-ab contracts` no pueda evaluar (§5.c).

## 5.c `x6200-ab contracts` (gate del CI) — BLOCKED, preexistente

`ab/contracts.txt`: el gate **no evalúa** (exit 2) sobre el HEAD **ni sobre la
revisión base** (`02801c0c`), por el mismo escenario `dfl-mirror-lsb`. Se reporta
BLOCKED con su razón (nunca como PASS).

## 6. Determinismo del arnés

`estaticos/determinismo.txt`: el gate `tests/check_harness_determinism.sh`
**falla**, pero por dos fixtures **ajenos a este cambio**:

```
dfl-mirror-lsb: el gesto colgado tras cambiar de modo toco el filtro (low=200 high=3525)
dfl-mirror-usb: el handle LOW no esta dibujado en x=37 (mejor columna x=-1, run=0)
FAIL: missing .../out/determinism/dfl-mirror-lsb-1.png
```

Reproducido **en la revisión base** (`02801c0c`, sin este cambio) con el mismo
arnés: `estaticos/determinismo-base-dfl-mirror.txt` (ambos `exit=1` con el mismo
mensaje). Los cuatro fixtures de Auto Tune v2 salen con un único sha256 por
fixture (dos corridas idénticas), incluidos en la salida del gate:

```
autotune: 07924cacf6d6b2ff317739a7298b5d6f48d4f2dbd54d3c8c3f9398c9fecc1969
autotune-found: 8e95c8dffdb432bb42a012c10cfd1e7e0fd57cc1be921e464531ed629d04cc7a
autotune-notcw: 606cfedc2fb3992d763bb2927d3b772a74e759147e93e38ce447fb3fcc67e451
autotune-progress: 8e95c8dffdb432bb42a012c10cfd1e7e0fd57cc1be921e464531ed629d04cc7a
```

Arreglar esos dos fixtures (#179/#182) queda **fuera del alcance** del issue #190.

## 7. ARM/ROM (cross-compile) y el sello de fuentes (#165)

`arm/cross-compile-arm.txt` + `arm/bgui-make.log` + `arm/bgui-cmake.log`: la GUI
se cross-compiló para ARM con el toolchain de Buildroot, con el sello
`-DX6200_FUENTE_MD5=$(tools/fuente-md5.sh)`:

```
-- x6200_gui: fuente md5 d9dcd060e20a25aaf3691b2e55418bff
[100%] Linking CXX executable x6200_gui
[100%] Built target x6200_gui
9dcde687764a1facf0b60b18feb89274  /b/target/usr/sbin/x6200_gui
```

El binario loguea su sello al arrancar (`src/main.c`, issue #165) y el log del
banco lo confirma contra el `.fuente.md5` del volumen y contra
`tools/fuente-md5.sh` (los tres iguales):

```
[User]  (757764.702, +757764702)  boot_identity: fuente: d9dcd060e20a25aaf3691b2e55418bff (x6200_gui)  (in main.c line #142)
```

o sea: el sello embebido en el binario == el `.fuente.md5` del volumen (que el
script escribe) == `sh tools/fuente-md5.sh` sobre el worktree
(`d9dcd060e20a25aaf3691b2e55418bff`), y el banco imprime además
`fuente: md5 d9dcd060e20a (src/ + lvgl/src)` en cada captura (ver §8).

El build ARM **no agrega ningún warning** en los archivos de este cambio: los 4
warnings del log son preexistentes (`remote_server.cpp` `-Wpragmas`,
`widgets/lv_waterfall.c` `-Wdiscarded-qualifiers`).

> El rebuild oficial de Buildroot (`make x6200-gui-rebuild`) **no está disponible
> en este entorno**: el volumen del build tiene el *output dir* y el toolchain,
> pero no el árbol fuente de Buildroot (`/workspace/buildroot` no existe), que el
> Makefile del volumen necesita. Por eso el cross-compile se hizo con CMake y el
> `toolchainfile.cmake` del volumen, que produce el mismo binario para ARM con el
> mismo sello (`X6200_FUENTE_MD5`) — y el banco lo verificó con el gate #86.

## 8. Banco QEMU (binario ARM real, sin radio)

`qemu-banco/`: cinco capturas del **binario ARM** corriendo bajo `qemu-arm-static`
en un chroot privado con framebuffer falso, comandado por su socket de control.
El bench corrió con `RECONSTRUIR=no` (el binario ya estaba compilado de estas
fuentes; el sello coincide) y pasó su propio gate **#86**:

```
OK: .../at190-chroot/usr/sbin/x6200_gui es el firmware del port (ELF32 ARM, 13923332 B, 3/3 marcas)
  version: 314644ad Docs (#190): reescribir autotune.md para v2 y dejar
  fuente: md5 d9dcd060e20a (src/ + lvgl/src)
  binario: md5 9dcde687764a   qemu-cpu: cortex-a7
```

| captura | qué muestra |
|---|---|
| `qemu-190-1-accion-visible.png` | la página **APP 6:6** con el botón **Auto Tune** (la acción sigue existiendo y es alcanzable en el binario de la radio) |
| `qemu-190-2-activacion.png` | CW: `Auto Tune: tuning...` (dial 14.074.000, sin cambios) |
| `qemu-190-3-cancelacion.png` | CW: segundo pulsado → `Auto Tune: cancelled` (dial 14.074.000) |
| `qemu-190-4-rechazo.png` | **AM**: `Auto Tune: CW/CWR only`, sin sesión y con el dial intacto |
| `qemu-190-5-sin-senal.png` | CW sin señal: `Auto Tune: no reliable adjustment` a los 3 s, dial intacto |

El dial se leyó por el estado del socket en cada paso (`freq = 14074000`) y no se
movió en ninguna de las cinco.

> **"tuned" no se puede probar en el banco**: no hay RF ni DSP (sin placa de
> sonido no llegan tramas de CW), así que el camino de la corrección verificada
> se prueba en el arnés de UI con la muestra inyectada (`autotune-found`) y en el
> núcleo con señales sintéticas. La correlación con RF real es del owner
> (**Hardware RF validation pending**).

### Un defecto real encontrado por el banco (y corregido)

La primera corrida del banco mostró que, **sin ninguna trama de CW**, la sesión
no fallaba a los 3 s sino recién al tope de 7000 ms: el chequeo del plazo de fase
estaba dentro de la rama "llegó una trama". Con la corrección, el plazo corre
siempre y `qemu-190-5-sin-senal.png` lo demuestra (mensaje a los 3 s). También se
encontró (por un warning del build ARM) que la UI pasaba `cw_tune_sample_t *`
donde el núcleo espera `autotune_sample_t *`: ahora hay **un solo tipo**
(`autotune_sample_t`, en `src/autotune.h`, incluido por `src/cw.h`).

## 9. Matriz del harness (`check-matrix.sh`)

`check-matrix.txt` (con el volumen y el chroot privados de esta corrida
exportados, más `RECONSTRUIR=no` para el banco: en este entorno no existe el
árbol fuente de Buildroot, así que el rebuild del banco no está disponible — ver
§7):

```
$ VOLUMEN=x6200-build-output-at190 CHROOT=../at190-chroot RECONSTRUIR=no \
      scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD $PWD
```

| bloque | resultado |
|---|---|
| native tests + los 9 chequeos estáticos del firmware | **ok** |
| check_build_identity | **ok** |
| check_wcag (temas de la imagen) | SKIP ("sin cambios en rootfs/**/themes/*.json") |
| ruff check / ruff format / pytest (`-m 'not e2e'`) | **ok** |
| x6200-ab themes | **ok** |
| x6200-ab contracts | **BLOCKED** (preexistente: `dfl-mirror-lsb`, §5.c) |
| bash -n de los .sh tocados | **ok** |
| harness determinism gate | **FAIL** (preexistente: `dfl-mirror-lsb`, §6) |
| banco QEMU: preflight / params.db / captura de humo | **ok** (15.458 colores, binario md5 `9dcde687764a`) |

19 bloques `ok`; el `exit=1` de la matriz es el determinism gate, rojo también en
la base. Los dos logs intermedios (con dos artefactos de esta corrida: un `FAIL`
del banco por intentar reconstruir sin el árbol de Buildroot, y un falso
"submódulo lvgl sin inicializar" por correr `run_all_tests.sh --quick` en
paralelo con el gate, que esconde `lvgl/` para medir la paridad con el CI) quedan
en `check-matrix-con-dos-artefactos.txt` y `check-matrix-artefacto-lvgl-oculto.txt`
para no borrar evidencia.

## Commits de la corrida

| sha | qué |
|---|---|
| `876b8925` | núcleo puro + snapshot DSP + coordinador CW/CWR (RX) |
| `6f4e9004` | tests nativos del núcleo y del signo + chequeo estático |
| `7294169e` | fixtures v2 del banco de UI + inyección de la muestra de CW |
| `9543a103` | docs (`autotune.md`) |
| _HEAD_ | este directorio de evidencia |

(`git log --oneline 02801c0c..HEAD` en el worktree de la corrida; el commit de la
evidencia es el HEAD de la rama, así que su sha no se cita a sí mismo.)

## Pendientes

1. **Hardware RF validation pending** (la lista del §7 del issue, también en
   `docs/autotune.md`): portadora estable a **+35 Hz** y a **−35 Hz** en CW y en
   CWR, CW Decoder **AUTO #139 ON/OFF** y su coexistencia, CW débil/QRM (no QSY),
   dial/banda/VFO/APP/RIT/PTT durante la operación (el usuario siempre gana) y
   que la pantalla CW tuner y la aceleración de rueda (#186) sigan operativas.
2. `tests/check_harness_determinism.sh` sigue en rojo y `x6200-ab contracts`
   sigue BLOCKED (no evalúa) por los fixtures `dfl-mirror-*` (#179/#182),
   **preexistente** y fuera del alcance del #190: reproducido en la base
   (§6 y §5.c). Con eso, el CI del PR va a quedar rojo en
   `test_all_fixtures_can_be_captured` / *UI Quality Contracts* hasta que el
   owner arregle esos dos fixtures.
3. El rebuild oficial de Buildroot (`make x6200-gui-rebuild`) no se pudo correr
   en este entorno (falta el árbol fuente de Buildroot); ver §7.
4. La barra de mensajes del arnés de UI no se dibuja (`event_send()` es un no-op
   documentado): los textos exactos quedan fijados por `check_autotune_v2.py`, y
   la prueba de que se ven es el banco QEMU (§8).

## Cómo se reproduce

```bash
# 1) núcleo puro + signo (el signo necesita liquid; la imagen full tiene 1.4.0)
sh tests/native/run_native_tests.sh
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/src" -w /src \
    x6200-uiharness-full sh tests/native/run_native_tests.sh

# 2) chequeos estáticos + arnés A/B
python3 tests/check_autotune_v2.py
(cd tools/ab_testing && uv run pytest -m 'not e2e' -q && uv run x6200-ab themes)

# 3) fixtures del arnés de UI
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" -v "$PWD:/src" \
    -w /src/tools/ui_harness x6200-uiharness -lc \
    'cd build && for f in autotune autotune-progress autotune-found autotune-notcw; do
        ./ui_harness --scenario ../fixtures/$f.scn --png out/$f.png --json out/$f.json; done'

# 4) ARM + banco QEMU (volumen y chroot privados de una corrida)
VOLUMEN=x6200-build-output-at190 sh tools/compila-gui.sh     # en este entorno: ver §7
CHROOT=../at190-chroot VOLUMEN=x6200-build-output-at190 RECONSTRUIR=no \
    tools/captura-banco.sh /tmp/banco.png '{"cmd":"page","text":"app"}'

# 5) la matriz del harness (con el volumen y el chroot privados)
VOLUMEN=x6200-build-output-at190 CHROOT=$PWD/../at190-chroot \
    scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD "$PWD"
```

## Índice de la evidencia

```
check-matrix.txt          la matriz completa (19 ok, 1 BLOCKED y 1 FAIL preexistentes)
check-matrix-*.txt        los dos logs intermedios con sus artefactos documentados
signo/          signo-cw-cwr-asan-liquid140.txt   (FFT real, liquid 1.4.0, ASan+UBSan)
                leak-liquid160-ubuntu.txt         (leak de libliquid 1.6.0, ajeno al cambio)
nativos/        run_native_tests-host.txt
                run_native_tests-docker-full-asan-ubsan-tsan.txt
                nucleo-autotune-standalone.txt
banco-ui/       corrida.txt + los 4 PNG/JSON de las fixtures
estaticos/      checks-del-repo.txt
                check-autotune-v2-rojo.txt        (ciclo rojo -> verde del chequeo nuevo)
                determinismo.txt
                determinismo-base-dfl-mirror.txt  (el rojo, reproducido en la base)
ab/             ruff-format-ty-pytest-themes.txt
                contracts.txt
arm/            cross-compile-arm.txt + bgui-make.log (warnings) + bgui-cmake.log
qemu-banco/     00-comandos.txt + las 5 capturas del binario ARM real
```
