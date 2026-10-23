# Issue #288 — Identidad X6200 y copy en inglés (Field Ops / SWRScan / Propagation)

**Épica**: #284 (UI POLISH). **Contrato**: issue #288, vinculante (mapeo literal).
**Rama**: `feat/ui-polish-app-copy-284` · **Worktree**:
`state/worktrees/x6200/ui-polish-app-copy-284`.
**Base de esta fase**: `eea3ed2022fc0f3be8c0afa1c318e32e42455ce8` (`origin/main` al
abrir la corrida, 2026-10-23). **Fecha**: 2026-10-23.

## Veredicto en una línea

**FASES 1 y 1b completas**: se aplicaron los **37 literales visibles** del contrato
más los **11 de §1bis** que completó el coordinador (más 5 pines de test) **sin
tocar** ADIF/JS8/APIs/comentarios, y la verificación **estructural** (compilación ARM
de las tres unidades, placeholders `%`, truncación de `snprintf` con los peores
casos, ASCII) pasa limpia. La evidencia **visual antes/después 800×480** queda
**PENDIENTE PARA LA FASE 2**, después del rebase sobre #285/#286 (ver §8):
capturarla antes daría capturas de una superficie visual obsoleta.

## 1. Mapeo aplicado (archivo → viejo → nuevo)

Sólo literales de runtime visibles. Los especificadores de formato y los saltos de
línea se preservaron (verificado en §4.2).

### `src/dialog_field.c` (1)

```
"X6100" -> "X6200"            (header visible, lv_label_set_text del head)
```

### `src/dialog_swrscan.c` (2)

```
"sin datos" -> "no data"                          (abort_text, SWR_ABORT_NO_DATA)
"sin datos medidos" -> "No measurements"          (format_readout, estado vacío)
```

### `src/dialog_propagation.c` (34)

Botones (label inicial **y** callers dinámicos de `button_update_label`):

```
"Repetir"  -> "Repeat"        (.label de btn_repeat)
"Escuchar" -> "Listen"        (.label de btn_listen y listen_stop())
"Cerrar"   -> "Close"         (.label de btn_close)
"Parar"    -> "Stop"          (button_update_label al escuchar)
```

Banda / veredicto / evidencia:

```
"la banda actual"                                  -> "current band"          (x2: start_sweep y construct_cb)
"CERRADA"                                          -> "CLOSED"                (verdict_es)
"SIN BEACONS IBP"                                  -> "NO IBP BEACONS"
" | no se escucho"                                 -> " | not heard"          (ladder del evidence_text)
"Banda %s  |  %d beacons IBP aplican  |  %d en el plan" -> "Band %s  |  %d eligible IBP beacons  |  %d scheduled"
"Banda %s  |  sin beacons IBP"                     -> "Band %s  |  no IBP beacons"
"Banda %s"                                         -> "Band %s"
```

Pasos y estado:

```
"Paso 1/4  |  Preparando"     -> "Step 1/4  |  Preparing"
"Paso 2/4  |  Sincronizando"  -> "Step 2/4  |  Syncing"
"Paso 3/4  |  Midiendo"       -> "Step 3/4  |  Measuring"
"Paso 4/4  |  Cancelado"      -> "Step 4/4  |  Cancelled"
"Paso 4/4  |  Resultado"      -> "Step 4/4  |  Result"
```

Reloj NTP / slot:

```
"Reloj NTP: %s"                                    -> "NTP clock: %s"
"sincronizado"                                     -> "synchronized"
"SIN sincronizar (el turno puede estar corrido)"   -> "NOT synchronized (slot timing may drift)"
"OK: sincronizado"                                 -> "OK: synchronized"
"NO: sin sincronizar"                              -> "NO: unsynchronized"
"Reloj NTP: %s\nTransmitiendo ahora: %s (%s, quedan %d s)\n%s"
    -> "NTP clock: %s\nBeacon on air: %s (%s, %d s left)\n%s"
"Turno alineado con el reloj del equipo."          -> "Slot aligned with radio clock."
"El turno de los beacons puede estar corrido (sin NTP)."
    -> "Beacon slot timing may drift without NTP."
"Esperando su slot..."                             -> "Waiting for slot..."
"Slot en curso: midiendo la escalera..."           -> "Slot active: measuring power steps..."
```

