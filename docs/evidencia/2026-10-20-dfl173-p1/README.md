# #173 PR 1 — Visual a escala, solo RX/UI (banda pasante + ticks del waterfall)

Corrida `factory/dfl-visual-173`. **Base**: `9bb1d306` (trunk tip, `lu2dsx/x6200-rom`).
**Commits de código**: `912f5d31` (banda + ticks + golden + fixtures) y `f5f3f14f`
(la banda arranca debajo del S-meter). Alcance **PR 1** del §6 del issue #173:
**solo** la banda pasante RX a escala en el espectro y **dos ticks** del ancho del
filtro en el borde superior de la cascada. **Sin** diálogo editor (eso es PR 2).

## 1. Exploración obligatoria (§8) — con líneas

1. **HEAD real**: `git rev-parse HEAD` = `9bb1d306dcb7176f6a89be51ae49797474db9a80`
   (igual al HEAD del issue). Sin cambios nuevos del owner sobre `src/spectrum.c`
   ni `src/waterfall.c` desde ese commit.
2. **Función del área visible del espectro**: `spectrum_draw_cb()`
   (`src/spectrum.c`), registrada en `LV_EVENT_DRAW_MAIN_END` por `spectrum_init()`.
   Toma `x1 = obj->coords.x1`, `w = lv_obj_get_width(obj)` (=800),
   `h = lv_obj_get_height(obj)` (=`spectrum_height` = 480/3 = 160,
   `src/main_screen.c:85,1404`). El visor de señales es la franja
   `y1+h-visor_height .. y1+h` con `VISOR_HEIGHT_RX 100` (`src/spectrum.c:30`), o
   sea `y=60..160`. La franja IARU (`band_plan_init(obj, spectrum_height -
   BAND_PLAN_H)`, `src/main_screen.c:1480`, `BAND_PLAN_H 22`, `src/band_plan.h:25`)
   es un objeto **hermano creado después** del espectro: se dibuja encima, así que
   la banda no la tapa.
3. **Fuentes de Subjects y callbacks**:
   - `src/cfg/mode.c::update_real_filters()` (línea 550) es la **única** fuente de
     `cfg_cur.filter.real.from/to`; el mapeo por modo (líneas 556-582): LSB/LSB-D →
     `-high..-low`; USB/USB-D/CW/CWR → `low..high`; AM/SAM/NFM/WFM → `-high..+high`.
   - `cfg_mode_set_low_filter()` (`src/cfg/mode.c:317`): devuelve 0 en
     AM/SAM/NFM/WFM (**LOW no editable**); en CW/CWR acepta `val < high`
     (**negativo permitido**).
   - `src/vol.c`: LOW paso **10 Hz** (línea 79), HIGH **50 Hz** (10 en CW/CWR,
     líneas 96-107), BW **20 Hz** (línea 122). `MAX_FILTER_FREQ` = 10000.
   - Verificado en el binario actual: CW negativo y AM LOW no editable salen de
     esas dos funciones, sin cambios.
4. **Ciclo de vida de diálogos**: PR 1 **no toca diálogos**, `src/buttons.cpp` ni
   `src/main_screen.c`; el antecedente #131 (LVGL fuera del hilo de GUI) se
   respeta: los observers del filtro/span sólo **difieren** el redibujo por
   `scheduler_put_noargs()` y nunca llaman a LVGL desde el hilo que notifica.
5. **Fixtures deterministas**: se agregaron las claves `filter_low`/`filter_high`
   al parser del arnés (`tools/ui_harness/src/scenario.c`) y al mock de radio
   (`mocks/mock_radio.c`), que deriva los bordes reales con el mismo mapeo de
   `update_real_filters()`. Fixtures `dfl-*` (USB-D/LSB-D, 6 temas, spans).
6. **Impedimento técnico**: ninguno que obligue a ampliar alcance. Dos ajustes
   propios del entorno, documentados: (a) el volumen de build compartido se clonó a
   uno privado con reflink (btrfs); (b) el `waterfall_test` del arnés necesitó
   `spectrum.h` staged porque la geometría es `static inline`.

