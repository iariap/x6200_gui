# #371 — Preparación instrumental física del audio S/P y contrato MUX/restore (sin conmutación)

**Fecha de la corrida:** 2026-10-30 (host America/Argentina/Buenos_Aires, UTC−3). **Sin conexión física,
sin contacto con la radio, sin TX/PTT/ATU, sin apertura de chasis, sin MUX, sin compras, sin firmware ni UI.**
**Issue:** [#371](https://github.com/iariap/x6200_gui/issues/371) · **Épico:** [#362](https://github.com/iariap/x6200_gui/issues/362)
· **Antecedentes:** [#363](https://github.com/iariap/x6200_gui/issues/363)/[PR #366](https://github.com/iariap/x6200_gui/pull/366)
(`main`), [#367](https://github.com/iariap/x6200_gui/issues/367)/[PR DRAFT #368](https://github.com/iariap/x6200_gui/pull/368) (sin merge),
[#369](https://github.com/iariap/x6200_gui/issues/369)/[PR #370](https://github.com/iariap/x6200_gui/pull/370) (`main`).
**Base `main`:** declarada al delegar `cb9ea287548534ac5c4438c786b57deb436c7c40`; **verificada al empezar**
con `git fetch origin main` → `origin/main = 84bf4c40e51e240ebbe77847a54f886825073a5b` (ya incluye `#366`
`ead65675` y `#370` `84bf4c40`). El worktree se **rebasó a `84bf4c40`** antes de escribir; `docs/HALLAZGOS.md`
(§77 de #366 y §78 de #370) queda **intacto**.
**Worktree/rama:** `x6200-rx-physical-instrumentation` / `audit/rx-physical-instrumentation`.
**PR documental:** pendiente de publicar al cierre de esta evidencia (URL en `commands.log`); sólo
documentación, sin merge automático.

**Este directorio no conecta nada ni prueba la radio.** Es el plan físico ejecutable, honesto y verificable
para una ventana presencial **posterior al 2026-11-12**, cuando el propietario esté con el equipo. El
entregable es documentación; cualquier medición real futura va en su propia evidencia, no acá.

## Decisión

**Estado: `PREP_READY / PHYSICAL_BLOCKED`.** El protocolo, el pinout, el preflight y el handoff están
listos; **falta instrumentación declarada** y **falta verdad independiente del estado del MUX**. Por eso:

| Gate | Estado | Resumen |
|---|---|---|
| **P0** — medida BASE pasiva por S/P | **`P0_PENDING_EQUIPMENT`** | No hay ningún instrumento de captura/medida confirmado (ni cable TRS verificado, ni interfaz LINE aislada, ni atenuador, ni osciloscopio/sonda, ni medidor de continuidad). Sin eso no se conecta nada. |
| **P1** — independencia de MUX y restore | **`P1_BLOCKED_NO_SAFE_RESTORE`** | Sin readback real del registro 12 ni sonda externa no invasiva del MUX, no hay fuente de verdad física; el único restore conocido es el reinicio de la GUI (≳4 s) y no existe guardián independiente ≤250 ms verificado en radio. **No se autoriza conmutar el MUX.** |
| **P2** — verificación DAC→S/P | **`P2_NOT_EXECUTED`** | Depende de P1 PASS; corresponde al retest físico de #363. No se ejecuta ni se improvisa acá. |
| **P3** — router Linux al parlante | **`P3_OUT_OF_SCOPE`** | Fuera de alcance de este issue; #364/#365 siguen **bloqueados**. |

Flujo físico ordenado (desde 2026-11-12): [`operator-handoff.md`](operator-handoff.md) → sólo P0 pasiva si
el equipamiento se confirma; si falta algo, **STOP** documentado (`P0_PENDING_EQUIPMENT`) sin conectar.
Las condiciones de interrupción y el rol del operador están en [`preflight.md`](preflight.md).

## Dependencia de #363 (no se desbloquea con este PR)

Este trabajo **no** acredita nada de #363 ni de #362:

- **#363 / #366** cerró `GATE_A = BLOCKED_SAFE_MUX`: no hay readback real del registro 12, no hay dueño
  serializado fuera de la GUI y no hay restauración física probada. A4 (prueba física) quedó
  `NOT_EXECUTED / PHYSICAL_OUTPUT_NOT_VERIFIED`.
- **#369 / #370** cerró `R_INCONCLUSIVE`: la única lectura controlada devolvió `00 00 00 00` en
  `0x0038` (rxvol conocido = 1) y `0x0030`, así que la vía ensayada **no acredita readback**. No se repite
  el sondeo.
- **#367 / #368 (DRAFT)** tiene máscara/guardián seguros bajo mocks (p99 host 103,3 ms) pero
  `A_BLOCKED_READBACK`: **no hay restore físico verificado** y el guardián no debe habilitarse.
- **#364/#365 siguen bloqueados por #363 `GATE_A=PASS`**, que no existe. Este PR no declara PASS física, no
  desbloquea esas fases y no modifica firmware/UI.

Reabrir el gate exige una **fuente de verdad independiente** (readback real, sonda no invasiva del estado
del MUX o documentación OEM verificable que especifique una ventana de lectura) **y** un restore físico
medido en radio. Está detallado en [`safety-conclusion.md`](safety-conclusion.md); no se hace acá.

## Qué se hizo (sólo documentación)

1. Lectura completa del issue #371 y del estado integrado de #363/#369 (evidencia ya en `main`).
2. Verificación del **manual Xiegu X6200 Rev.05** (PDF público, `sha256` registrado en `commands.log`):
   conector S/P de 3,5 mm TRS con **AUDIO (tip), NC (ring), GND (sleeve)**, modo `EAR PHONE`/`SPEAKER`,
   advertencia de cortocircuito/silencio con plug **mono (1 contacto)** y advertencia de daño auditivo en
   modo SPEAKER. El dibujo se contrastó contra el PDF (p. 8 impresa, PDF p. 12) y se cita; **no se
   verificó contra el ejemplar físico** (eso es tarea del handoff).
3. Lectura de la documentación **Mestre** (externa, otro equipo) como **hipótesis**, nunca como evidencia
   de nuestro ejemplar ni como permiso de escritura I²C.
4. Inventario de equipamiento con **disponibilidad real declarada** (presente / no disponible / no
   declarado) a partir de la evidencia versionada y de una inspección **estática** de este host:
   [`equipment.md`](equipment.md).
5. Pinout y reglas de conexión sin voltajes inventados: [`wiring.md`](wiring.md) + [`croquis.md`](croquis.md).
6. Preflight con límites/aislamiento, rol de operador, backup/rollback y condiciones de interrupción:
   [`preflight.md`](preflight.md).
7. Hoja de captura **vacía** con exactamente las columnas del issue: [`measurements-template.csv`](measurements-template.csv).
8. Separación estricta de claims: [`safety-conclusion.md`](safety-conclusion.md).
9. Handoff de una página desde 2026-11-12: [`operator-handoff.md`](operator-handoff.md).
10. Comandos de preparación y verificación estática con exit reales: [`commands.log`](commands.log).

## Qué NO se hizo (límites duros respetados)

- **Cero** conexiones físicas, cables, sondas o instrumentos; **cero** contacto con la radio (ni remoto).
- **Cero** TX/PTT/ATU, **cero** conmutación de MUX, **cero** escrituras de registros.
- **Cero** apertura de chasis, soldadura o sonda interna; **cero** compras o pedidos de hardware.
- **Cero** cambios de firmware, ROM, GUI, UI o ruteo de audio; **cero** despliegues.
- **Cero** issues nuevos; **sin** cambios en `docs/HALLAZGOS.md`; **sin** reescribir §77/§78.
- **Cero** audio de terceros, binarios ni mediciones inventadas: `measurements-template.csv` está vacía y
  no se publica ninguna grabación.

## Disponibilidad real de equipamiento (resumen)

Ver detalle y fuentes en [`equipment.md`](equipment.md). En una línea: **este PR no pudo confirmar ningún
instrumento de medida**. Están documentados/presentes la radio y el host del proyecto, pero **no** hay
evidencia de cable TRS de 3 contactos, breakout/medidor de continuidad, interfaz de audio externa con
entrada LINE, aislamiento galvánico, atenuador pasivo, osciloscopio/sonda ni sonda de estado del MUX.
El host de trabajo (ThinkPad P14s Gen5 AMD, Fedora) tiene codecs HDA internos y **ninguna interfaz USB de
audio** conectada (`lsusb`, 2026-10-30); su jack combo **no** es una entrada LINE aislada con límites
declarados y no se usa como instrumento. Consecuencia: **P0 = `P0_PENDING_EQUIPMENT`** y **P1 =
`P1_BLOCKED_NO_SAFE_RESTORE`** (`NO_SAFE_MUX_STATE_TRUTH`). No se asume que el owner tenga nada: se
declara como **no disponible hasta confirmación en la ventana del 12/11**. El agente no compra hardware.

## Contrato de evidencia en una línea

> Escuchar o capturar la salida **S/P** demuestra **audio resultante**, no que el MCU devuelva el estado
> del registro 12, ni que un guardián pueda **restaurar** el MUX tras un fallo. Por eso
> `AUDIO_OUTPUT_VERIFIED`, `MUX_SELECT_PHYSICALLY_VERIFIED` y `RESTORE_VERIFIED` son claims separados:
> hoy los tres son **NO** (ver [`safety-conclusion.md`](safety-conclusion.md)).

## Archivos entregados

| Archivo | Contenido |
|---|---|
| `README.md` | Este resumen, decisión y dependencia de #363. |
| [`equipment.md`](equipment.md) | Equipo mínimo y disponibilidad REAL (presente / no disponible), límites conocidos y lo que falta. |
| [`wiring.md`](wiring.md) | S/P 3,5 mm TRS del manual (AUDIO/NC/GND), reglas de conexión, sin voltajes inventados. |
| [`croquis.md`](croquis.md) | Croquis estático en Markdown del cable y de la cadena de medida (con bloques NO DISPONIBLES). |
| [`preflight.md`](preflight.md) | Chequeos antes de conectar, límites/aislamiento, rol del operador, backup/rollback, `tx=false`, condiciones de interrupción. |
| [`measurements-template.csv`](measurements-template.csv) | Plantilla con exactamente las columnas del issue, **sin filas** (no hubo medición). |
| [`safety-conclusion.md`](safety-conclusion.md) | `AUDIO_OUTPUT_VERIFIED` / `MUX_SELECT_PHYSICALLY_VERIFIED` / `RESTORE_VERIFIED` separados; `P1_BLOCKED_NO_SAFE_RESTORE`; no se autoriza MUX. |
| [`operator-handoff.md`](operator-handoff.md) | Handoff presencial de una página, secuencia desde 2026-11-12, qué registrar y cuándo abortar. |
| [`commands.log`](commands.log) | Comandos de preparación/verificación estática con exit reales; sin secretos ni IPs. |

## Limitaciones declaradas (no se ocultan)

1. **No hay instrumentación confirmada.** La disponibilidad se declara a partir de evidencia del repo y de
   una inspección estática del host; **no hubo confirmación del owner** en esta corrida. Todo lo no
   documentado se trata como **no disponible**.
2. **No hay medición alguna**: no se midió el nivel/impedancia/offset de S/P, no se abrió el cable ni se
   verificó continuidad, no se capturó audio. Sin dato no se dimensiona un atenuador.
3. **El manual verificado es el PDF público Rev.05**; el handoff exige contrastarlo con la revisión física
   antes de insertar cualquier cable.
4. **Mestre es documentación externa de otro equipo**; se usa como hipótesis (MUX `voice_rec`, PA por
   `rxvol>0`, reloj `pll-audio`), no como evidencia de nuestro ejemplar ni como receta de escritura I²C.
5. **No hay fuente de verdad independiente del MUX** identificada: sin readback real ni sonda externa, P1
   queda bloqueado; no se propone abrir el equipo.
6. **El audio de RX de terceros no se publica**; si en la ventana se captura audio, sólo se versionan
   metadatos, estadísticas y hashes, salvo material propio/licenciado.

## Validación estática y ASF

Ver [`commands.log`](commands.log): chequeo de que la plantilla tiene las 21 columnas y **0 filas**, de que
no existe bandera `PASS` en P1/P2/P3, de que la ruta TRS respeta AUDIO/NC/GND y de que no hay binarios ni
audio en el directorio; `git diff --check`; y la matriz local de x6200 y `risk.py` con la base real del PR.
Resultado: **sólo documentación, riesgo low, sin FAIL**. Ningún check sustituye una medición física.