Notas / resultado:

```
"Sin beacons IBP en %s. La IARU no respalda beacons debajo de 14 MHz; abajo van los spots del cluster, si los hay."
    -> "No IBP beacons on %s. IARU IBP beacons start at 14 MHz; cluster spots follow if available."
"Sin beacons IBP en %s: la IARU no respalda beacons debajo de 14 MHz. Abajo van los spots del cluster, si hay."
    -> "No IBP beacons on %s: the IBP network starts at 14 MHz. Cluster spots appear below if available."
"Cancelado por el operador. VFO restaurado."        -> "Cancelled by operator. VFO restored."
"No hubo slot disponible para medir. VFO restaurado." -> "No measurement slot available. VFO restored."
"No se escucho ningun beacon en su slot: silencio no es prueba de banda cerrada (puede ser QRM/QRN)."
    -> "No beacon heard in its slot. Silence does not prove the band is closed (QRM/QRN possible)."
"Umbrales: abierta >= %d dB, marginal >= %d dB."    -> "Thresholds: open >= %d dB, marginal >= %d dB."
"Umbrales:\n%d/%d dB"                              -> "Thresholds:\n%d/%d dB"
"El equipo dejo de estar en RX: barrido cancelado. VFO restaurado."
    -> "Radio left RX: scan cancelled. VFO restored."
"Banda actual y turno de los beacons listos.\nReloj NTP: %s.\nPulsa Run (o Repetir) para medir."
    -> "Current band and IBP schedule ready.\nNTP clock: %s.\nPress Run or Repeat to measure."
"Beacon %d de %d: %s en %u.%03u MHz\n%s\n%s"        -> "Beacon %d of %d: %s at %u.%03u MHz\n%s\n%s"
"Medidos %d de %d beacons IBP, %d escuchados. Registro IARU R2 (10 m): %d activos de %d."
    -> "Measured %d/%d IBP beacons, %d heard. IARU R2 (10 m): %d active of %d."
"Medidos %d de %d beacons IBP, %d escuchados."      -> "Measured %d/%d IBP beacons, %d heard."
"Escuchando %s en %u.%03u MHz: Parar/Cerrar vuelve al VFO.\n"
    -> "Listening to %s at %u.%03u MHz: Stop/Close restores VFO.\n"
```

Mensajes de `msg_update_text_fmt`:

```
"Propagation: frecuencia actual invalida"                  -> "Propagation: invalid current frequency"
"No se pudo crear el hilo del barrido."                    -> "Could not start scan worker."
"Propagation: no hay un beacon escuchado para sintonizar"   -> "Propagation: no heard beacon available to tune"
```

### `tests/native/test_swrscan.c` / `test_swrscan_multipass.c` (5 pines de la app)

Los pines que fijaban literales cambiados (el suite compila la **fuente real**
`src/dialog_swrscan.c` con stubs). Sin estos cambios el suite quedaría en rojo por
el pin viejo, no por una regresión:

```
test_swrscan.c:816          "sin datos medidos"  -> "No measurements"        (readout vacio)
test_swrscan.c:798          "Stopped (parcial)"  -> "Stopped (partial)"
test_swrscan.c:853          "Error: timeout de RF" -> "Error: RF timeout"
test_swrscan.c:720          strstr(..., "validacion RF") -> strstr(..., "RF validation")
test_swrscan_multipass.c:441 "Stopped (parcial)"  -> "Stopped (partial)"
```

El de `:720` es un `strstr` (no un `strcmp`): el test verifica el **motivo visible**
del gate de multipasada, así que se actualizó a la subcadena del texto nuevo
(`"3/5 passes require RF validation"`). Es el que se escapó en la primera pasada y
lo cazó el suite nativo (`461 comprobaciones, 1 fallos`): quedó registrado acá
porque es exactamente el tipo de pin que un grep por el literal completo no ve.

