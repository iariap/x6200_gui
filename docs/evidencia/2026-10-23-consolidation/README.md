# Consolidación `legacy → main` (PR #254) — evidencia

**Fecha:** 2026-10-23
**Repo:** `iariap/x6200_gui`
**Rama de trabajo:** `factory/consolidate-legacy-main` (creada en el **tip de la legada**; **no** se pusheó nada a `lu2dsx/x6200-rom`)
**Worktree:** `state/worktrees/x6200/consolidate-legacy-main`
**Estado: NO-GO** — el merge integra las dos líneas sin perder trabajo, pero **no** es mergeable a `main` hasta que el owner decida la semántica de audio/PSD de `src/dsp.cpp` (ver §4). No se afirma compatibilidad BASE 1.0.7 vs radio BASE 1.0.6.

## 1. Contexto medido (SHAs inmutables)

```
origin/main                936e75e96e9a3eea99775b72c4242a6ecff5fca3
origin/lu2dsx/x6200-rom    7877852f0720f893d14370b15f14294f5f243b5e   (incluye #252)
merge-base                  d4fb57f8ff8ccbcfdef13fc191cc9977511b6845
```

- `main`-only: **12 commits** (`git log --oneline origin/lu2dsx/x6200-rom..origin/main`).
- legada-only: **37 commits** (`git log --oneline origin/main..origin/lu2dsx/x6200-rom`).
- El merge se hizo con `git merge origin/main --no-commit --no-ff` en la rama nueva; **5 archivos** entraron en conflicto.

Merge commit: `ed531177b16f0341415ea5b22e7771a959445ba7` (árbol integrado sobre el que corrieron los gates)
Commit de evidencia: HEAD de la rama (agrega sólo `docs/evidencia/2026-10-23-consolidation/**`, sin cambios de código)
Padres: `7877852f…` (legada) + `936e75e9…` (main).

Comandos:

```
git -C work/x6200_gui worktree add -b factory/consolidate-legacy-main \
    state/worktrees/x6200/consolidate-legacy-main origin/lu2dsx/x6200-rom
cd state/worktrees/x6200/consolidate-legacy-main && git merge origin/main --no-commit --no-ff
```

## 2. Conflictos y cómo se resolvieron

| # | Archivo | Naturaleza | Resolución |
|---|---|---|---|
| 1 | `src/dsp.cpp` | **Semántico (audio/PSD/DSP)** — límite duro | **Unión** de ambos observers, **marcado NO-GO** (ver §4) |
| 2 | `src/dialog_settings.c` | Clock / HOME #250 | Lado de `main` |
| 3 | `docs/manual/MANUAL.md` | Clock / HOME #250 | Lado de `main` |
| 4 | `docs/manual/MANUAL.html` | Clock / HOME #250 | Lado de `main` |
| 5 | `docs/settings-order.md` | Clock / HOME #250 | Lado de `main` |

### 2.1 `src/dialog_settings.c` — lado de `main` (HOME #250)

La legada sólo tocó ese bloque para el texto del label de **#221** (`"Timeout Clock, Power, TX"`
→ `"Clock / Power / TX (s)"`) **dentro de `make_clock()`**, una fila que `main` **elimina** por
HOME #250 (reloj permanente: la hora ya no alterna con la alimentación). `main` reemplaza las
callbacks + `make_clock()` por un comentario y deja `clock_set_view`/`clock_set_*_timeout`
exportadas e inertes (coherente con `src/clock.c` de `main`, que es el que quedó en el árbol).

```
<<<<<<< HEAD              (legada)
static uint8_t make_clock(uint8_t row) { ... lv_label_set_text(obj, "Clock / Power / TX (s)"); ... }
=======                   (main)
/* (la fila `Clock view` y la de los timeouts vivian aca: issue #250) */
>>>>>>> origin/main
```