## 2. Fórmula implementada (§2 del issue)

`spectrum_filter_band(from, to, span, w)` (`src/spectrum.h`, `static inline`,
compartida por el espectro y los ticks):

```text
x0 = w/2
xA = x0 + round(from * w / span)
xB = x0 + round(to   * w / span)
left = min(xA, xB); right = max(xA, xB)
recorte a [0, w]; sin interseccion (o span <= 0) -> no se dibuja
```

Redondeo al entero más cercano y, en los medios, **alejándose del cero**
(`spectrum_round_div`), documentado. La posición sale **exclusivamente** de
`cfg_cur.filter.real.from/to` y `cfg_cur.fft_width`; **no** se centra una ventana
de BW sobre el VFO. `span <= 0`, `w <= 0` e `INT32_MIN/MAX` no dividen por cero ni
desbordan (`(int64_t)from * w`).

### Golden medido (test nativo, `tests/native/test_spectrum.c`)

```text
golden #173 USB-D from=225 to=3525 span=48000 W=800 -> x=404..459 (contrato 404..459, a la derecha del VFO)
golden #173 LSB-D from=-3525 to=-225 span=48000 W=800 -> x=341..396 (contrato 341..396, espejo)
golden #173 AM    from=-4500 to=+4500 span=48000 W=800 -> x=325..475 (simetrico)
```

Casos extra verificados (mismo test): spans 50/100 kHz, redondeo en los medios,
`span=0`, `span<0`, `w=0`, filtro fuera de vista (izq/der), recortes
parciales/totales, zoom extremo, `INT32_MIN/MAX`, y los espejos por modo
(USB/USB-D/LSB/LSB-D, AM/SAM/NFM/WFM simétricos, CW/CWR con LOW negativo). El
test también dibuja el caso golden con el callback real y verifica el relleno, las
**dos** líneas de borde en x404/459 y el texto de BW dentro de la banda.

## 3. Qué se cambió y qué no

**Se cambió** (PR 1): `src/spectrum.c` (banda pasante a escala: relleno discreto
en el rol `vfo_line`, dos líneas de 1 px, texto de BW si entra, más el refresco
diferido/coalescido cuando cambia el filtro/modo/span); `src/spectrum.h` (la
geometría pura); `src/waterfall.c`/`.h` (dos ticks overlay en el borde superior
de la cascada); el test nativo del espectro + stubs; el test del waterfall +
stubs; el arnés de UI (claves `filter_low`/`filter_high`, fixtures `dfl-*`,
settle de los ticks); la calidad A/B (región `waterfall_ticks` + dos
`expected_changes`).

**No se tocó**: DSP/RF/TX, MCU, FFT, CAT, audio, notch de DFN (sigue con su
relleno blanco, distinguible), paleta del waterfall, `src/cfg/mode.c`,
`src/vol.c`, `src/buttons.cpp`, `src/main_screen.c`, los seis temas JSON, la
línea central del #140.

## 4. Ticks del waterfall