### Ajuste de buffer (derivado de la verificación, no del contrato)

`thresholds_label_fn` tenía `static char buf[32]` y **ya** disparaba
`-Wformat-truncation=2` con `"Umbrales:\n%d/%d dB"` (peor caso 37 B en 32). Con
`"Thresholds:"` el peor caso sube a **38 B**: se agrandó a `buf[48]`. El warning
desaparece (ver §4.1) y el contrato pide explícitamente verificar los destinos de
`snprintf` sin truncación.

## 1bis. Adiciones del coordinador (vetables)

**Motivo**: la épica #284 manda todo el copy de operador en inglés y el contrato
tradujo `CERRADA`→`CLOSED` pero **no** `ABIERTA`, así que la misma pantalla habría
quedado **`ABIERTA` / `MARGINAL` / `CLOSED`** — peor que el estado anterior. Se
completan los huecos de §3 con **traducción literal** (sin wording creativo). **Son
una adición del coordinador sobre el contrato del issue #288: vetables.**

### `src/dialog_propagation.c` (4 literales)

```
"ABIERTA"                                -> "OPEN"                  (par de CLOSED; verdict_es)
"Hacia %s (%s): %s %d deg, %.0f km."     -> "Toward %s (%s): %s %d deg, %.0f km."   (fill_direction)
"el beacon"                              -> "the beacon"            (x2: fallback de entity en
                                                                      fill_direction y de call en
                                                                      la nota de listening)
```

### `src/dialog_swrscan.c` (7 literales: motivos de abort y estado visible)

```
"timeout de RF"                          -> "RF timeout"                            (abort_text)
"sin TX"                                 -> "no TX"                                 (abort_text)
"cambio de estado"                       -> "state change"                          (abort_text)
"3/5 pasadas requieren validacion RF"    -> "3/5 passes require RF validation"      (abort_text)
"span/centro fuera de la banda de TX"    -> "span/center outside the TX band"       (abort_text)
"parametros invalidos"                   -> "invalid parameters"                    (abort_text)
"Stopped (parcial)"                      -> "Stopped (partial)"                     (estado visible)
```

**Interpretación de las abreviaturas del coordinador**: su mapeo decía
`"3/5 pasadas…" → "3/5 passes..."` y `"span/centro…" → "span/center..."`. Los
literales reales son **más largos** (`"3/5 pasadas requieren validacion RF"`,
`"span/centro fuera de la banda de TX"`); se tradujo el texto **completo**
(`"3/5 passes require RF validation"`, `"span/center outside the TX band"`) para no
borrar información (regla del contrato: nunca borrar información en silencio). Si
el owner prefiere el texto recortado, es un cambio de una línea.

Todos son **runtime-visibles**: `verdict_es` → label del veredicto;
`fill_direction` → `status.direction` → `lbl_detail`; `abort_text` →
`dialog_swrscan_state_text()` (`"Error: %s"`) → línea de estado; y el fallback de
callsign → nota de listening. Ninguno es sólo log ni comentario.

`"sin datos"` **no** se duplicó: ya se había mapeado a `"no data"` en la Fase 1 y
`abort_text` sigue siendo la **única** fuente del motivo.

## 2. Qué NO se tocó, y por qué