Se conserva el lado de `main`: la fila no existe en la UI implementada, así que su label no
aplica. La **única** aportación real de la legada a ese archivo (`audio_enh_update_cb` →
`audio_enh_apply_mode_preset()`, fix de audio CW de #232) vive en **otro** hunk, no conflictivo,
y quedó en el árbol (ver §3).

### 2.2 `docs/manual/MANUAL.md`, `docs/manual/MANUAL.html`, `docs/settings-order.md` — lado de `main`

Los tres describen la pestaña `Clock` del manual/orden de Ajustes. La legada listaba las filas
`Clock view / Clock / Power / TX (s)`; `main` (HOME #250) las retiró.

```
<<<<<<< HEAD
| **Clock** | Day, Month, Year / Hour, Min, Sec / Clock view / Clock / Power / TX (s) | Hora UTC ... y qué muestra el reloj |
=======
| **Clock** | Day, Month, Year / Hour, Min, Sec | Hora UTC ... El reloj de la barra superior muestra siempre la hora, y la alimentación/batería va en una banda compacta debajo (issue #250) |
>>>>>>> origin/main
```

Son descripciones de la **UI implementada**, no registros acumulativos: una "unión" describiría
filas que ya no existen. Se toma la versión de `main` (que coincide con el código: `dialog_settings.c`
sin `make_clock` y `src/clock.c` de `main`, idéntico a `main` en el árbol). La variante de la
legada se conserva acá como fragmento. `MANUAL.html` se editó a mano para seguir a `MANUAL.md`
(no hay generador en el repo).

## 3. Demostración: no se perdió trabajo de ninguna línea

### 3.1 Qué aporta cada padre al árbol integrado

```
$ git log --oneline 936e75e9..HEAD     # lo que aporta la LEGADA
ed531177 merge: consolidacion legacy (lu2dsx/x6200-rom) -> factory/consolidate-legacy-main
7877852f fix(audio): el ruteo record/mic del MCU se restaura SIEMPRE (mute de RX tras CW, #244) (#252)
f572438c docs: hallazgo #65 — el disparo del audio mudo de #244 no esta identificado (#248)
6f7cf881 docs: #63 — el "roam" WiFi es NM cambiando a "DharmaNet 2.4" (Refs #243) (#245)
301fd188 audio RX: un solo camino para el nivel y re-sync al salir de CW (#244) (#247)
5b2f93de [ROM][P1] S02update: la re-entrada del updater no puede volver a correr el update (#246)
... (+ la ola #211/#216 y toda la evidencia)                             (38 líneas, 37 commits únicos)

$ git log --oneline 7877852f..HEAD     # lo que aporta MAIN
ed531177 merge: consolidacion legacy (lu2dsx/x6200-rom) -> factory/consolidate-legacy-main
936e75e9 [HOME] Improve top-left status typography and icon spacing (#259)
d6d20b06 [HOME] Keep clock visible and render compact battery status (#258)
09ff910b Merge pull request #255 from iariap/chore/main-only-policy-20261023
49477ecc Merge pull request #253 from iariap/docs/main-primary-branch-20261023
0b6f9abc docs: require main-only branch and safe legacy retirement
804a2b17 docs: establish main as primary development branch
8549891e Merge pull request #223 from iariap/factory/trunk-to-main
4fe4edd1 Evidencia del merge trunk->main (docs/evidencia/2026-10-22-mainmerge)
895bfbac Sincronizar main con el trunk (lu2dsx/x6200-rom)
e1476ece Bump version
d6776bd9 Add CW zoom control
c74df4a3 Fixes for 1.0.7 BASE, some changes                                          (13 líneas, 12 commits únicos)
```

### 3.2 Diffs de árbol contra cada padre

```
$ git diff --shortstat 7877852f HEAD    # todo lo que trae main al árbol
 590 files changed, 156033 insertions(+), 666 deletions(-)
$ git diff --shortstat 936e75e9 HEAD    # todo lo que trae la legada al árbol
 1966 files changed, 229469 insertions(+), 794 deletions(-)
```

`HEAD` vs legada difiere sólo en lo que `main` aporta; `HEAD` vs `main` difiere sólo en lo que
la legada aporta. Los 3 archivos de código que difieren de **ambos** padres son los resueltos
(`dsp.cpp` unión, `dialog_settings.c` y `radio.c`/`cfg.c` por auto-merge de dos hunks disjuntos).

### 3.3 Hitos verificados presentes en el árbol integrado

De la **legada** (todo el hardening + fixes):

| Hito | Evidencia en `HEAD` |
|---|---|
| #231 fork bomb / re-entrada del updater | `tools/release/update-overlay/etc/init.d/S02update:98` — *"re-entrada no tiene tope: es el fork bomb. La marca viaja en el entorno"* |
| #244 / #252 audio RX (mute tras CW) | `src/radio.c:349` `radio_vol_apply()/on_cur_mode_change_vol()`; `:619` `x6200_control_record_set(false)`; `:703` `radio_vol_apply()`; `:556` observer de `cfg_cur.mode` |
| #232 perfil de audio CW | `src/audio_enhancement.c:331` `audio_enh_apply_mode_preset()`; llamada desde `src/dialog_settings.c:397` y `src/dsp.cpp:306` |
| #211 SQLite fail-closed | `src/params/db.c`, `src/sql_policy.{c,h}` |
| #211 supervisor GUI (backoff/logs únicos) | `rootfs/usr/sbin/x6200_daemon` (gate `daemon`: 15 casos, 0 fallos) |
| #216 ola (wifi wakeups #234, CW redraws #233, flow sleep #235) | `tests/native/test_wifi_idle_timer.cpp`, `tests/native/radio_thread_test_main.c` (57 comprobaciones, 0 fallos) |
| #205 Auto Tune multimodo | `src/autotune_spectrum.{c,h}`, `tests/native/test_autotune_spectrum.c` |
| #242 cobertura de diálogos QEMU | `tests/check_dialog_coverage.py`, `tests/qemu/dialog-coverage.csv` |

De **`main`** (upstream BASE 1.0.7 + HOME):

| Hito | Evidencia en `HEAD` |
|---|---|
| #223 contrato `cfg_cur.fft_dec`/`fft_width` | `src/cfg/cfg.h:131-132` |
| #223 `MFK_CW_ZOOM` al final del enum persistido | `src/mfk.h:74` (antes de `MFK_LAST`; con el comentario append-only) |
| #223 nueva ruta DSP/medidor | `src/dsp.cpp` `fft_width`/`dsp_update_min_max` con `meter_set_noise`; `src/radio.c:231` `samples[i] = pack->samples[i] - 126.0f`; `:234` `dbm = -(int16_t)pack->dbm + 4`; `:558` `cfg_cur.fft_dec`; `:559` `cfg.fft_zoom_cw.val` |
| #223 mocks | `tools/ui_harness/mocks/mock_spectrum.h`; `tools/ab_testing/quality/contracts.yaml` |
| #249 HOME (clock/info) | `src/clock.c` y `src/info.c` **idénticos a `main`** (`git diff 936e75e9 HEAD -- src/clock.c src/info.c` → vacío) |
| #253/#255 política main-only | `CONTRIBUTING.md`, `README.md` |

## 4. LÍMITE DURO — `src/dsp.cpp` (audio/PSD/DSP): NO-GO

**No se decide acá.** Los dos fragmentos:

**(A) `main` — upstream BASE 1.0.7 (#223):** reescribe el pipeline PSD/DSP.

```cpp
// src/dsp.cpp (origin/main)
// AveragedPSD<RADIO_SAMPLES, SPECTRUM_NFFT>::add_samples()
-   liquid_vectorf_add(psd.data(), samples, psd.size(), psd.data());
+   for (size_t i = 0; i < psd.size(); i++) {
+       psd[i] += powf(10.0f, samples[i] * 0.1f);   // dB -> potencia
+   }
// AveragedPSD::get()
-   averaged[i] = 20.0f * log10f(averaged[i] + 1e-12f) - 30.0f;
+   averaged[i] = 10.0f * log10f(averaged[i] + 1e-12f);
// dsp_update_min_max() reescrito: window = 2700*RADIO_SAMPLES/fft_width,
// offset por zoom (18/15/12/9/6/3 dB) y meter_set_noise(min)
static uint32_t fft_width = FFT_FULL_WIDTH;      // (era uint8_t spectrum_factor)
static void on_fft_dec_change(Subject *s, void *u) { fft_width = subject_get_int(cfg_cur.fft_width); }
```

**(B) legada — Auto Tune multimodo #205:** consume **esa misma** PSD promediada.

```cpp
// src/dsp.cpp (7877852f)
static void dsp_autotune_publish(const float *db, size_t n, bool tx) {
    bool ok = !tx && db != NULL && n == (size_t) SPECTRUM_NFFT && atune_width > 0;
    for (size_t i = 0; i < (size_t) SPECTRUM_NFFT; i++) { atune_snapshot.db[i] = (i < n) ? db[i] : 0.0f; ... }
    atune_snapshot.width_hz = atune_width;    // fft_width del contexto
    ...
}
// y en dsp_samples(): dsp_autotune_publish(spectrum_avg_data->data(), spectrum_avg_data->size(), tx);
```

**El conflicto (2 hunks):** el forward-decl y `dsp_init()` — la legada suscribe
`on_cur_fft_width_change` y hace `atune_center = …`; `main` suscribe `on_fft_dec_change`.

**Resolución (unión, por decisión del owner de esta corrida):**

```cpp
static void on_cur_fft_width_change(Subject *subj, void *user_data);
static void on_fft_dec_change(Subject *subj, void *user_data);
...
cfg_cur.fft_width->subscribe(on_cur_fft_width_change)->notify();   // #205 (contexto del Auto Tune)
atune_center = subject_get_int(cfg_cur.fg_freq);
/* Consolidacion legacy->main (2026-10-23): quedan suscritos LOS DOS observers ... NO-GO */
cfg_cur.fft_width->subscribe(on_fft_dec_change)->notify();        // #223 (fft_width del DSP/medidor)
```

**Por qué es semántico y por qué NO es un GO.** La unión no descarta ninguna línea de ninguna
rama, pero **combina comportamientos**: el Auto Tune #205 lee la PSD que `main` cambió de escala
(`20·log10−30` → `10·log10`) y de entrada (`samples[i] − 126.0f`, `dbm + 4`), y `autotune_spectrum.c`
usa **diferencias en dB** contra umbrales absolutos (`AUTOTUNE_AM_PEAK_MIN_DB`,
`AUTOTUNE_AM_DOMINANCE_MIN_DB`, `−3.0f`, `clamp(...,0,24)`) y mapea bin→Hz con `width_hz / 512`.
Cambiar la escala y el offset de las muestras altera esos umbrales/márgenes. Decidir si el Auto
Tune sigue siendo correcto sobre el pipeline 1.0.7 (y por lo tanto si la **radio BASE 1.0.6** es
compatible con lo que se despliega) es **decisión del owner** — la advierten la épica #249 y el
PR #254. **No se afirma compatibilidad ni un GO.**

Pendiente de decisión del owner: (a) validar #205 sobre el pipeline PSD 1.0.7 y ajustar
umbrales/escala si hace falta; (b) o declarar el Auto Tune #205 fuera de alcance hasta
validarlo en hardware; (c) o aportar el contrato de hardware que haga falta.

## 5. Gates (salida real; SKIP/BLOCKED nunca es PASS)

Log completo en `gates.txt` de este directorio. Resumen:

| Gate | Resultado |
|---|---|
| `tests/run_all_tests.sh --quick` | **RC=0 — 21 OK, 0 fallos** (incluye `dialog_coverage`, `drawbuf_partial`, `daemon`, `db_failclosed`, `navtex`) |
| `tests/native/run_native_tests.sh` | **RC=0** (57 comprobaciones, 0 fallos) |
| `tests/native/run_native_tests.sh --ci-parity` | **RC=0** — `ci-parity: OK (build host sin lvgl/)` |
| `tests/check_harness_determinism.sh` | **RC=0** — `PASS: deterministic` |
| `tests/check_harness_natural.sh` | **RC=0** — `PASS: natural bench deterministic, profiled and delimited` |
| `tests/check_harness_wakeups.sh` | **RC=0** — `PASS: wakeup profile measured, sourced and delimited` |
| `python3 tests/audita-pruebas.py` | **RC=0** — `OK: estructura y evidencia en orden` |
| `scripts/apps/x6200/check-matrix.sh origin/main HEAD <wt>` | **RC=0** — todos los checks `ok`, incluido `x6200-ab contracts`; `check_wcag (temas)` = SKIP (sin cambios de temas); **banco QEMU = BLOCKED** (entorno del owner ausente) |
| `uv run x6200-ab themes` | **RC=0** |
| `uv run x6200-ab contracts --revision ed531177` | **ok** (corrido por la matriz) |
| `uv run x6200-ab compare --a 936e75e9 --b ed531177` | **RC=0 — PASS** (Functional/Visual/Quality Contract/Performance PASS; Hardware/Energy BLOCKED sin radio). A/B host-side: **no** valida la semántica de audio/PSD de §4 ni convierte el NO-GO en GO |
| Build ARM / ROM | **PENDING** (sin toolchain cross; ver `gates.txt` §12) |
| Validación física en radio | **PENDING_HARDWARE** |

SKIPs honestos: `qemu_apps` (entorno privado del banco), `wspr`/`sstv`/`wefax` (la imagen `x6200-uiharness` es el target `ci`, sin `fftw3`/`sndfile`), `js8` (omitido por `--quick`), `webserver` (falta el checkout `../.x6200_webserver`).

Nota de entorno: el submódulo `lvgl` no estaba inicializado en el worktree nuevo. Sin
`git submodule update --init lvgl`, `drawbuf_partial` y el gate de determinismo fallan con un
error de cmake (`/src/lvgl does not contain a CMakeLists.txt`). Tras inicializarlo, todo pasa.

El `check_dialog_coverage` que la corrida anticipaba como FAIL preexistente **pasó** (con
`origin/main` como base y también standalone).

## 6. Pendientes / decisiones del owner

1. **NO-GO**: decisión sobre `src/dsp.cpp` (§4). Sin esa decisión el merge no debe ir a `main`.
2. Validación física en la radio: **PENDING_HARDWARE** (esta corrida no flasheó, no usó SSH, no transmitió).
3. `.factory/app.json` de la app todavía declara `"branch": "lu2dsx/x6200-rom"` — es gobernanza
   (`medium`) y parte del paso 6 de #256: migrar la referencia a `main` **fuera** de esta corrida.
4. PR #254 (draft, `CONFLICTING`) y el retiro de la rama legada: pasos posteriores de #256/#257,
   **no** de esta corrida.