Dos `lv_line` de 1 px y 6 px de alto, hijos de la **imagen** de la cascada
(superficie de 800 px, alineada con el espectro), en el rol `vfo_line`, en las x
que da el **mismo** `spectrum_filter_band()` que el espectro. Son overlay de UI:
no se escribe en el buffer de datos ni en el anillo de filas, no se tintan
columnas de la historia, no se toca la paleta. La línea central (#140) sigue
siendo el último hijo del waterfall (los ticks se crean antes). El arnés lo
codifica en `settle_waterfall_filter_ticks()` (`tools/ui_harness/src/screens.c`).

## 5. Pruebas y gates (salida real)

### 5.1 Nativo (host + contenedor)

- `tests/native/run_native_tests.sh` (host): **118 comprobaciones, 0 fallos**;
  el bloque del espectro, **1018 comprobaciones, 0 fallos** (`native-host.log`).
- El mismo script dentro de `x6200-uiharness` (ASan + UBSan + TSan): **1018
  comprobaciones, 0 fallos** en el espectro, **TSan sin razas** y el fixture de
  carrera deliberada disparando (`native-container-asan-ubsan-tsan.log`).
- `gcc -fanalyzer` sobre `spectrum.c` staged: sin observaciones (el script lo
  corre junto con la suite).
- `tests/waterfall/test_waterfall.c` (rasterizado incremental + ticks): PASS
  (`waterfall-test.log`).
- `tests/check_waterfall_incremental.sh` (con ASan/UBSan): **PASS 195 checks**
  (`waterfall-incremental.log`).
- `tests/check_harness_natural.sh`: **PASS**; el área invalidada del espectro
  sigue en **132000 px por frame** (idéntica al baseline de #149 → sin regresión
  de costo) (`natural.log`).
- `tests/check_drawbuf_partial.sh`: **PASS** (`drawbuf-partial.log`).

### 5.2 Estáticos y calidad

- `ruff check .`, `ruff format --check .`, `ty check`: OK (`ab-static.log`).
- `pytest -m "not e2e"`: **227 passed, 13 deselected** (`ab-pytest.log`);
  `pytest -m e2e`: **13 passed** (`ab-pytest-e2e.log`).
- `tests/check_*.py` + `audita-pruebas.py` + `test_swrscan_curve_evidence.py`:
  todos OK (`static-checks.log`).
- `x6200-ab themes` (WCAG estático sobre los 6 temas): OK (`ab-themes.log`).
- `x6200-ab audit` (inspección estructural real sobre 101 escenarios, 11062
  objetos): **0 hallazgos** (`ab-audit.log`).
- `tests/check_harness_determinism.sh`: **PASS** (PNG+JSON idénticos en dos
  corridas de cada fixture, incluidos los `dfl-*`) (`determinism.log`).
- `tests/run_all_tests.sh`: **14 OK, 0 fallos** (`run-all-tests.log`). Los
  codecs (wspr/sstv/wefax), js8 y el webserver quedan en **SKIP** hermético
  (la imagen `x6200-uiharness` en uso es la `ci`, sin `fftw3.h`/`sndfile.h`; el
  script lo reporta como omitido, nunca como PASS).

### 5.3 A/B (`AB-DFL173-1`)

`uv run x6200-ab compare --a 9bb1d306 --b factory/dfl-visual-173 --experiment-id AB-DFL173-1`:

```text
AB-DFL173-1: PASS (exit 0)
  Functional         PASS   92 scenarios captured on both revisions
  Visual Regression  PASS   no unexplained visual regressions
  Quality Contract   PASS   943 contract checks passed on the candidate
  Performance        PASS   render-cost proxy within budget
  Hardware/Energy    BLOCKED (advisory)
```

El diff de píxeles está **confinado al espectro y a los ticks**: medido sobre
`home-usb-20m` (base vs rama), por región (tolerancia |dR|+|dG|+|dB| > 12):

```text
barra_superior 0   s_metro 0   espectro 564   frecuencia 0   waterfall_ticks 12   botones 0
bbox x=400..406 y=66..245
```

Es decir: **cero** cambios en header, S-meter, fila de frecuencia, band plan y las
cinco teclas; todo dentro de `espectro` y `waterfall_ticks`. Reporte completo en
`ab-DFL173-1-report.md` y `ab-DFL173-1-results.json`.

### 5.4 Capturas 800×480 antes/después

`capturas/antes-home-usb-20m.png` y `capturas/despues-home-usb-20m.png` (y el par
de `main-espectro-agua`): la banda pasa de invisible (relleno `bg_color` casi
negro) a la banda discreta con bordes y texto, sólo dentro del espectro.

## 6. Banco QEMU (ARM real, `qemu-arm-static`, cortex-a7, gate #86)

Volumen de build **privado** (`x6200-build-output-dfl173`, clonado con reflink del
compartido) y chroot **privado** (`/home/pablo/desa/asf/state/dfl173-chroot`).

```text
binario: md5 57b11ddc058a   (commit f5f3f14f)
fuente:  md5 8efe98c2ee26   (sello .fuente.md5; el binario loguea el mismo al arrancar)
```

La GUI del banco arranca, y con el estado sembrado en la base (`fft_dec=3` → span
48 kHz; modo `lsb_dig` con `filter_low=225`, `filter_high=3525`) reproduce el
**golden del contrato sobre el binario ARM**:

| Captura | Tema | Modo | Banda |
|---|---|---|---|
| `banco-usbd-aurora-oscuro.png` | Aurora (oscuro) | USB-D | x≈404..459, a la derecha, “3.3k” |
| `banco-lsbd-aurora-oscuro.png` | Aurora (oscuro) | LSB-D | x≈341..396, a la izquierda, “3.3k” |
| `banco-usbd-sunlit-claro.png` | Sunlit (claro) | USB-D | x≈404..459 |
| `banco-lsbd-sunlit-claro.png` | Sunlit (claro) | LSB-D | x≈341..396 |

Los ticks se ven en el borde superior de la cascada (y=240). `banco-home.png` es
el arranque (banda por defecto). Salida: `banco.log`.

## 7. Comandos reproducibles

```sh
# nativo (host y contenedor)
tests/native/run_native_tests.sh
docker run --rm --entrypoint bash -v "$PWD:/src" -w /src x6200-uiharness \
    sh tests/native/run_native_tests.sh /tmp/native

# A/B y calidad
cd tools/ab_testing
uv run x6200-ab compare --a 9bb1d306 --b factory/dfl-visual-173 --experiment-id AB-DFL173-1
uv run x6200-ab audit   --revision factory/dfl-visual-173
uv run x6200-ab themes
uv run ruff check . && uv run ruff format --check . && uv run ty check
uv run pytest -m "not e2e" && uv run pytest -m e2e

# arnes
tests/check_harness_determinism.sh
tests/check_waterfall_incremental.sh
tests/check_harness_natural.sh

# ARM + banco QEMU (volumen y chroot privados)
VOLUMEN=x6200-build-output-dfl173 BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot \
    tools/compila-gui.sh
CHROOT=/home/pablo/desa/asf/state/dfl173-chroot VOLUMEN=x6200-build-output-dfl173 \
    tools/captura-banco.sh /tmp/home.png
```

## 8. PENDIENTES / decisiones para el owner

1. **Validación física en la radio** (`requires-radio`): el banco QEMU no tiene
   FFT ni audio real; el espectro del banco no muestra señal. Las capturas ARM
   prueban el **render** del binario real, no la recepción.
2. **TSan** corre en host x86_64, no en el Cortex-A7 (mismo fuente, no el binario
   ARM): es evidencia del código de sincronización, no del ARM.
3. **Setup del chroot del banco**: el rootfs extraído no trae `/mnt/params.db`
   sembrado; la primera corrida creó una base vacía y la GUI murió con
   `subject_get_int con sujeto NULL` (params_init no abrió la base). Se sembró
   `/mnt/params.db` desde `/usr/share/x6200/params.default.db`. **No es de este
   cambio** (es del chroot); queda anotado por si el runbook del banco debe
   sembrar la base.
4. **Texto de BW**: se dibuja sólo si entra entre las dos líneas (formato `3.3k`
   para múltiplos exactos de 100 Hz ≥ 1000, entero en Hz en el resto); si no
   entra, se omite, y **nunca** se agranda el filtro para que entre.
5. **Alcance PR 2**: el editor DFL (diálogo, quinta tecla, `ACTION_APP_DFL_EDITOR`,
   restablecer) **no** está en esta corrida.

## 9. Cómo deshacer

Revertir `912f5d31` y `f5f3f14f`: vuelve el relleno de filtro original
(`bg_color` a `LV_OPA_50`), se quitan los ticks, la región `waterfall_ticks`, los
`expected_changes`, las claves `filter_low`/`filter_high` y los fixtures `dfl-*`.
No hay migración de datos ni cambio de paleta.