| Superficie | Por qué queda igual |
| --- | --- |
| `src/field/flog.c` `PROGRAMID` (`X6100-IU1WXY`) | Es **dato de protocolo** del ADIF, no el header visible. El contrato lo prohíbe explícitamente. |
| Enums/tipos/comentarios `X6100` (`dialog_field.c:4`, `:66-67`; `test_field.c`) | No son strings de UI (nombres heredados y evidencia histórica). |
| Metadatos/enums JS8, `third-party/js8core` | No son copy de UI. |
| Nombres de APIs públicas (`verdict_es`, `abort_text`, `dialog_*_remote*`, etc.) | El contrato prohíbe renombrarlas; sólo cambia el texto que devuelven. |
| Comentarios en español de los tres archivos | "No reemplazar frases en español en comentarios". Grep de control en §3. |
| Evidencia histórica (`docs/PRUEBAS.md`, `docs/NEW_FEATURES.md`, `docs/evidencia/**`) | Es registro del SHA que midió; no se reescribe. |
| `"Run"`, `"Cancel"`, `"Clock not synchronized: beacon timing may be inaccurate."`, `"Set QTH in Settings to calculate direction."` | Ya estaban en inglés correcto. |
| DSP/GPS/radio/PTT/antena/protocolos/IDs/timing/threads/settings de idioma | Fuera del contrato (sólo presentación). |

Control de que los literales viejos ya no están (los 37 del contrato **y** los 11 de
§1bis; sólo sobreviven en **comentarios**, que el contrato manda no tocar):

```
$ grep -n 'X6100"|"sin datos"|sin datos medidos|"Repetir"|...|no hay un beacon|
           "ABIERTA"|"Hacia |"el beacon"|"timeout de RF"|"sin TX"|"cambio de estado"|
           "3/5 pasadas|"span/centro|"parametros invalidos"|"Stopped (parcial)"' \
      src/dialog_field.c src/dialog_swrscan.c src/dialog_propagation.c
src/dialog_propagation.c:6:   *  App a demanda para la banda actual: ...
src/dialog_propagation.c:256: /* El hilo del barrido avisa a la GUI. ...
src/dialog_propagation.c:439:  * puede atribuir a un escalon que no se escucho. */
src/dialog_propagation.c:626: /* El equipo dejo de estar en RX (PTT del operador, ...
src/dialog_propagation.c:725: /* Plan del barrido para la banda actual: ...
```

(En `tests/native/` quedan menciones a `"timeout de RF"`/`"parcial"` sólo en
comentarios y en las **copias de build** gitignored; los pines de código están
actualizados, §1.)

## 3. Huecos del contrato — **cerrados en la Fase 1b (§1bis)**

