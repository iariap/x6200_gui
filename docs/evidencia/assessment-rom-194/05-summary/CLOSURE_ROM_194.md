# Cierre de la épica #194 — verificación del checklist e integración del trunk

> **Alcance de este archivo:** artefacto de **cierre de épica** (no de la unidad #199). Se agrega
> **después** de la congelación de #199 y de la integración del trunk en `assessment/rom-194`.
> A propósito **no** modifica `SHA256SUMS`, `index.json`, `artifact-manifest.json` ni
> `acceptance-manifest.json` de #199: esos manifiestos sellados siguen siendo válidos para los
> archivos de esa unidad (un archivo nuevo no listado no rompe `sha256sum -c`). Queda declarado en
> [§12](#12-inconsistencias-y-decisiones-declaradas).

- **Campaña:** `iariap/x6200_gui#194` — evidencia pre-release de la ROM completa (sólo evidencia; sin
  cambios de firmware).
- **Candidato medido (congelado):** GUI `c888308eede71205605c06383432ee7e165f202e`, Buildroot
  `d084e2870a00b64d9c57474e7eceeef80f43f25b`, LVGL `74d0a816440eea53e030c4f1af842a94f7ce3d3`.
- **Rama de integración:** `assessment/rom-194`. **HEAD verificado en esta corrida:** `f78c6c71`.
- **Resultado global propuesto:** **CONDITIONAL** (alcance software) con **NO_GO de release** para el
  candidato congelado. No es GO pleno: hardware/energía pendiente + P1 abiertos + tip STALE.

---

## 1. Integración del trunk (estado del merge)

Se integró `origin/lu2dsx/x6200-rom` (trunk) en `assessment/rom-194`.

| Ref | SHA |
|---|---|
| HEAD de integración antes del merge | `e4473b0e` |
| Trunk `origin/lu2dsx/x6200-rom` | `37ec3b54` |
| Merge-base | `d4fb57f8` |
| Commit de merge | `f78c6c71` |
| Divergencia (ahead/behind) | integración +14 / trunk +27 |

- **Conflictos: 1, sólo documental** (`docs/PRUEBAS.md`). Ningún conflicto en `src/**`, `tests/**`,
  `tools/**`, `rootfs/**`, contratos, budgets ni golden. El trunk se integró sin resolución de
  producción.
- **Naturaleza del conflicto:** ambos lados habían agregado la **misma fila** del assessment (contexto
  común, sin conflicto); el trunk además insertó, en el mismo punto, la fila
  `22/10 ~11:48 · ROM fresca 458b1d06…`. El lado `HEAD` no aportaba nada en ese punto.
- **Resolución (conserva ambas partes):** se mantuvo la fila del trunk; el lado `HEAD` estaba vacío,
  por lo que no se perdió información de ninguno.

  Fragmento del conflicto original y su resolución:

  ```
  | 22/10 (UTC) | … ASSESSMENT #194 … |              <- fila común (sin conflicto)
  <<<<<<< HEAD
  =======
  | 22/10 ~11:48 | **ROM fresca 458b1d06…** (GUI 00a653e7 + BR f0ff620a) flasheada en la SD | `dadb1705` | …
  >>>>>>> origin/lu2dsx/x6200-rom
  | 22/10 ~10:12 | **ROM 03c6f9e6… (sin logo de kernel)** en la radio | `2a6059b5` | …
  ```
  → se eliminaron los marcadores y se conservó la fila `~11:48` del trunk.

- Se leyeron las tres etapas del merge (`:1` base, `:2` ours, `:3` theirs) para confirmar que la fila
  del assessment era idéntica en ambos lados y que la única diferencia era la inserción del trunk.

## 2. Hallazgo de cierre: el trunk YA contenía el dossier completo

Verificado por contenido (no por mensaje de commit ni por la descripción del PR):

- `git diff --name-only origin/lu2dsx/x6200-rom HEAD` → **vacío**: tras el merge, la rama de
  integración es **byte-idéntica** al trunk.
- `sha256sum` de `findings.csv`, `ASSESSMENT_ROM_194.md` y `PRUEBAS.md` → **idénticos** en trunk y HEAD.
- El trunk ya tenía `docs/evidencia/assessment-rom-194/05-summary/hardware-remote/` (fase B) y los 1420
  archivos del dossier, con commits **lineales** (mismos mensajes que la integración:
  `443d480e/3793aa83/07ee0808/a383b191/76f3f10e/a262e4f0/252741d9/c44a1203`). No hubo un merge commit
  del dossier en el trunk.

**Consecuencia (importante para el maintainer):** el PR final **#210** (`assessment/rom-194 →
lu2dsx/x6200-rom`) queda **sin contenido diferencial** una vez integrado el trunk: el dossier ya está en
`lu2dsx/x6200-rom`. Este archivo de cierre es, por diseño, lo único que aportaría un nuevo commit a la
rama.

## 3. Tabla de hijos (issue / PR / commit / fecha / resultado real)

| # | Issue | PR | Commit del informe | Merge | Fecha merge (UTC) | Archivos | SHA256SUMS | Resultado real |
|---|---|---|---|---|---|---|---|---|
| 1 | [#195](https://github.com/iariap/x6200_gui/issues/195) | [#201](https://github.com/iariap/x6200_gui/pull/201) | `be6088d1` | `230b2063` | 2026-10-21 20:58:54Z | 81 | 80 OK | ✅ entregable aceptado (issue aún OPEN) |
| 2 | [#196](https://github.com/iariap/x6200_gui/issues/196) | [#207](https://github.com/iariap/x6200_gui/pull/207) | `afa6109b` | `ca1698d1` | 2026-10-21 22:51:51Z | 103 | 102 OK | ✅ entregable aceptado (issue aún OPEN) |
| 3 | [#197](https://github.com/iariap/x6200_gui/issues/197) | [#203](https://github.com/iariap/x6200_gui/pull/203) | `f82dbadc` | `c41f36b2` | 2026-10-21 22:52:51Z | 816 | 815 OK | ✅ entregable aceptado (issue aún OPEN) |
| 4 | [#198](https://github.com/iariap/x6200_gui/issues/198) | [#208](https://github.com/iariap/x6200_gui/pull/208) | `2986d04d` | `05020942` | 2026-10-22 00:06:43Z | 273 | 272 OK | ✅ entregable aceptado (issue aún OPEN) |
| 5 | [#199](https://github.com/iariap/x6200_gui/issues/199) | [#209](https://github.com/iariap/x6200_gui/pull/209) | `f26fda7f` (+`89c9f8ff`,`2777913e`,`e4473b0e`) | `8074d297` | 2026-10-21 21:20:02Z | 147 | 146 OK | ✅ entregable aceptado (issue aún OPEN) |
| — | [#194](https://github.com/iariap/x6200_gui/issues/194) | [#210](https://github.com/iariap/x6200_gui/pull/210) | `f78c6c71` (merge de integración) | — | — | 1 (este archivo) | — | ⏳ PR final OPEN/DIRTY; sin contenido diferencial |

Estados verificados con `gh` en esta corrida: PRs #201/#203/#207/#208/#209 = **MERGED**; PR #210 =
**OPEN/DIRTY**; issues #195–#199 = **OPEN**.

## 4. Verificación del inventario de entregables (checklist de cierre)

Verificado contra el **contenido real de la rama integrada**, no contra la descripción de los PRs.

| Ítem del inventario | ¿Existe? | Evidencia |
|---|---|---|
| `01-baseline/`: coverage.csv, manifiesto, identidad, baseline/CI, logs/omisiones | ✅ | `01-baseline/` = 81 archivos; `coverage.csv` 334 filas; `artifact-manifest.json` con candidato/SHA GUI/lib/imagen (`sdcard.img d0ff4435`, `rootfs.tar 7c26c76b`, GUI md5 `1f998c46`, fuente-md5 `964193ae`); README §6 baseline host (17 OK/0 fallos/5 SKIP), §11 limitaciones |
| `02-rom/`: layout/rootfs/deps/DATA/update/rollback | ✅ | `02-rom/README.md` §3–§6 (ROM-01..ROM-04); 103 archivos |
| `02-rom/`: barrido 25 entradas sin `off`, alias FT4, negativos, 1h idle/100 ciclos o bloqueos | ✅ con hueco visible | barrido **BLOCKED** con causa exacta (`banco-apps` ocupado, §7.1); 100 ciclos PASS (§7.3); 1 h idle PASS (§7.4); negativas §8 |
| `03-ui-ux/`: atlas con huecos, contratos/inspección, seis temas, tareas, límites de mocks | ✅ | `03-ui-ux/` 816 archivos; `atlas.md`; `inspection/{audit,contracts}`; seis temas (README §5); `operator-tasks.csv` 25 filas; límites §11 |
| `04-performance/`: percentiles/n/dispersión/proxies/A-A, natural/idle/off/ECO, recursos/IO/audio, límites | ✅ | `04-performance/` 273 archivos; `capturas/benchmark-per-scenario.csv`; `benchmark-aa*`; §5 natural/wakeups/ECO/display-off; §6 recursos/IO; `audio/` (6 WAV voz/silencio); §10 límites |
| `05-summary/` + `docs/ASSESSMENT_ROM_194.md` + registro en `docs/PRUEBAS.md` | ✅ | `05-summary/` 147 archivos (`index.json`, `coverage-final.csv` 334, `findings.csv` 28, `hardware-plan.csv` 14); `docs/ASSESSMENT_ROM_194.md` existe (tracked); `docs/PRUEBAS.md` fila L39 + sección L1344 |
| Auditoría `python3 tests/audita-pruebas.py` | ✅ exit 0 | esta corrida: `OK: estructura y evidencia en orden` (`builds 237 · secciones 14 · enlaces 59`) |
| SHA256 revisados | ✅ | `sha256sum -c SHA256SUMS` → **exit 0** en 01-baseline/02-rom/03-ui-ux/04-performance/05-summary |
| Tests no ejecutados declarados | ✅ | README §3/§4 (SKIP/BLOCKED), `findings.csv` F-13 (sin CI del SHA), `hardware-plan.csv` |
| Revalidación final de ramas + comparación de producción (STALE tip nuevo) | ✅ declarado | ver [§8](#8-revalidación-final-de-ramas-y-stale-del-tip-nuevo) |

**Enlaces locales:** `scripts/check_links.py` → **115 enlaces OK** (exit 0).

## 5. Procedencia común

- **SHA del candidato** `c888308e` es ancestro de la rama de integración (verificado); el diff de
  producción candidato→integración es **0 archivos** (la integración sólo agregó `docs/`).
- **Identidad fuente/binario/libs/imagen** coherente y revalidada: `fuente-md5 964193ae` (checkout =
  staged = embebido), GUI `1f998c46` (13.407.624 B) extraído de `rootfs.tar`, `sdcard.img`
  `d0ff4435`, gate #86 **PASS**. Coincide entre #195/#196/#198 y #199.
- **Correspondencia medición↔SHA:** las mediciones 01..04 pertenecen al candidato congelado; no se
  rebasearon a documentación nueva ni se les atribuyó otro SHA.
- **Fase B (hardware remoto RX)** se ejecutó sobre el **tip instalado** (GUI `3b22a03a`), **no** sobre
  el candidato; declarado en `05-summary/hardware-remote/` y en el dossier.

## 6. Cobertura exhaustiva incluidos los huecos

- 334 ítems únicos (`coverage-final.csv`): REAL 151 · PARTIAL 138 · SYNTHETIC 4 · UNSUPPORTED 41.
- Estados consolidados: PASS 174 · REVIEW_REQUIRED 76 · **PENDING_HARDWARE 62** · **BLOCKED 13** ·
  FAIL 6 · UNSUPPORTED 3.
- Huecos **visibles y enlazados** (no tapados): barridos de apps BLOCKED (`02-rom` §7.1), 22 diálogos
  inertes F-09, webserver SKIP, CTest ft8/qth BLOCKED, 5 suites SKIP en imagen minimal, sin CI del SHA
  (F-13), y HW-01..HW-08 `PENDING_HARDWARE` (`hardware-plan.csv`).
- Ningún SKIP/exit 77 se cuenta como PASS; ausencia de instrumento = `null` con motivo.

## 7. Separación hecho/hipótesis y referencias válidas

- `findings.csv`: **18 fundados** (F-01..F-18) + **10 hipótesis separadas** (H-01..H-10). Las
  hipótesis no se declaran hallazgos.
- Referencias: PRs #201/#203/#207/#208/#209 MERGED, PR #210 OPEN/DIRTY, issues #194–#199 OPEN
  (verificados con `gh`). `check_links.py` OK (115 enlaces locales).

## 8. Revalidación final de ramas y STALE del tip nuevo

| Ref | Declarado por #199 | Verificado en esta corrida |
|---|---|---|
| GUI tip | `d4fb57f8` (25 archivos de producción vs candidato) | **`37ec3b54`** → **166 archivos de producción** vs candidato |
| Buildroot tip | `f23d14f` | **`e5a63930`** (`git ls-remote iariap/AetherX6200Buildroot`) |

La divergencia de producción del candidato contra el tip **creció de 25 a 166 archivos**: el dossier
sigue siendo válido **sólo** para `c888308e` + `d084e287` + el artefacto del par congelado, y queda
**STALE** para el tip nuevo (señalado aquí; no se rebaseó ninguna medición). Adoptar el tip exige
repetir build/identidad, layout/MBR y DATA primer boot, init/prune, arranque real, estrés/reposo,
update/rollback, UI/contratos, performance de boot/handoff y **toda** la fase de hardware.

## 9. Resultado global propuesto

**CONDITIONAL** (alcance software) / **NO_GO de release** para el candidato congelado.

Fundamento (regla del #199/#194):
- **No es GO:** (1) hardware/energía **no** ejecutados (HW-01/03/05/06/08 son P0/P1) — con hardware
  pendiente no corresponde GO pleno; (2) **P1 candidatos abiertos** F-01..F-04 (DATA vfat/primer boot,
  SIGSEGV de DB corrupta/RO/faltante, update exit 1 + owner 1000, update sin checksum/rollback) sin
  disposición del owner; (3) el tip GUI/Buildroot está **STALE** (166 archivos).
- **No es BLOCKED global:** la evidencia software 01..04 está integrada, es auditable y tiene
  procedencia común; el bloqueo es de la fase física, no de la recolección documental.
- **Distinción por capa (exigida por el Epic):** software host (medido) · software ARM en QEMU
  (medido, sin kernel/init real) · hardware operado remotamente (HR-01..HR-06 **PASS** sobre el **tip**)
  · validación física/instrumental (HW-01..HW-08 **PENDING_HARDWARE**).

**Nota de alcance:** este resultado evalúa el **candidato** `c888308e`. El trunk actual (`37ec3b54`)
incluye la ola de hardening (#211/#213/#214/#215/#216) que **no** forma parte del candidato; algunos
P1 del candidato tienen contraparte en el trunk (p. ej. `#225` SQLite fail-closed, `#226` supervisor
con backoff), pero **su re-verificación no está en el alcance de esta campaña**.

## 10. Pendientes físicos enlazados

- `05-summary/hardware-plan.csv` (14 casos: HW-01..HW-08 + HR-01..HR-06) y `05-summary/hardware-plan.md`.
- `05-summary/hardware-remote/` (HR-01..HR-06 ejecutados sobre el tip, 6/6 PASS; no sobre el candidato).
- `05-summary/findings.csv` (F-* con `existing_issue`) y `05-summary/stale.md`.
- Registro en `docs/PRUEBAS.md` (fila L39 + sección L1344) con hardware visible.
- **HW-04 TX** sigue separado y requiere autorización específica + instrumentos; no se extrapola
  `tx:true` a RF.

## 11. Qué falta para poder cerrar la épica

1. **PR final #210 (revisión/merge del maintainer).** Tras integrar el trunk queda **sin contenido
   diferencial** (el dossier ya está en `lu2dsx/x6200-rom`). El orquestador debe decidir: (a) pushear
   `assessment/rom-194` con este merge + este archivo de cierre y llevar ese PR a revisión/merge; o
   (b) **cerrar #210 como ya integrado** (el contenido está en el trunk). No se mergea sin revisión.
2. **Cerrar los hijos #195–#199**: sus entregables ya están aceptados y mergeados; sólo falta la
   decisión de cierre del orquestador/owner.
3. **Cerrar #194** después del punto 1–2, dejando hardware/energía **pendiente y visible**.
4. **Decisión sobre el tip** `37ec3b54` / Buildroot `e5a63930`: el dossier es del candidato y STALE para
   el tip; no se requiere resolver para cerrar la épica, pero debe quedar declarado.

No falta ningún entregable del inventario: todos existen y son coherentes (ver §4).

## 12. Inconsistencias y decisiones declaradas

Reportadas, **no “arregladas”** en la evidencia de otro hijo:

1. **`docs/ASSESSMENT_ROM_194.md` (raíz) ya existe y es la versión canónica** (10.703 B, con §8.1 de
   fase B y la ubicación correcta), pero:
   - `05-summary/ASSESSMENT_ROM_194.md` (10.148 B) es una **copia anterior** (dice “listo para
     moverse a esa ruta”; sin §8.1). El contenido raíz ya no está “diferido”.
   - `05-summary/README.md` §10 y `05-summary/acceptance-manifest.json` (`scope.deferred_by_orchestrator`)
     siguen declarando ambos documentos raíz como **diferidos**. Quedaron **obsoletos** tras el commit
     `89c9f8ff`/`2777913e`. Decisión: **no** se editaron (son artefactos sellados de #199) para no
     romper su `SHA256SUMS`.
2. **Conteo “40 archivos” de #199** en el cuerpo del PR #210: la carpeta real `05-summary/` tiene
   **147 archivos** (146 en `SHA256SUMS`). El “40” probablemente refleja el estado del PR #209 antes de
   la fase B. No se editó el cuerpo del PR.
3. **`provenance-audit.md` §5 se auto-describe desactualizado**: declara `builds 235 · enlaces 50`; en
   esta corrida `audita-pruebas.py` da `237 · 59`. Es efecto del avance del trunk (`docs/PRUEBAS.md`
   con filas nuevas), no un error del assessment.
4. **STALE ampliado**: el dossier declara el tip `d4fb57f8` (25 archivos); el tip real es `37ec3b54`
   (**166 archivos**) y Buildroot `e5a63930` (no `f23d14f`). Señalado en §8; no se rebasearon mediciones.
5. **Este archivo** (`CLOSURE_ROM_194.md`) es un artefacto de cierre de **épica** agregado después de
   la congelación de #199: a propósito **no** se agregó a `SHA256SUMS`, `index.json` ni
   `acceptance-manifest.json` de #199 (evitar reescribir el sellado de otra unidad). `sha256sum -c`
   sigue verificando los 146 archivos de #199.

## 13. Verificación ejecutada en esta corrida

| Comando | Resultado |
|---|---|
| `git merge origin/lu2dsx/x6200-rom` | 1 conflicto (`docs/PRUEBAS.md`), resuelto conservando ambas partes |
| `git diff --name-only origin/lu2dsx/x6200-rom HEAD` | vacío (rama == trunk en contenido) |
| `sha256sum -c SHA256SUMS` (01..05) | exit 0 en las cinco carpetas |
| `python3 tests/audita-pruebas.py` | exit 0 — `OK: estructura y evidencia en orden` |
| `python3 05-summary/scripts/check_links.py` | exit 0 — 115 enlaces |
| `gh pr view 210` / `gh issue view 195..199` | #210 OPEN/DIRTY; #195–#199 OPEN |
| `git diff --name-only c888308e origin/lu2dsx/x6200-rom \| grep -v '^docs/' \| wc -l` | 166 |
| `git ls-remote iariap/AetherX6200Buildroot lu2dsx/x6200-rom` | `e5a63930…` |

No se ejecutaron pruebas funcionales nuevas: el cierre sólo hace validación documental y de Git.
