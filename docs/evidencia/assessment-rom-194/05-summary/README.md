# Dossier consolidado — assessment pre-release ROM X6200 (#194 / unidad 5 de 5, #199)

- **Campaña:** `iariap/x6200_gui#194` — evidencia pre-release de la ROM completa.
- **Unidad:** #199 (5/5), **fase A: consolidación documental sin radio**.
- **Candidato medido:** GUI `c888308eede71205605c06383432ee7e165f202e`; Buildroot
  `d084e2870a00b64d9c57474e7eceeef80f43f25b`; LVGL `74d0a816440eea53e030c4f1af842a94f7ce3d3`.
- **Artefacto:** `sdcard.img` `d0ff4435…`, `rootfs.tar` `7c26c76b…`, GUI md5 `1f998c46…`,
  `fuente-md5` `964193ae…`, gate #86 PASS. Detalle en [`artifact-manifest.json`](artifact-manifest.json).
- **Base del PR:** `assessment/rom-194` = `05020942` (merge de #208), con #201/#207/#203/#208
  integrados. **Este PR no mergea ni aprueba release.**
- **Fase de hardware:** **BLOCKED / PENDING_HARDWARE** — no se contactó la radio en esta fase.
  Causa exacta en [§7](#7-fase-de-hardware-blockedpending_hardware-y-causa-exacta).
- **Recomendación:** **CONDITIONAL** para el alcance software; **NO_GO de release** mientras la
  fase B no se ejecute y los P1 candidatos no tengan disposición del owner. Ver [§9](#9-recomendación).

---

## 1. Índice del dossier

| Archivo | Contenido |
|---|---|
| [`README.md`](README.md) | este índice, criterios, cobertura, aceptación, hallazgos, STALE y fase B |
| [`index.json`](index.json) | índice máquina-legible con estado, hashes y conteos |
| [`coverage-final.csv`](coverage-final.csv) | 334 ítems únicos del inventario #195 con veredicto consolidado y evidencia |
| [`coverage.md`](coverage.md) | metodología de unión 01-04, tablas y huecos (sin doble conteo) |
| [`findings.csv`](findings.csv) | 18 hallazgos + 10 hipótesis separadas, con prioridad/certeza/estado |
| [`findings.md`](findings.md) | detalle de hallazgos fundados, hipótesis y criterio de "fundado" |
| [`stale.md`](stale.md) | declaración formal de validez y repeticiones necesarias |
| [`hardware-plan.csv`](hardware-plan.csv) | HW-01..HW-08 + HR-01..HR-06, criterios PASS/FAIL/BLOCKED |
| [`hardware-plan.md`](hardware-plan.md) | runbook de fase B listo para el orquestador |
| [`hardware-remote/`](hardware-remote/README.md) | fase remota RX: 6 casos, todos BLOCKED en fase A, con causa |
| [`provenance-audit.md`](provenance-audit.md) | auditoría de hashes, merges, ramas, diff y SHA256SUMS |
| [`history.md`](history.md) | antecedentes #86…#191, PRs y estado consultado |
| [`acceptance-manifest.json`](acceptance-manifest.json) | aceptación por unidad, PRs/merges, hashes y conteos |
| [`artifact-manifest.json`](artifact-manifest.json) | identidad del candidato/artefacto y límites de capa |
| [`ASSESSMENT_ROM_194.md`](ASSESSMENT_ROM_194.md) | documento pedido por #199 (contenido listo; ubicación raíz diferida, ver §10) |
| [`PRUEBAS-row.md`](PRUEBAS-row.md) | fila/sección pedida para `docs/PRUEBAS.md` (contenido listo; ubicación raíz diferida, ver §10) |
| [`commands.jsonl`](commands.jsonl) | comandos de verificación de esta unidad, UTC, exit y log |
| [`logs/`](logs/) | logs crudos de las verificaciones (git, hashes, gate #86, auditoría) |
| [`SHA256SUMS`](SHA256SUMS) | hash de todos los archivos de esta carpeta |

No se copiaron logs de 01..04: se enlazan por ruta relativa durable y su `SHA256SUMS` propio.

---

## 2. Cómo se compone la evidencia 01..04

La campaña se ejecutó en secuencia estricta y cada unidad entregó su carpeta + manifiesto +
`results.csv` + logs + `SHA256SUMS`, revisada y mergeada por el orquestador.

| Unidad | Issue | PR | Commit del informe | Merge en `assessment/rom-194` | Archivos | SHA256SUMS | Estado |
|---|---|---|---:|---|---:|---:|---|
| 1/5 baseline | #195 | **#201** | `be6088d1` | `230b2063` (2026-10-21T20:58:54Z) | 81 | 80 OK | ✅ aceptada |
| 2/5 ROM/confiabilidad | #196 | **#207** | `afa6109b` | `ca1698d1` (2026-10-21T22:51:51Z) | 103 | 102 OK | ✅ aceptada |
| 3/5 UI/UX | #197 | **#203** | `f82dbadc` | `c41f36b2` (2026-10-21T22:52:51Z) | 816 | 815 OK | ✅ aceptada |
| 4/5 performance | #198 | **#208** | `2986d04d` | `05020942` (2026-10-22T00:06:43Z) | 273 | 272 OK | ✅ aceptada |
| 5/5 dossier | #199 | este PR | ver commit del PR | — | — | — | 📄 fase A |

Verificación: `sha256sum -c SHA256SUMS` en las cuatro carpetas → **exit 0**; conteos de archivos
81/103/816/273; el diff de cada merge sólo toca su carpeta. Detalle reproducible en
[`provenance-audit.md`](provenance-audit.md) y [`logs/provenance/`](logs/provenance/).

**Corrección documental:** #198 escribió "26 archivos de producción" para el tip `d4fb57f8`; el
recuento reproducible de este dossier da **25** (`git diff --name-only c888308e d4fb57f8 | grep -v '^docs/'`).
Se conserva el informe de #198 y se corrige el número acá, sin editarlo.

**Orden de merges:** `ca1698d1` (#196) entró **antes** que `c41f36b2` (#197), aunque el README de
#197 se escribió cuando #196 aún no estaba integrado. Por eso la fila `DEP-196` de #197 quedó
**superada**; el hueco real que queda es la falta de captura de los 22 diálogos inertes, no la
dependencia. Se declara, no se borra evidencia.

---

## 3. Criterio de estados y cobertura

### 3.1 Estados (contrato #194/#199)

| Estado | Significado |
|---|---|
| **PASS** | El contrato aplicable se cumplió con evidencia ejecutada en la capa declarada. Un PASS HOST no prueba hardware. |
| **FAIL** | Se demostró un incumplimiento del contrato en la capa declarada (con repro y evidencia). |
| **BLOCKED** | La evidencia requerida no pudo producirse por una dependencia externa exacta (banco ocupado, instrumento, autorización, radio). No es PASS ni FAIL del producto. |
| **INCONCLUSIVE** | Hay medición, pero no alcanza para decidir (ruido, fixture incompleto, detector ausente). |
| **PENDING_HARDWARE** | El caso requiere hardware/instrumento y **no se ejecutó**. |
| **REVIEW_REQUIRED** | Observación de código/UX que necesita decisión humana o carece de test 1:1; **no** es un fallo demostrado. |
| **UNSUPPORTED** | No hay implementación/instrumento en el alcance; ausencia declarada. |
| **SKIP** | Runner que se omitió por dependencia (exit 77 / imagen minimal / volumen ajeno). Nunca se cuenta como PASS; en `coverage-final.csv` se pliega a BLOCKED. |
| **HYPOTHESIS** | Solo en `findings.csv`: explicación plausible **no fundada**. No es hallazgo. |

### 3.2 Cobertura

| Etiqueta | Significado |
|---|---|
| **REAL** | Se ejecutó el artefacto/código real en la capa declarada. |
| **PARTIAL** | Se ejecutó una parte o un sustituto con límites explícitos. |
| **SYNTHETIC** | Fixture/mock/null-sink; el camino real no se ejercitó. |
| **UNSUPPORTED** | No hay instrumento/implementación; no se generaliza desde otra capa. |

Capas: **HOST**, **QEMU_USER**, **HARDWARE**. Un resultado de otra SHA se conserva como
HISTORICAL y no rellena cobertura del candidato. No se suman "tests pasados" de campañas
distintas ni se calcula un porcentaje único de ROM.

---

## 4. Cobertura consolidada (unión 01-04, sin doble conteo)

**Inventario único:** 334 ítems de `01-baseline/coverage.csv` (apps, acciones, Settings, páginas,
entradas físicas, pantallas, subsistemas, servicios, paquetes, config, CI y 114 pruebas). #197
materializa **142 de esos mismos ítems** en `03-ui-ux/ui-coverage.csv`; #196/#198/#199 aportan
**métricas/resultados**, no ítems nuevos. Por eso la unión son **334**, no 334+142.

| Corte | Distribución |
|---|---|
| Cobertura del inventario | **REAL 151 · PARTIAL 138 · SYNTHETIC 4 · UNSUPPORTED 41** |
| Dueño de medición | #197 142 · #195 102 · #196 51 · #199 21 · #198 18 |
| Estado consolidado | **PASS 174 · REVIEW_REQUIRED 76 · PENDING_HARDWARE 62 · BLOCKED 13 · FAIL 6 · UNSUPPORTED 3** |
| Pruebas (114) | PASS 94 · REVIEW_REQUIRED 17 · BLOCKED 3 — **PASS de inventario/asignación**, no de ejecución individual: la ejecución real es por suite (17 OK, 0 fallos, 5 SKIP; códecs en imagen full; 138 checks nativos). |

| Tipo | Total | PASS | REVIEW | PENDING_HW | BLOCKED | FAIL | UNSUP |
|---|---:|---:|---:|---:|---:|---:|---:|
| action | 42 | 40 | 0 | 0 | 0 | 0 | 2 |
| app | 26 | 10 | 8 | 5 | 3 | 0 | 0 |
| build_config | 4 | 2 | 1 | 0 | 0 | 1 | 0 |
| build_package | 7 | 1 | 0 | 3 | 3 | 0 | 0 |
| buttons_page | 16 | 0 | 0 | 16 | 0 | 0 | 0 |
| ci_environment | 6 | 5 | 1 | 0 | 0 | 0 | 0 |
| physical_input | 10 | 0 | 0 | 10 | 0 | 0 | 0 |
| screen | 3 | 0 | 3 | 0 | 0 | 0 | 0 |
| service | 18 | 0 | 1 | 14 | 1 | 2 | 0 |
| settings_group | 8 | 8 | 0 | 0 | 0 | 0 | 0 |
| settings_item | 31 | 0 | 31 | 0 | 0 | 0 | 0 |
| subsystem | 49 | 14 | 14 | 14 | 3 | 3 | 1 |
| test | 114 | 94 | 17 | 0 | 3 | 0 | 0 |

**Filas de resultados (métricas, no ítems):** 1.025 en total — #195 84, #196 86, #197 256,
#198 599. Estados: **PASS 933 · INCONCLUSIVE 24 · REVIEW_REQUIRED 21 · FAIL 15 · BLOCKED 13 ·
PENDING_HARDWARE 12 · SKIP 5 · UNSUPPORTED 2**. Capas: HOST 839 · QEMU_USER 170 · HARDWARE 11 ·
sin capa 5. No se suman como "tests" ni se usa el total como score de ROM.

**Huecos principales** (todos declarados, ninguno contado como PASS):
- 41 ítems `UNSUPPORTED` de cobertura (hardware, webserver, periféricos, apps sin instrumento).
- 62 ítems `PENDING_HARDWARE` (19 ya marcados en #195 + 43 adjudicados por #197/#199).
- 22 diálogos inertes del harness (F-09), webserver SKIP, CTest ft8/qth BLOCKED, 3 barridos de
  apps BLOCKED por `banco-apps` ocupado, 5 suites SKIP en imagen minimal (repetidas en full).
- No hay corrida de CI del commit de merge `c888308e` (F-13).

El detalle fila por fila, con evidencia y limitación, está en [`coverage-final.csv`](coverage-final.csv)
y la metodología en [`coverage.md`](coverage.md).

---

## 5. Hallazgos fundados (resumen)

Criterio: un hallazgo es **fundado** si tiene repro, evidencia ejecutada o camino de código
determinista citado, expected/observed e impacto. Lo que no se puede fundar va a **hipótesis**
(H-01..H-10) y no se declara hallazgo. **No se abrió ningún ticket.**

| ID | P | Hallazgo | Estado | Evidencia principal |
|---|---|---|---|---|
| F-01 | P1 | Tarjeta nueva: DATA nace vfat y el primer boot no monta `/mnt` | FAIL | [02-rom §5.2](../02-rom/README.md#52-lo-que-realmente-pasa-evidencia-ejecutada), `logs/rom03/` |
| F-02 | P1 | DB corrupta/RO/faltante → SIGSEGV determinista + crash-loop | FAIL | [02-rom §8.2](../02-rom/README.md#82-db-corrupta--read-only--faltante-fail-crash-determinista) |
| F-03 | P1 | Empaquetador update exit 1 (setuid 3 vs 5) y S02update owner 1000 (pulse) | FAIL | [02-rom §6](../02-rom/README.md#6-rom-04--update-y-rollback) |
| F-04 | P1 | Update sin checksum/rollback; primera update backup 706/5466 | FAIL | [02-rom §6](../02-rom/README.md#6-rom-04--update-y-rollback) |
| F-05 | P1 | Sin watchdog y daemon `while true` sin backoff (34 inicios/10 s) | REVIEW | [02-rom §9](../02-rom/README.md#9-hallazgos-sin-abrir-tickets) |
| F-06 | P2 | Timer WiFi 30 ms incondicional: ~41,8 wakeups/s | REVIEW | [04-performance §8](../04-performance/README.md#8-perf-06--perfil-estatico-de-costes-y-riesgos-host-codigo) |
| F-07 | P2 | CW decoder sin señal ~9 % de un core (5,4× home) en QEMU | REVIEW | [04-performance §9](../04-performance/README.md#9-hallazgos-principales-sin-abrir-tickets) |
| F-08 | P2 | `radio_thread` ~100 % sin BASE, ~630k read/s (QEMU) | REVIEW | [04-performance §9](../04-performance/README.md#9-hallazgos-principales-sin-abrir-tickets) |
| F-09 | P3 | 22 diálogos inertes en harness: hueco de cobertura UI | REVIEW | [03-ui-ux §2](../03-ui-ux/README.md#2-dependencia-no-satisfecha--196-blocked-explicito) |
| F-10 | P3 | Mensajes ES/EN mezclados en GPS/WiFi/Status/Propagation | REVIEW | [03-ui-ux §8](../03-ui-ux/README.md#8-ux-03--apps-errores-y-alcance-real) |
| F-11 | P2 | Defconfig congelado sin audio: **resuelto en el repo** por Buildroot PR #2 | RESOLVED | [02-rom §1.2](../02-rom/README.md#12-reproducibilidad-del-par-congelado-defconfig) |
| F-12 | P2 | Migración 2 no idempotente si se pierde `version` | REVIEW | [02-rom §5.3](../02-rom/README.md#53-base-de-datos-por-defecto) |
| F-13 | P3 | Sin corrida CI del commit `c888308e` (solo referencia PR #193) | REVIEW | [01-baseline §3](../01-baseline/README.md#3-paso-1--estado-refs-toolchain-y-ci) |
| F-14 | P3 | Conteos/documentación inconsistentes (55/469, 401, 138); About/DFL | REVIEW | [01-baseline §5](../01-baseline/README.md#5-paso-3--revision-de-documentacion-fixtures-y-pruebas) |
| F-15 | P3 | `/bin/sh`→bash; init scripts con arrays/`[[ ]]` no corren en ash | REVIEW | [02-rom §4.2](../02-rom/README.md#42-sintaxis-con-el-interprete-real-de-la-imagen-pass-2626) |
| F-16 | P2 | Render/A-A INCONCLUSIVE por ruido del host (−33…+54 %) | INCONCLUSIVE | [04-performance §3-4](../04-performance/README.md#3-perf-01--baseline-de-render-host-199-fixtures) |
| F-17 | P3 | Webserver/CTest/barridos/suites omitidos con causa | BLOCKED | [01-baseline §6](../01-baseline/README.md#6-paso-4--baseline-host-una-vez-y-complementos) |
| F-18 | P2 | Imagen sin p3: DATA depende de procedimiento externo no versionado | REVIEW | [02-rom §3.1/§5.2](../02-rom/README.md#31-layout-y-arranque) |

**No fundados (hipótesis, separadas):** H-01 crash de DB en hardware; H-02 boot-loop sin
watchdog; H-03 `radio_thread` al 100 % en hardware; H-04 impacto energético del timer WiFi;
H-05 p3 corta al flashear el candidato; H-06 Auto Tune con FFT no confiable; H-07 error de
operador por ES/EN; H-08 boot/consumo del tip #206; H-09 mixer/audio analógico; H-10 pérdida de
datos del primer boot al actualizar. Detalle y por qué no se fundan en [`findings.md`](findings.md).

---

## 6. STALE y validez

**Declaración formal:** este dossier es válido **solo** para GUI `c888308e`, Buildroot
`d084e287`, LVGL `74d0a816` y el artefacto con los SHA256 de [`artifact-manifest.json`](artifact-manifest.json).
No es válido para el tip actual ni para el ROM del orquestador.

- **GUI tip:** `d4fb57f8` (incluye #200 boot animation, #202 limpieza u-boot, #206 bootanim v2
  y commits de QA). `git diff --name-only c888308e d4fb57f8 | grep -v '^docs/'` = **25 archivos
  de producción**: `src/bootanim/*`, `src/boot_splash.*`, `src/main.c`, `src/CMakeLists.txt`,
  `CMakeLists.txt`, `rootfs/etc/init.d/S01bootanim`, `tests/bootanim/*`,
  `tests/check_bootanim.sh`, `.github/workflows/{main,boot-rootfs}.yml` y
  `u-boot/u-boot-sunxi-with-spl.bin` (borrado).
- **Buildroot tip:** `f23d14f` (PR #2 defconfig audio + PR #3 sin logo de kernel), posterior al
  `d084e287` congelado. El artefacto medido se construyó con `d084e287` + override local; **no**
  se reconstruyó con el tip.
- **Consecuencia:** cualquier conclusión de release para `d4fb57f8`/`f23d14f` requiere repetir
  build/identidad, layout/MBR y DATA, prune/init, arranque real, estrés/reposo, update,
  UI/contratos, performance de boot/handoff y **toda** la fase de hardware. Lista completa en
  [`stale.md`](stale.md).
- Revalidar ambos tips al iniciar la fase B: pueden haber avanzado otra vez.

---

## 7. Fase de hardware: BLOCKED/PENDING_HARDWARE y causa exacta

**No se contactó la radio en esta fase.** No hubo SSH, socket, captura de framebuffer, RF,
power-cycle ni cambio de SD/DATA. La fase A es documental por directiva del orquestador; la
fase B la coordina el owner.

- **Estado:** **BLOCKED** para la ejecución remota RX (HR-01..HR-06, 6/6 casos) y
  **PENDING_HARDWARE** para el protocolo físico completo (HW-01..HW-08).
- **Causa exacta:** la radio no responde ahora / no debe contactarse en fase A; no hay sesión
  exclusiva verificada, ni identidad del binario instalado, ni snapshot inicial. Sin esos
  prerrequisitos no se puede distinguir el software instalado del candidato ni garantizar que no
  se interfiere con el owner.
- **Dato externo (no revalidado):** la orquestación registra que la radio tenía un build del
  **tip** (GUI md5 `3b22a03a…`, 13.412.248 B), no el candidato (`1f998c46…`). Si eso sigue así,
  la fase B producirá evidencia del software instalado, **no del candidato**. No se autoriza
  desplegar el candidato desde este PR.
- **No es un fallo del candidato ni una omisión:** es una dependencia externa de hardware,
  autorización y ventana. No se declara PASS ni se inventan resultados.

Los 14 casos (8 HW + 6 HR), con precondiciones, instrumento, pasos, medición, expected, abort,
restore y evidencia, están en [`hardware-plan.csv`](hardware-plan.csv).

---

## 8. Plan de fase B (resumen)

Runbook completo y comandos concretos: [`hardware-plan.md`](hardware-plan.md).

**Prerrequisitos que debe dar el owner/orquestador:**
1. Autorización escrita de la fase B y **ventana exclusiva** (sin otro agente/DeepSeek).
2. Confirmación de que se puede leer la radio y, si se quiere evaluar el candidato, una SD de
   prueba con el artefacto exacto **o** autorización explícita de despliegue (este PR no la da).
3. Backup restaurable de DATA/params y procedimiento de restore probado.
4. Instrumentos: fuente/medidor V/I/P, carga fantasma + wattmeter/analizador (solo HW-04),
   generador/analizador de audio, cámara para input-to-feedback, SDs de prueba.

**Secuencia remota RX (si hay ventana):** HR-01 identidad/exclusividad → HR-02 baseline y captura
→ HR-03 navegación (Status/Settings/Bands/Eq/DFLEditor, 20 s) → HR-04 QSY +100/−100 Hz ×20 →
HR-05 60 min RX con recursos nativos → HR-06 restauración y home. Ante TX inesperada, caída,
boot_id distinto, filesystem ro o intervención externa: **abortar y conservar evidencia**.

**Secuencia física completa:** HW-01 identidad/boot (5 fríos + 5 ordenados) → HW-02 controles/UI
→ HW-03 audio/DSP/RX/CW → HW-05 energía/térmica → HW-06 persistencia/DB/update con corte →
HW-07 conectividad → HW-08 8 h RX. **HW-04 TX** va separado, solo con autorización específica,
carga fantasma e instrumentos; no se extrapola `tx:true` a RF.

**Criterio de aceptación:** PASS solo con el contrato cumplido y evidencia de la capa correcta;
FAIL con repro; BLOCKED con causa. Un corte de energía, update o formateo **solo** en medios de
prueba, nunca en la SD/DATA del owner.

---

## 9. Recomendación

**CONDITIONAL** para el alcance software; **NO_GO de release** en el estado actual.

- **No es GO:** la fase de hardware no se ejecutó (HW-01/03/05/06/08 son P0/P1) y hay P1
  candidatos abiertos (F-01..F-05) sin disposición del owner.
- **No es BLOCKED global:** la evidencia software 01..04 está integrada, es auditable y tiene
  procedencia común; el bloqueo es de la fase física, no de la recolección documental.
- **Condiciones para pasar a GO:** (1) fase B ejecutada con identidad del artefacto exacto;
  (2) F-01..F-05 resueltos o aceptados formalmente por el owner; (3) decisión sobre el tip
  `d4fb57f8`/Buildroot `f23d14f` y repetición de la cobertura STALE; (4) revisión/merge del PR
  final por el maintainer.
- **Este dossier no concede permiso de deployment, flash ni TX.**

---

## 10. Limitaciones de esta unidad

1. **Sin radio:** todo lo físico es BLOCKED/PENDING_HARDWARE, con causa; no se inventó evidencia.
2. **No se repitieron mediciones** de 01..04; se consolidó lo aceptado. Las correcciones
   documentales se declaran (p. ej. 26→25 archivos del tip).
3. **La adjudicación de `coverage-final.csv` es documental**, no una nueva corrida: cada fila
   cita la unidad y el veredicto que esa unidad dejó; no se infiere PASS donde no lo hay.
4. **#197 se ejecutó antes del merge de #196**: su `DEP-196` quedó superado, pero los 22 diálogos
   inertes siguen sin captura.
5. **El defconfig congelado** no reproduce el artefacto desde cero; el fix está en Buildroot
   PR #2 (`f23d14f`), fuera del par medido. No se reconstruyó el artefacto.
6. **No hay CI del SHA candidato**; la referencia es el head del PR #193.
7. **La identidad de la radio actual** (tip `3b22a03a…`) es un dato externo de orquestación, no
   revalidado en fase A.
8. **Ubicación de documentos raíz:** el issue #199 también pide `docs/ASSESSMENT_ROM_194.md` y una
   fila en `docs/PRUEBAS.md`. Por instrucción explícita del orquestador, este PR commitea **solo
   `05-summary/`**; ambos contenidos quedan listos en [`ASSESSMENT_ROM_194.md`](ASSESSMENT_ROM_194.md)
   y [`PRUEBAS-row.md`](PRUEBAS-row.md) para que el PR de integración final (o una tarea
   autorizada) los mueva a su ruta. Es una **diferencia de alcance declarada**, no un olvido.

---

## 11. Verificación de esta unidad

- `sha256sum -c` de 01..04: **OK** (80/102/815/272).
- Artefacto: SHA256 de sdcard/rootfs/boot/zImage/dtb/u-boot y md5/sha256 del GUI dentro de
  `rootfs.tar`: **coinciden** con #195/#196/#198.
- `python3 tools/verifica-firmware-port.py --tar rootfs.tar`: **exit 0**, gate #86 PASS.
- `git merge-base --is-ancestor c888308e origin/assessment/rom-194`: **0**; diff de producción
  candidato→integración: **0 archivos**.
- `tests/audita-pruebas.py`: se ejecutó sobre el repo tal como está (sin modificar
  `docs/PRUEBAS.md`); resultado y limitación en [`logs/provenance/audita-pruebas.log`](logs/provenance/audita-pruebas.log)
  y [`provenance-audit.md`](provenance-audit.md).
- Enlaces locales y hashes: [`scripts/check_links.py`](scripts/check_links.py) + `SHA256SUMS`.
- Comandos exactos/UTC/exit: [`commands.jsonl`](commands.jsonl).

### 7.1 Fase B remota RX EJECUTADA (2026-10-22, sobre el tip)

Con autorización del owner se ejecutó **HR-01..HR-06** sobre la ROM **instalada = tip** (GUI `3b22a03a…`, fuente
`a576d48d…`, kernel 6.1.82): **6/6 PASS** (exclusividad, baseline, 8/8 apps, 20/20 QSY reversibles, 60 min RX estables
sin reinicios ni CRASHLOG y con RSS/fds/threads constantes, cierre/restauración). Sesión exclusiva, sin TX, sin flash.
Detalle y crudo en [`hardware-remote/`](hardware-remote/README.md). **HW-01..HW-08 siguen BLOCKED/PENDING_HARDWARE**
(instrumentos, TX autorizado, medios de prueba de update/DB, 8 h). La medición es del tip, no del candidato `1f998c46…`.