Estos literales **españoles visibles** existían en el código pero **no** estaban en
el mapeo del contrato. En la Fase 1 quedaron **sin tocar** (regla "no additional
wording without owner approval"); el coordinador los completó después con
traducción literal: **ver §1bis**. La tabla queda como registro de lo detectado:

| Archivo:línea | Literal original | Contexto | Estado |
| --- | --- | --- | --- |
| `src/dialog_propagation.c:1056` | `"ABIERTA"` | veredicto OPEN (`verdict_es`) | → `"OPEN"` (§1bis) |
| `src/dialog_propagation.c:538` | `"Hacia %s (%s): %s %d deg, %.0f km."` | dirección del mejor beacon | → `"Toward …"` (§1bis) |
| `src/dialog_propagation.c:539` | `"el beacon"` | fallback de `entity` | → `"the beacon"` (§1bis) |
| `src/dialog_propagation.c:1338` | `"el beacon"` | fallback del callsign en listening | → `"the beacon"` (§1bis) |
| `src/dialog_swrscan.c:330-336` | `"timeout de RF"`, `"sin TX"`, `"cambio de estado"`, `"3/5 pasadas requieren validacion RF"`, `"span/centro fuera de la banda de TX"`, `"parametros invalidos"` | motivos de abort (`abort_text`) | → 6 literales (§1bis) |
| `src/dialog_swrscan.c:357` | `"Stopped (parcial)"` | estado visible | → `"Stopped (partial)"` (§1bis) |

No hubo ningún literal del contrato **ausente** ni **cambiado**: los 37 se
encontraron al SHA de la base, y los 11 de §1bis también (verificados uno por uno
con `grep -n` antes de tocarlos).

## 4. Verificación estructural

Reproducible con `sh docs/evidencia/2026-10-23-ui-polish/app-copy/run.sh`
(salida completa en `output.txt`).

### 4.1 Compilación C/ARM de las tres unidades

Toolchain **real del target**: `arm-buildroot-linux-gnueabihf-gcc 12.3.0`
(Buildroot 2023.02.11), flags de firmware de `CMakeLists.txt`
(`-mtune=cortex-a7 -mcpu=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4`) más
`-Wall -Wextra -Wformat=2 -Wformat-security -Wformat-truncation=2`.

```
dialog_propagation: exit 0 (3 warning(s))
dialog_field:       exit 0 (30 warning(s))
dialog_swrscan:     exit 0 (13 warning(s))
dialog_propagation.o: ELF 32-bit LSB relocatable, ARM, EABI5 version 1 (SYSV)
```

Comparación **antes vs después** del conjunto de warnings (base
`eea3ed20:src/<f>.c` vs árbol): `dialog_field` y `dialog_swrscan` **idénticos**;
`dialog_propagation` pasa de 4 a 3. Evidencia completa: `warning_diff.txt`.

```
--- dialog_propagation: base=4 head=3
1,2c1
<   1 warning: '%.0f' directive output may be truncated ... region of size between 130 and 141
<   1 warning: '%d' directive output may be truncated ... region of size between 10 and 20
---
>   1 warning: '%.0f' directive output may be truncated ... region of size between 128 and 140
--- dialog_field: base=30 head=30
--- dialog_swrscan: base=13 head=13
```

Lectura exacta (para que el diff no se lea como un warning nuevo):

- el de **`%d`** (`thresholds_label_fn`) **desaparece** (Fase 1, `buf[48]`);
- el de **`%.0f`** de la línea 538 (`fill_direction`) **ya existía en la base**:
  mismo archivo:línea, mismo directivo, misma causa (el `%.0f` teórico de 310 B
  contra un buffer de 160). Sólo se mueve la **cota numérica** de la región
  (130..141 → 128..140) porque los literales de esa llamada son más largos
  (`"Toward"` vs `"Hacia"`: +1 B; `"the beacon"` vs `"el beacon"`: +2 B);
- **ningún warning nuevo** (ni de clase ni de línea) y el total **baja** de 4 a 3.

No se agrandó `direction[160]` para "apagar" ese warning: el peor caso real de la
línea (entidad DXCC ≤40, grilla 6, rumbo 3 dígitos, km 5) son ~99 B, medido en
`truncation_check.c`; agrandar el buffer sólo movería el número del mensaje.

**Límite honesto**: el **link completo** de la GUI ARM no se pudo correr en este
entorno: el sysroot de Buildroot del host no trae los start files (`Scrt1.o`,
`crti.o`), así que `cmake` falla en `CMakeTestCCompiler.cmake` (`ld: cannot find
Scrt1.o`). Es una carencia del entorno, no del cambio; las tres unidades que toca
este issue compilan y codegenan para ARM (objetos ELF32 arriba). El build de imagen
completo queda para el CI/matriz del coordinador.

### 4.2 Placeholders `%`: mismos conteos y tipos

`verify_placeholders.py` compara, para **cada literal de formato** de los tres
archivos, la secuencia de especificadores `%` contra `git show <base>:<archivo>`
(multiconjunto; falla si cambia el conteo, el orden o la conversión). La base por
defecto está pinneada en `run.sh`
(`eea3ed2022fc0f3be8c0afa1c318e32e42455ce8`, el `origin/main` sobre el que se aplicó
el mapeo), así el check sigue comparando contra la versión **sin** el cambio después
de commitear; en el rebase de la Fase 2 se pasa `BASE=<sha nuevo>`.

```
OK   src/dialog_propagation.c: 33 format literals, 77 specifiers preserved
OK   src/dialog_swrscan.c:     10 format literals, 31 specifiers preserved
OK   src/dialog_field.c:      100 format literals, 212 specifiers preserved
```

### 4.3 `snprintf` sin truncar (peores casos)

`truncation_check.c` reproduce **cada** `snprintf` cuyo formato cambió, con los
tamaños de destino **reales** de las fuentes (`status.note[192]`, `buf[256]`,
`verdict[32]`, `run_label[8]`, `thresholds[48]`, `readout[64]`, `note[512]`,
`line[256]`, `state[64]` de `dialog_swrscan_state_text`, `direction[160]` y
`detail[512]` de Propagation) y datos de peor caso: nombre de banda de 31
caracteres, **entidad DXCC de 60** + grilla 6 + rumbo 359 + 20015 km para
`"Toward …"`, los **8 motivos de abort** por `"Error: %s"`, `INT_MIN`/`INT_MAX`,
`UINT_MAX`, `999` para `%03u`, escalera completa. Cada caso exige `ret < size` y
`strlen == ret` (nunca "salió truncado"):

```
PASS: 0 failure(s)
```

## 5. Tests (comandos exactos y resultados reales)

Entorno: host Linux, `uv` del repo, venv `tools/ab_testing/.venv` sincronizado.

### 5.1 Los 4 comandos del paso `Python framework` del CI (`ab-testing.yml`)

`working-directory: tools/ab_testing`

| Comando | Resultado | Exit |
| --- | --- | --- |
| `uv sync` | `Installed 11 packages`, venv creado (CPython 3.14.2) | **0** |
| `uv run ruff check .` | `All checks passed!` | **0** |
| `uv run ruff format --check .` | `200 files already formatted` | **0** |
| `uv run ty check src tests` | `All checks passed!` (emite además `WARN No python files found under the given path(s)`, comportamiento preexistente del comando en esta versión de `ty`, no del cambio: el diff no toca Python) | **0** |
| `uv run pytest -m "not e2e"` | **`233 passed, 13 deselected in 2.50s`** | **0** |

> Se usó el comando **exacto** del job `functional` (`pytest -m "not e2e"`), **no**
> `--quick`: `--quick` deselecciona 13 tests más, entre ellos `test_natural.py`
> (la trampa de la evidencia natural, §6).

### 5.2 Tests nativos (host) — el paso `Native (host) unit tests` del CI

```
$ sh tests/native/run_native_tests.sh
exit 0 · 53.076 comprobaciones, 24 resúmenes de etapa, todos "0 fallos"
```

Etapas de las apps del contrato:

```
== SWR Scan (#85/#119/#125): 461 comprobaciones, 0 fallos
== SWR Scan (#125) 3/5 pasadas con el backend fake: 4851 comprobaciones, 0 fallos
```

`tests/native/test_swrscan.c` compila la **fuente real** `src/dialog_swrscan.c`
(con stubs) y es donde viven los 4 pines actualizados (readout, stopped, timeout y
el `strstr` del motivo del gate); `test_swrscan_multipass.c` tiene el quinto.

### 5.3 Suite completa del repo (paso 4 del contrato)

```
$ sh tests/run_all_tests.sh --quick
22 OK, 0 fallos
(omitidos: qemu_apps wspr sstv wefax js8 webserver)
```

Incluye `wcag_rendered: OK` (contraste renderizado, compila el arnés y renderiza 24
fixtures), `curve_evidence: OK` (8 pruebas de la curva de ROE) y
`host_ci_parity: OK` (57 comprobaciones con `lvgl/` oculto, el árbol del CI).
`qemu_apps` se omite por falta del banco privado QEMU en este entorno (no es un
pase: `SKIP`).

### 5.4 Checks estáticos del job `functional` (todos exit 0)

```
check_cfg_fills check_actions check_settings_focus check_settings_grid
check_apps_menu check_wcag check_autotune_v2 check_build_identity
check_dialog_coverage
```

## 6. Trampa de la evidencia "natural" — verificada, no asumida

`tests/test_natural.py` fija `flushed_pixels == 1100352` y el artefacto
comprometido `tools/ab_testing/reports/NATURAL-ECOFINAL/*.json`. **No se regeneró
nada** y los 5 tests pasan (`5 passed`), porque el cambio **no puede** mover ese
número:

- los dos fixtures del banco natural (`main-natural`, `main-natural-eco`) declaran
  `screen main` — renderizan la **home**, no las apps;
- de los tres archivos tocados, `dialog_field.c` y `dialog_propagation.c` **no se
  compilan** en el arnés de host (son no-op en `mock_main_screen.c`) y
  `dialog_swrscan.c` sólo se renderiza en los fixtures `swrscan-*`.

Es decir: el artefacto comprometido sigue siendo válido para este cambio. En el
rebase de la Fase 2 hay que **volver a correr los 4 comandos**: si #285 ya movió el
render de la home, su artefacto/`SETTLE_PX` vienen actualizados desde `main`, y
cualquier desalineación tiene que aparecer como rojo ahí, no acá.

## 7. Límites de esta fase

- **No hay capturas "después"**: son la Fase 2 (§8). Ninguna captura de esta
  carpeta es posterior al cambio.
- **No hay build de imagen ARM completo** (falta el sysroot completo en este
  entorno, §4.1). Sí hay compilación ARM real de las tres unidades.
- **No hay radio, RF, TX ni periféricos**: no se valida nada físico.
- **Legibilidad física en el LCD de 4"**: `PENDING` (requiere radio).
- **No se recorrieron los flujos en vivo** de SWRScan/Propagation (barrido real,
  beacons, red): el comportamiento es el mismo por construcción (sólo cambian
  literales), pero la evidencia visual y funcional en QEMU es de la Fase 2.
- **Warning preexistente `%.0f` de `fill_direction`** (línea 538): no es de este
  cambio ni se apaga acá (§4.1). El peor caso real se mide en `truncation_check.c`.

## 8. Fase 2 (después del rebase sobre #285 y #286) — PENDIENTE

Al momento de cerrar esta fase, `origin/main` ya tiene **#285** (`107c1536`,
home: reloj/batería/BT) y **#286** (`styles.c` / `band_plan.c`) **no** está
integrado. El coordinador avisa cuando `main` tenga los dos; ahí se rebasa
`feat/ui-polish-app-copy-284` y **recién entonces** se captura la evidencia
antes/después 800×480 (superficie visual ya aprobada), en esta misma carpeta:

| App | Capturas |
| --- | --- |
| **Field** | idle/cuerpo con el header visible (`X6200  FIELD OPERATIONS`), ambos estados de softkeys |
| **SWRScan** | idle/estado vacío (`No measurements`), scanning (`Scanning pass X/Y`), completed, stopped (`Stopped (partial)`), error (motivos de abort: `RF timeout`, `no TX`, `state change`, `3/5 passes require RF validation`, `span/center outside the TX band`, `invalid parameters`, `no data`) |
| **Propagation** | idle/paso 1 (`Step 1/4  Preparing`), sync (`Step 2/4  Syncing`, `Beacon on air`), measure (`Step 3/4  Measuring`), result (`Step 4/4  Result`, `Measured …`, veredictos **`OPEN`/`MARGINAL`/`CLOSED`** y `NO IBP BEACONS`, línea de dirección `Toward …`), listening (`Listening to … Stop/Close restores VFO`), stop/cancelado (`Step 4/4  Cancelled`, `Radio left RX: …`) |

En esa pasada hay que mirar: etiquetas de softkeys, notas de estado, truncación
(`LV_LABEL_LONG_DOT`) y solapamiento, contraste, y que navegación/acciones no
cambien. Los textos que crecieron respecto del español son los que hay que mirar
con lupa en 800×480: `Band %s  |  %d eligible IBP beacons  |  %d scheduled`,
`3/5 passes require RF validation`, `span/center outside the TX band` y
`Toward %s (%s): %s %d deg, %.0f km.`. Lo que dependa de RF/periféricos ausentes se
marca `PARTIAL`/`BLOCKED`, nunca `PASS` fabricado.

## 9. Contenido de esta carpeta

```
README.md              este documento
run.sh                 reproduce §4.1-§4.3 (placeholders + truncación + compilación ARM)
verify_placeholders.py §4.2
truncation_check.c     §4.3
output.txt             salida real de run.sh
warning_diff.txt       multiconjunto de warnings base vs head (flags de firmware)
```
